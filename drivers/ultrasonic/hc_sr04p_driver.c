// drivers/ultrasonic/hc_sr04p_driver.c
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/ktime.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>
#include <linux/wait.h>
#include <linux/atomic.h>

#define DEVICE_NAME "hc_sr04p"
#define CLASS_NAME "ultrasonic"

// GPIO 핀 설정 (라즈베리파이 기준)
#define TRIGGER_PIN 523
#define ECHO_PIN 525

// 디바이스 데이터 구조
struct sensor_data {
    dev_t dev_number;
    struct class *dev_class;
    struct device *dev_device;
    struct cdev char_dev;
    
    // 센서 관련
    ktime_t pulse_start;
    ktime_t pulse_end;
    int distance_mm;
    int irq_number;
    
    // 동기화
    wait_queue_head_t wait_queue;
    atomic_t measurement_ready;
    struct mutex lock;
    
    // 상태 관리
    enum {
        SENSOR_IDLE,
        SENSOR_MEASURING
    } state;
    
    unsigned long last_trigger_time;
};

static struct sensor_data *sensor_dev;

// *** 추가: 디바이스 권한 자동 설정 함수 ***
static int hc_sr04p_dev_uevent(const struct device *dev, struct kobj_uevent_env *env)
{
    add_uevent_var(env, "DEVMODE=%#o", 0666);
    return 0;
}

// 인터럽트 핸들러 (ECHO 핀의 rising/falling edge)
static irqreturn_t echo_irq_handler(int irq, void *dev_id) {
    struct sensor_data *data = (struct sensor_data *)dev_id;
    
    if (gpio_get_value(ECHO_PIN)) {
        // Rising edge: 펄스 시작
        data->pulse_start = ktime_get();
        pr_debug("[HC-SR04P]: Pulse started\n");
    } else {
        // Falling edge: 펄스 끝
        data->pulse_end = ktime_get();
        
        // 거리 계산
        s64 pulse_duration_ns = ktime_to_ns(ktime_sub(data->pulse_end, data->pulse_start));
        int pulse_duration_us = (int)(pulse_duration_ns / 1000);
        
        // 유효성 검사 (20μs ~ 38ms: 3mm ~ 6.5m)
        if (pulse_duration_us >= 20 && pulse_duration_us <= 38000) {
            data->distance_mm = (pulse_duration_us * 10) / 58;  // mm 단위
            atomic_set(&data->measurement_ready, 1);
        } else {
            data->distance_mm = -1;  // 오류 표시
            atomic_set(&data->measurement_ready, -1);
        }
        
        data->state = SENSOR_IDLE;
        wake_up_interruptible(&data->wait_queue);
        
        pr_debug("[HC-SR04P]: Distance: %d mm (pulse: %d μs)\n", 
                data->distance_mm, pulse_duration_us);
    }
    
    return IRQ_HANDLED;
}

// 측정 트리거 함수
static int trigger_measurement(void) {
    unsigned long now = jiffies;
    
    // 최소 60ms 간격 보장 (센서 스펙)
    if (time_before(now, sensor_dev->last_trigger_time + msecs_to_jiffies(60))) {
        return -EBUSY;
    }
    
    if (sensor_dev->state != SENSOR_IDLE) {
        return -EBUSY;
    }
    
    sensor_dev->state = SENSOR_MEASURING;
    atomic_set(&sensor_dev->measurement_ready, 0);
    
    // 10μs 트리거 펄스
    gpio_set_value(TRIGGER_PIN, 1);
    udelay(15);  // 10μs 이상
    gpio_set_value(TRIGGER_PIN, 0);
    
    sensor_dev->last_trigger_time = now;
    
    return 0;
}

// 디바이스 읽기 함수
static ssize_t device_read(struct file *filp, char __user *buffer, size_t len, loff_t *offset) {
    char result[32];  // ✅ 수정: 배열로 제대로 선언
    int ret;
    size_t result_len;

    pr_info("[HC-SR04P]: Read request started\n");

    if (*offset > 0)
        return 0;  // EOF
    
    if (mutex_lock_interruptible(&sensor_dev->lock))
        return -ERESTARTSYS;

    // 상태 초기화
    sensor_dev->state = SENSOR_IDLE;
    atomic_set(&sensor_dev->measurement_ready, 0);

    // 새로운 측정 시작
    ret = trigger_measurement();
    if (ret) {
        pr_err("[HC-SR04P]: Trigger failed: %d\n", ret);
        mutex_unlock(&sensor_dev->lock);
        return ret;
    }
    
    mutex_unlock(&sensor_dev->lock);
    
    // 측정 완료 대기 (최대 2초)
    ret = wait_event_interruptible_timeout(
        sensor_dev->wait_queue,
        atomic_read(&sensor_dev->measurement_ready) != 0,
        msecs_to_jiffies(2000)
    );
    
    if (ret == 0) {
        pr_err("[HC-SR04P]: Measurement timeout\n");
        return -ETIMEDOUT;
    } else if (ret < 0) {
        return ret;
    }
    
    // 거리 데이터를 문자열로 변환
    if (atomic_read(&sensor_dev->measurement_ready) > 0) {
        result_len = snprintf(result, sizeof(result), "%d\n", sensor_dev->distance_mm);
    } else {
        result_len = snprintf(result, sizeof(result), "ERROR\n");
    }
    
    if (len < result_len)
        return -EINVAL;
    
    if (copy_to_user(buffer, result, result_len))  // ✅ 수정: result는 이제 배열
        return -EFAULT;
    
    *offset += result_len;    
    return result_len;
}

// 파일 오퍼레이션
static const struct file_operations fops = {
    .read = device_read,
};

// 모듈 초기화 (수정된 부분 - 권한 설정 추가)
static int __init hc_sr04p_init(void) {
    int ret;
    
    pr_info("[HC-SR04P]: Initializing ultrasonic sensor driver\n");
    
    // 메모리 할당
    sensor_dev = kzalloc(sizeof(struct sensor_data), GFP_KERNEL);
    if (!sensor_dev)
        return -ENOMEM;
    
    // 동기화 객체 초기화
    mutex_init(&sensor_dev->lock);
    init_waitqueue_head(&sensor_dev->wait_queue);
    atomic_set(&sensor_dev->measurement_ready, 0);
    sensor_dev->state = SENSOR_IDLE;
    sensor_dev->last_trigger_time = jiffies - msecs_to_jiffies(100);
    
    // GPIO 설정
    ret = gpio_request_one(TRIGGER_PIN, GPIOF_OUT_INIT_LOW, "HC-SR04P Trigger");
    if (ret) {
        pr_err("[HC-SR04P]: Cannot request trigger GPIO %d\n", TRIGGER_PIN);
        goto err_free_mem;
    }
    
    ret = gpio_request_one(ECHO_PIN, GPIOF_IN, "HC-SR04P Echo");
    if (ret) {
        pr_err("[HC-SR04P]: Cannot request echo GPIO %d\n", ECHO_PIN);
        goto err_free_trigger;
    }
    
    // 인터럽트 설정
    sensor_dev->irq_number = gpio_to_irq(ECHO_PIN);
    ret = request_irq(sensor_dev->irq_number, echo_irq_handler,
                     IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
                     "hc_sr04p_echo", sensor_dev);
    if (ret) {
        pr_err("[HC-SR04P]: Cannot request IRQ %d\n", sensor_dev->irq_number);
        goto err_free_echo;
    }
    
    // 문자 디바이스 등록
    ret = alloc_chrdev_region(&sensor_dev->dev_number, 0, 1, DEVICE_NAME);
    if (ret < 0) {
        pr_err("[HC-SR04P]: Cannot allocate major number\n");
        goto err_free_irq;
    }
    
    cdev_init(&sensor_dev->char_dev, &fops);
    ret = cdev_add(&sensor_dev->char_dev, sensor_dev->dev_number, 1);
    if (ret < 0) {
        pr_err("[HC-SR04P]: Cannot add device\n");
        goto err_unreg_chrdev;
    }
    
    // *** 수정: 클래스 생성 및 권한 설정 콜백 등록 ***
    sensor_dev->dev_class = class_create(CLASS_NAME);
    if (IS_ERR(sensor_dev->dev_class)) {
        pr_err("[HC-SR04P]: Cannot create class\n");
        ret = PTR_ERR(sensor_dev->dev_class);
        goto err_del_cdev;
    }
    
    // *** 추가: 권한 자동 설정을 위한 uevent 콜백 등록 ***
    sensor_dev->dev_class->dev_uevent = hc_sr04p_dev_uevent;
    
    // 디바이스 파일 생성
    sensor_dev->dev_device = device_create(
        sensor_dev->dev_class,
        NULL,
        sensor_dev->dev_number,
        NULL,
        DEVICE_NAME
    );
    
    if (IS_ERR(sensor_dev->dev_device)) {
        pr_err("[HC-SR04P]: Cannot create device\n");
        ret = PTR_ERR(sensor_dev->dev_device);
        goto err_destroy_class;
    }
    
    pr_info("[HC-SR04P]: Device registered successfully. Device: /dev/%s (auto-permission: 0666)\n", DEVICE_NAME);
    return 0;
    
    // 에러 처리
err_destroy_class:
    class_destroy(sensor_dev->dev_class);
err_del_cdev:
    cdev_del(&sensor_dev->char_dev);
err_unreg_chrdev:
    unregister_chrdev_region(sensor_dev->dev_number, 1);
err_free_irq:
    free_irq(sensor_dev->irq_number, sensor_dev);
err_free_echo:
    gpio_free(ECHO_PIN);
err_free_trigger:
    gpio_free(TRIGGER_PIN);
err_free_mem:
    kfree(sensor_dev);
    return ret;
}

// 모듈 해제
static void __exit hc_sr04p_exit(void) {
    pr_info("[HC-SR04P]: Exiting ultrasonic sensor driver\n");
    
    device_destroy(sensor_dev->dev_class, sensor_dev->dev_number);
    class_destroy(sensor_dev->dev_class);
    cdev_del(&sensor_dev->char_dev);
    unregister_chrdev_region(sensor_dev->dev_number, 1);
    free_irq(sensor_dev->irq_number, sensor_dev);
    gpio_free(ECHO_PIN);
    gpio_free(TRIGGER_PIN);
    kfree(sensor_dev);
}

module_init(hc_sr04p_init);
module_exit(hc_sr04p_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Veda");
MODULE_DESCRIPTION("HC-SR04P Ultrasonic Distance Sensor Driver for Raspberry Pi - Auto Permission");
MODULE_VERSION("1.0");

// i2c_lcd1602_driver.c
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/i2c.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/slab.h>    // kzalloc, kfree
#include <linux/ioctl.h>   // _IO, _IOW 매크로용


// 1602 LCD 전용 설정
#define DEVICE_NAME "lcd1602"
#define CLASS_NAME  "lcd"
#define I2C_BUS_AVAILABLE   1
#define SLAVE_DEVICE_NAME   "LCD1602"
#define LCD_SLAVE_ADDR      0x27

// 1602 LCD 크기 정의 (수정된 부분)
#define LCD_WIDTH    16
#define LCD_HEIGHT   2
#define LCD_MAX_CHARS (LCD_WIDTH * LCD_HEIGHT)

// 1602 LCD 라인 주소 (수정된 부분)
#define LCD_LINE1_ADDR  0x00
#define LCD_LINE2_ADDR  0x40

// LCD 명령어 정의
#define LCD_CLEAR_DISPLAY   0x01
#define LCD_RETURN_HOME     0x02
#define LCD_ENTRY_MODE_SET  0x04
#define LCD_DISPLAY_CONTROL 0x08
#define LCD_CURSOR_SHIFT    0x10
#define LCD_FUNCTION_SET    0x20
#define LCD_SET_CGRAM_ADDR  0x40
#define LCD_SET_DDRAM_ADDR  0x80

// PCF8574 핀 매핑
#define BACKLIGHT_ON  0x08
#define BACKLIGHT_OFF 0x00
#define ENABLE        0x04
#define READ_WRITE    0x02
#define REGISTER_SELECT 0x01

// 드라이버 상태 구조체 (추가된 부분)
struct lcd1602_data {
    struct i2c_client *client;
    struct mutex lock;
    int cursor_col;
    int cursor_row;
    bool backlight;
    bool display_on;
    bool cursor_on;
    bool blink_on;
};

static struct lcd1602_data *lcd_data;
static dev_t first;
static struct cdev c_dev;
static struct class *cl;

// I2C 보드 정보
static struct i2c_board_info lcd_i2c_board_info = {
    I2C_BOARD_INFO(SLAVE_DEVICE_NAME, LCD_SLAVE_ADDR)
};

// I2C 디바이스 ID
static const struct i2c_device_id lcd_i2c_id[] = {
    { SLAVE_DEVICE_NAME, 0 },
    { }
};
MODULE_DEVICE_TABLE(i2c, lcd_i2c_id);

// 4비트 모드로 데이터 전송
static int lcd_write_nibble(u8 data, u8 control)
{
    u8 buffer;
    int ret;
    
    buffer = (data & 0xF0) | control;
    if (lcd_data->backlight) buffer |= BACKLIGHT_ON;
    
    ret = i2c_master_send(lcd_data->client, &buffer, 1);
    if (ret != 1) return -EIO;
    
    buffer |= ENABLE;
    ret = i2c_master_send(lcd_data->client, &buffer, 1);
    if (ret != 1) return -EIO;
    udelay(1);
    
    buffer &= ~ENABLE;
    ret = i2c_master_send(lcd_data->client, &buffer, 1);
    if (ret != 1) return -EIO;
    udelay(50);
    
    return 0;
}

// LCD 명령어 전송
static int lcd_write_command(u8 cmd)
{
    int ret;
    
    ret = lcd_write_nibble(cmd, 0);
    if (ret) return ret;
    
    ret = lcd_write_nibble(cmd << 4, 0);
    if (ret) return ret;
    
    if (cmd == LCD_CLEAR_DISPLAY || cmd == LCD_RETURN_HOME) {
        mdelay(2);
        lcd_data->cursor_col = 0;
        lcd_data->cursor_row = 0;
    }
    
    return 0;
}

// LCD 데이터 전송
static int lcd_write_data(u8 data)
{
    int ret;
    
    ret = lcd_write_nibble(data, REGISTER_SELECT);
    if (ret) return ret;
    
    ret = lcd_write_nibble(data << 4, REGISTER_SELECT);
    if (ret) return ret;
    
    // 커서 위치 업데이트 (수정된 부분)
    lcd_data->cursor_col++;
    if (lcd_data->cursor_col >= LCD_WIDTH) {
        lcd_data->cursor_col = 0;
        lcd_data->cursor_row = (lcd_data->cursor_row + 1) % LCD_HEIGHT;
    }
    
    return 0;
}

// 커서 위치 설정 (추가된 함수)
static int lcd_set_cursor(int col, int row)
{
    u8 addr;
    
    if (col >= LCD_WIDTH || row >= LCD_HEIGHT)
        return -EINVAL;
    
    if (row == 0)
        addr = LCD_LINE1_ADDR + col;
    else
        addr = LCD_LINE2_ADDR + col;
    
    lcd_data->cursor_col = col;
    lcd_data->cursor_row = row;
    
    return lcd_write_command(LCD_SET_DDRAM_ADDR | addr);
}

// 1602 LCD 초기화 (수정된 부분)
static int lcd_init(void)
{
    int ret;
    
    mdelay(50);
    
    // 4비트 모드 설정 시퀀스
    ret = lcd_write_nibble(0x30, 0);
    if (ret) return ret;
    mdelay(5);
    
    ret = lcd_write_nibble(0x30, 0);
    if (ret) return ret;
    udelay(150);
    
    ret = lcd_write_nibble(0x30, 0);
    if (ret) return ret;
    udelay(150);
    
    ret = lcd_write_nibble(0x20, 0);
    if (ret) return ret;
    
    // 기능 설정: 4비트, 2라인, 5x8 도트
    ret = lcd_write_command(LCD_FUNCTION_SET | 0x08);
    if (ret) return ret;
    
    // 디스플레이 ON, 커서 OFF, 깜빡임 OFF
    ret = lcd_write_command(LCD_DISPLAY_CONTROL | 0x04);
    if (ret) return ret;
    
    // 화면 지우기
    ret = lcd_write_command(LCD_CLEAR_DISPLAY);
    if (ret) return ret;
    
    // 엔트리 모드: 오른쪽으로 이동, 시프트 OFF
    ret = lcd_write_command(LCD_ENTRY_MODE_SET | 0x02);
    if (ret) return ret;
    
    // 상태 초기화
    lcd_data->cursor_col = 0;
    lcd_data->cursor_row = 0;
    lcd_data->backlight = true;
    lcd_data->display_on = true;
    lcd_data->cursor_on = false;
    lcd_data->blink_on = false;
    
    return 0;
}

// 파일 연산 - write (개선된 버전)
static ssize_t lcd_write(struct file *file, const char __user *buf,
                        size_t len, loff_t *ppos)
{
    char kernel_buf[LCD_MAX_CHARS + 1];
    int i, ret;
    ssize_t written = 0;
    
    if (len > LCD_MAX_CHARS)
        len = LCD_MAX_CHARS;
    
    if (copy_from_user(kernel_buf, buf, len))
        return -EFAULT;
    
    mutex_lock(&lcd_data->lock);
    
    for (i = 0; i < len; i++) {
        switch (kernel_buf[i]) {
        case '\n':
            // 다음 줄로 이동
            if (lcd_data->cursor_row == 0) {
                ret = lcd_set_cursor(0, 1);
            } else {
                ret = lcd_set_cursor(0, 0);
            }
            break;
        case '\r':
            // 현재 줄의 처음으로 이동
            ret = lcd_set_cursor(0, lcd_data->cursor_row);
            break;
        case '\f':
            // 화면 지우기
            ret = lcd_write_command(LCD_CLEAR_DISPLAY);
            break;
        case '\b':
            // 백스페이스
            if (lcd_data->cursor_col > 0) {
                ret = lcd_set_cursor(lcd_data->cursor_col - 1, 
                                   lcd_data->cursor_row);
                if (!ret) ret = lcd_write_data(' ');
                if (!ret) ret = lcd_set_cursor(lcd_data->cursor_col - 1,
                                             lcd_data->cursor_row);
            }
            break;
        default:
            if (kernel_buf[i] >= 0x20 && kernel_buf[i] <= 0x7F) {
                ret = lcd_write_data(kernel_buf[i]);
            }
            break;
        }
        
        if (ret) {
            mutex_unlock(&lcd_data->lock);
            return ret;
        }
        written++;
    }
    
    mutex_unlock(&lcd_data->lock);
    return written;
}

// IOCTL 명령어 정의 (추가된 부분)
#define LCD_IOC_MAGIC  'L'
#define LCD_IOC_CLEAR       _IO(LCD_IOC_MAGIC, 1)
#define LCD_IOC_HOME        _IO(LCD_IOC_MAGIC, 2)
#define LCD_IOC_SETCURSOR   _IOW(LCD_IOC_MAGIC, 3, int[2])
#define LCD_IOC_BACKLIGHT   _IOW(LCD_IOC_MAGIC, 4, int)
#define LCD_IOC_DISPLAY     _IOW(LCD_IOC_MAGIC, 5, int)

// IOCTL 함수 (추가된 함수)
static long lcd_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    int ret = 0;
    int params[2];
    
    switch (cmd) {
    case LCD_IOC_CLEAR:
        mutex_lock(&lcd_data->lock);
        ret = lcd_write_command(LCD_CLEAR_DISPLAY);
        mutex_unlock(&lcd_data->lock);
        break;
        
    case LCD_IOC_HOME:
        mutex_lock(&lcd_data->lock);
        ret = lcd_write_command(LCD_RETURN_HOME);
        mutex_unlock(&lcd_data->lock);
        break;
        
    case LCD_IOC_SETCURSOR:
        if (copy_from_user(params, (int __user *)arg, sizeof(params)))
            return -EFAULT;
        mutex_lock(&lcd_data->lock);
        ret = lcd_set_cursor(params[0], params[1]);
        mutex_unlock(&lcd_data->lock);
        break;
        
    case LCD_IOC_BACKLIGHT:
        mutex_lock(&lcd_data->lock);
        lcd_data->backlight = !!arg;
        mutex_unlock(&lcd_data->lock);
        break;
        
    default:
        ret = -ENOTTY;
        break;
    }
    
    return ret;
}

// 파일 연산 구조체 (누락된 부분)
static struct file_operations fops = {
    .owner = THIS_MODULE,
    .write = lcd_write,
    .unlocked_ioctl = lcd_ioctl,
};

// I2C 드라이버 probe 함수 (누락된 부분)
static int lcd_i2c_probe(struct i2c_client *client)
{
    int ret;
    
    lcd_data->client = client;
    ret = lcd_init();
    if (ret) {
        pr_err("LCD initialization failed\n");
        return ret;
    }
    
    pr_info("LCD I2C Driver Probed\n");
    return 0;
}

// I2C 드라이버 remove 함수 (누락된 부분)
static void lcd_i2c_remove(struct i2c_client *client)
{
    lcd_write_command(LCD_CLEAR_DISPLAY);
    pr_info("LCD I2C Driver Removed\n");
}

// I2C 드라이버 구조체 (누락된 부분)
static struct i2c_driver lcd_i2c_driver = {
    .driver = {
        .name   = SLAVE_DEVICE_NAME,
        .owner  = THIS_MODULE,
    },
    .probe    = lcd_i2c_probe,
    .remove   = lcd_i2c_remove,
    .id_table = lcd_i2c_id,
};

// 모듈 초기화 함수 (누락된 부분)
static int __init lcd_driver_init(void)
{
    int ret;
    struct i2c_adapter *adapter;
    
    // 메모리 할당
    lcd_data = kzalloc(sizeof(struct lcd1602_data), GFP_KERNEL);
    if (!lcd_data) {
        return -ENOMEM;
    }
    
    mutex_init(&lcd_data->lock);
    
    // I2C 어댑터 가져오기
    adapter = i2c_get_adapter(I2C_BUS_AVAILABLE);
    if (!adapter) {
        pr_err("I2C Adapter not found\n");
        kfree(lcd_data);
        return -ENODEV;
    }
    
    // I2C 클라이언트 생성
    lcd_data->client = i2c_new_client_device(adapter, &lcd_i2c_board_info);
    if (IS_ERR(lcd_data->client)) {
        pr_err("Failed to create I2C client\n");
        i2c_put_adapter(adapter);
        kfree(lcd_data);
        return PTR_ERR(lcd_data->client);
    }
    
    i2c_put_adapter(adapter);
    
    // I2C 드라이버 등록
    ret = i2c_add_driver(&lcd_i2c_driver);
    if (ret < 0) {
        pr_err("Failed to register I2C driver\n");
        i2c_unregister_device(lcd_data->client);
        kfree(lcd_data);
        return ret;
    }
    
    // 캐릭터 디바이스 등록
    ret = alloc_chrdev_region(&first, 0, 1, DEVICE_NAME);
    if (ret < 0) {
        i2c_del_driver(&lcd_i2c_driver);
        i2c_unregister_device(lcd_data->client);
        kfree(lcd_data);
        return ret;
    }
    
    cl = class_create(CLASS_NAME);
    if (IS_ERR(cl)) {
        unregister_chrdev_region(first, 1);
        i2c_del_driver(&lcd_i2c_driver);
        i2c_unregister_device(lcd_data->client);
        kfree(lcd_data);
        return PTR_ERR(cl);
    }
    
    if (!device_create(cl, NULL, first, NULL, DEVICE_NAME)) {
        class_destroy(cl);
        unregister_chrdev_region(first, 1);
        i2c_del_driver(&lcd_i2c_driver);
        i2c_unregister_device(lcd_data->client);
        kfree(lcd_data);
        return -1;
    }
    
    cdev_init(&c_dev, &fops);
    ret = cdev_add(&c_dev, first, 1);
    if (ret == -1) {
        device_destroy(cl, first);
        class_destroy(cl);
        unregister_chrdev_region(first, 1);
        i2c_del_driver(&lcd_i2c_driver);
        i2c_unregister_device(lcd_data->client);
        kfree(lcd_data);
        return -1;
    }
    
    pr_info("I2C LCD1602 Driver Loaded Successfully\n");
    return 0;
}

// 모듈 해제 함수 (누락된 부분)
static void __exit lcd_driver_exit(void)
{
    i2c_unregister_device(lcd_data->client);
    i2c_del_driver(&lcd_i2c_driver);
    
    cdev_del(&c_dev);
    device_destroy(cl, first);
    class_destroy(cl);
    unregister_chrdev_region(first, 1);
    
    if (lcd_data) {
        mutex_destroy(&lcd_data->lock);
        kfree(lcd_data);
    }
    
    pr_info("I2C LCD1602 Driver Unloaded\n");
}

// 모듈 등록 매크로 (누락된 부분)
module_init(lcd_driver_init);
module_exit(lcd_driver_exit);

// 모듈 정보 매크로 (누락된 부분 - 필수!)
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Veda");
MODULE_DESCRIPTION("I2C LCD1602 Character Device Driver for Raspberry Pi");
MODULE_VERSION("1.0");

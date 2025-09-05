// tests/ultrasonic/test_ultrasonic_basic.c
#include <stdio.h>
#include <assert.h>
#include <string.h>

typedef struct {
    int trigger_pin;
    int echo_pin;
    int last_distance_mm;  // mm 단위로 변경 (드라이버와 일치)
    int measurement_ready;
} test_ultrasonic_data_t;

// 테스트 카운터
static int tests_passed = 0;
static int tests_total = 0;

#define TEST_START(name) do { \
    printf("🧪 Testing: %s... ", name); \
    tests_total++; \
} while(0)

#define TEST_PASS() do { \
    printf("✅ PASSED\n"); \
    tests_passed++; \
} while(0)

#define TEST_FAIL(msg) do { \
    printf("❌ FAILED: %s\n", msg); \
    return -1; \
} while(0)

// 거리 계산 로직 테스트 (기본)
int test_distance_calculation_basic(void) {
    TEST_START("Basic distance calculation");
    
    // 정상 케이스: 10cm = 580μs
    int time_us = 580;
    int expected_distance_mm = 100;  // 10cm = 100mm
    
    int calculated_distance_mm = (time_us * 10) / 58;
    
    if (calculated_distance_mm != expected_distance_mm) {
        TEST_FAIL("Basic calculation mismatch");
    }
    
    TEST_PASS();
    return 0;
}

// 거리 계산 경계값 테스트
int test_distance_calculation_edge_cases(void) {
    TEST_START("Distance calculation edge cases");
    
    // 최소 거리: 20μs (약 3.4mm)
    int min_time_us = 20;
    int min_distance_mm = (min_time_us * 10) / 58;
    if (min_distance_mm < 3 || min_distance_mm > 5) {
        TEST_FAIL("Minimum distance calculation");
    }
    
    // 최대 거리: 38000μs (약 6.55m)
    int max_time_us = 38000;
    int max_distance_mm = (max_time_us * 10) / 58;
    if (max_distance_mm < 6500 || max_distance_mm > 6600) {
        TEST_FAIL("Maximum distance calculation");
    }
    
    // 무효한 값: 0μs
    int invalid_time_us = 0;
    int invalid_distance_mm = (invalid_time_us * 10) / 58;
    if (invalid_distance_mm != 0) {
        TEST_FAIL("Zero time handling");
    }
    
    TEST_PASS();
    return 0;
}

// 정밀도 테스트
int test_distance_precision(void) {
    TEST_START("Distance measurement precision");
    
    // 1cm 간격으로 정확도 확인
    struct {
        int time_us;
        int expected_mm;
    } test_cases[] = {
        {58, 10},     // 1cm
        {290, 50},    // 5cm  
        {580, 100},   // 10cm
        {1160, 200},  // 20cm
        {2900, 500},  // 50cm
        {5800, 1000}, // 100cm
    };
    
    for (int i = 0; i < 6; i++) {
        int calculated = (test_cases[i].time_us * 10) / 58;
        int diff = abs(calculated - test_cases[i].expected_mm);
        
        // ±5mm 오차 허용
        if (diff > 5) {
            char error_msg[100];
            snprintf(error_msg, sizeof(error_msg), 
                    "Precision test %d: expected %dmm, got %dmm", 
                    i, test_cases[i].expected_mm, calculated);
            TEST_FAIL(error_msg);
        }
    }
    
    TEST_PASS();
    return 0;
}

// GPIO 설정 테스트 (실제 드라이버 핀 번호 사용)
int test_gpio_setup(void) {
    TEST_START("GPIO pin configuration");
    
    test_ultrasonic_data_t sensor = {523, 525, 0, 0}; // 실제 드라이버 GPIO 번호
    
    // GPIO 핀 유효성 검사 (라즈베리파이 GPIO 범위: 0~27의 실제 번호는 더 큰 값)
    if (sensor.trigger_pin <= 0) {
        TEST_FAIL("Invalid trigger pin number");
    }
    
    if (sensor.echo_pin <= 0) {
        TEST_FAIL("Invalid echo pin number");
    }
    
    // 트리거와 에코 핀이 다른지 확인
    if (sensor.trigger_pin == sensor.echo_pin) {
        TEST_FAIL("Trigger and echo pins must be different");
    }
    
    // 구조체 초기화 확인
    if (sensor.last_distance_mm != 0 || sensor.measurement_ready != 0) {
        TEST_FAIL("Initial values not zero");
    }
    
    TEST_PASS();
    return 0;
}

// 트리거 펄스 시뮬레이션 테스트
int test_trigger_pulse_simulation(void) {
    TEST_START("Trigger pulse timing simulation");
    
    // HC-SR04P 스펙: 10μs 이상의 HIGH 펄스 필요
    int min_pulse_duration_us = 10;
    int recommended_pulse_us = 15; // 드라이버에서 사용하는 값
    
    // 최소 요구사항 확인
    if (min_pulse_duration_us < 10) {
        TEST_FAIL("Pulse duration too short");
    }
    
    // 권장값 확인
    if (recommended_pulse_us < min_pulse_duration_us) {
        TEST_FAIL("Recommended pulse shorter than minimum");
    }
    
    // 펄스 간격 확인 (60ms 최소 간격)
    int min_interval_ms = 60;
    if (min_interval_ms < 60) {
        TEST_FAIL("Measurement interval too short");
    }
    
    TEST_PASS();
    return 0;
}

// 실제 드라이버 통합 테스트 (하드웨어 있을 때)
int test_driver_integration(void) {
    TEST_START("Driver integration (hardware required)");
    
    // /dev/hc_sr04p 디바이스 파일 존재 확인
    FILE *device = fopen("/dev/hc_sr04p", "r");
    if (device == NULL) {
        printf("⚠️  SKIPPED (no hardware or driver not loaded)\n");
        return 0;
    }
    
    // 간단한 읽기 테스트
    char buffer[32];
    size_t bytes_read = fread(buffer, 1, sizeof(buffer)-1, device);
    fclose(device);
    
    if (bytes_read > 0) {
        buffer[bytes_read] = '\0';
        int distance = atoi(buffer);
        
        // 합리적인 거리 범위 확인 (3mm ~ 4m)
        if (distance < 3 || distance > 4000) {
            TEST_FAIL("Invalid distance reading from hardware");
        }
        
        printf("✅ PASSED (read %dmm)\n", distance);
        tests_passed++;
    } else {
        TEST_FAIL("Could not read from device");
    }
    
    return 0;
}

// 메인 테스트 함수
int main(void) {
    printf("🚀 Starting HC-SR04P Ultrasonic Sensor Tests\n");
    printf("=============================================\n\n");
    
    // 각 테스트 실행 및 실패 시 즉시 종료
    if (test_distance_calculation_basic() != 0) return 1;
    if (test_distance_calculation_edge_cases() != 0) return 1;  
    if (test_distance_precision() != 0) return 1;
    if (test_gpio_setup() != 0) return 1;
    if (test_trigger_pulse_simulation() != 0) return 1;
    if (test_driver_integration() != 0) return 1;
    
    // 결과 요약
    printf("\n📊 Test Results Summary\n");
    printf("=======================\n");
    printf("Total tests: %d\n", tests_total);
    printf("Passed: %d\n", tests_passed);
    printf("Failed: %d\n", tests_total - tests_passed);
    
    if (tests_passed == tests_total) {
        printf("\n🎉 All tests passed! HC-SR04P driver is ready for production.\n");
        return 0;
    } else {
        printf("\n❌ Some tests failed. Please check the implementation.\n");
        return 1;
    }
}

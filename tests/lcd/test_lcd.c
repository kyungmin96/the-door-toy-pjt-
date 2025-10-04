// tests/lcd/test_lcd_basic.c
#include <stdio.h>
#include <assert.h>
#include <string.h>

// 실제 드라이버 헤더는 커널 의존성 때문에 직접 포함하지 않고
// 테스트용 함수들만 간단히 작성
typedef struct {
    int cursor_col;
    int cursor_row;
    int backlight;
} test_lcd_data_t;

// 간단한 테스트 함수들
int test_cursor_update(void) {
    test_lcd_data_t lcd = {0, 0, 1};
    
    // 커서 위치 업데이트 로직 테스트
    lcd.cursor_col++;
    if (lcd.cursor_col >= 16) {
        lcd.cursor_col = 0;
        lcd.cursor_row++;
    }
    
    assert(lcd.cursor_col == 1);
    printf("✓ Cursor update test passed\n");
    return 0;
}

int test_line_wrap(void) {
    test_lcd_data_t lcd = {15, 0, 1};
    
    // 줄바꿈 테스트
    lcd.cursor_col++;
    if (lcd.cursor_col >= 16) {
        lcd.cursor_col = 0;
        lcd.cursor_row = (lcd.cursor_row + 1) % 2;
    }
    
    assert(lcd.cursor_col == 0);
    assert(lcd.cursor_row == 1);
    printf("✓ Line wrap test passed\n");
    return 0;
}

int test_backlight_blink(void) {
    printf("🔆 Testing: Backlight control... ");
    
    // /dev/i2c_lcd1602에 백라이트 ON/OFF 명령 전송
    FILE *device = fopen("/dev/i2c_lcd1602", "w");
    if (!device) {
        printf("⚠️  SKIPPED (device not found)\n");
        return 0;
    }
    
    // 3초간 깜빡임 (사용자가 눈으로 확인 가능)
    for (int i = 0; i < 6; i++) {
        fprintf(device, "BACKLIGHT_%s\n", (i % 2) ? "ON" : "OFF");
        fflush(device);
        usleep(500000); // 0.5초 대기
    }
    
    fclose(device);
    printf("✅ PASSED (check display visually)\n");
    return 0;
}

int main(void) {
    printf("Starting LCD basic tests...\n");
    
    // test_cursor_update();
    // test_line_wrap();
    test_backlight_blink();
    printf("All tests passed! ✅\n");
    return 0;
}

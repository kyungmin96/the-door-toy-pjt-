# # tests/lcd/Makefile
# CC = gcc
# CFLAGS = -Wall -Wextra -std=c99

# test: test_lcd

# test_lcd_basic: test_lcd.c
# 	$(CC) $(CFLAGS) -o test_lcd test_lcd.c

# clean:
# 	rm -f test_lcd

# run-tests: test
# 	./test_lcd

# .PHONY: test clean run-tests

# tests/lcd/Makefile
CC = gcc
CFLAGS = -Wall -Wextra -std=c99

# 소스 파일
TEST_SOURCES = test_lcd.c

# 기본 타겟
all: test_lcd

# 테스트 바이너리 빌드
test_lcd: $(TEST_SOURCES)
	$(CC) $(CFLAGS) -o test_lcd $(TEST_SOURCES)

# test 타겟 (GitHub Actions에서 호출)
test: test_lcd
	./test_lcd

# 실행 타겟
run-tests: test
	@echo "Tests completed successfully!"

clean:
	rm -f test_lcd

.PHONY: all test run-tests clean

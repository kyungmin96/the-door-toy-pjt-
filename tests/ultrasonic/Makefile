# tests/lcd/Makefile
CC = gcc
CFLAGS = -Wall -Wextra -std=c99

# 소스 파일
TEST_SOURCES = test_ultrasonic.c

# 기본 타겟
all: test_ultrasonic

# 테스트 바이너리 빌드
test_lcd: $(TEST_SOURCES)
	$(CC) $(CFLAGS) -o test_ultrasonic $(TEST_SOURCES)

# GitHub Actions에서 호출하는 테스트 타겟
test: clean test_ultrasonic
	./test_ultrasonic

# 실행 전용 타겟 (빌드 포함)
run-tests: test

clean:
	rm -f test_ultrasonic *.o

.PHONY: all test run-tests clean

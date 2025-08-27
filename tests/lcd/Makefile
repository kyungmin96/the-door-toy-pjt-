# tests/lcd/Makefile
CC = gcc
CFLAGS = -Wall -Wextra -std=c99

test: test_lcd

test_lcd_basic: test_lcd.c
	$(CC) $(CFLAGS) -o test_lcd test_lcd.c

clean:
	rm -f test_lcd

run-tests: test
	./test_lcd

.PHONY: test clean run-tests

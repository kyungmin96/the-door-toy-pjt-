# automatic-door-system/Makefile
# Top-level Makefile for Automatic Door System

# 프로젝트 정보
PROJECT_NAME = the-door-toy-pjt-
VERSION = 1.0.0

# 색상 출력을 위한 변수
RED = \033[0;31m
GREEN = \033[0;32m
YELLOW = \033[0;33m
BLUE = \033[0;34m
NC = \033[0m # No Color

# 드라이버 경로 (현재 + 미래 확장)
DRIVER_DIRS = drivers/lcd

# 테스트 경로
TEST_DIRS = tests/lcd

# 기본 타겟
.PHONY: all clean test install uninstall help status
.DEFAULT_GOAL := all

# 전체 빌드
all: banner build-drivers
	@echo "$(GREEN)✅ Build completed successfully!$(NC)"

# 배너 출력
banner:
	@echo "$(BLUE)"
	@echo "╔══════════════════════════════════════╗"
	@echo "║     Automatic Door System v$(VERSION)     ║"
	@echo "║              Build System            ║"
	@echo "╚══════════════════════════════════════╝"
	@echo "$(NC)"

# 드라이버 빌드
build-drivers:
	@echo "$(YELLOW)🔧 Building drivers...$(NC)"
	@for dir in $(DRIVER_DIRS); do \
		if [ -d $$dir ]; then \
			echo "$(BLUE)Building $$dir...$(NC)"; \
			$(MAKE) -C $$dir || exit 1; \
			echo "$(GREEN)✅ $$dir built successfully$(NC)"; \
		else \
			echo "$(YELLOW)⚠️  $$dir not found, skipping$(NC)"; \
		fi; \
	done

# 전체 정리
clean: clean-drivers clean-tests
	@echo "$(GREEN)🧹 Clean completed!$(NC)"

clean-drivers:
	@echo "$(YELLOW)🧹 Cleaning drivers...$(NC)"
	@for dir in $(DRIVER_DIRS); do \
		if [ -d $$dir ]; then \
			echo "Cleaning $$dir..."; \
			$(MAKE) -C $$dir clean 2>/dev/null || true; \
		fi; \
	done

clean-tests:
	@echo "$(YELLOW)🧹 Cleaning tests...$(NC)"
	@for dir in $(TEST_DIRS); do \
		if [ -d $$dir ]; then \
			echo "Cleaning $$dir..."; \
			$(MAKE) -C $$dir clean 2>/dev/null || true; \
		fi; \
	done

# 전체 테스트
test: build-drivers test-drivers
	@echo "$(GREEN)🧪 All tests completed!$(NC)"

test-drivers:
	@echo "$(YELLOW)🧪 Running driver tests...$(NC)"
	@for dir in $(TEST_DIRS); do \
		if [ -d $$dir ]; then \
			echo "$(BLUE)Testing $$dir...$(NC)"; \
			$(MAKE) -C $$dir run-tests || exit 1; \
			echo "$(GREEN)✅ $$dir tests passed$(NC)"; \
		fi; \
	done

# 드라이버 설치 (실제 하드웨어에서)
install: all
	@echo "$(YELLOW)📦 Installing drivers...$(NC)"
	@if [ "$$EUID" -ne 0 ]; then \
		echo "$(RED)❌ Please run as root (sudo make install)$(NC)"; \
		exit 1; \
	fi
	@for dir in $(DRIVER_DIRS); do \
		if [ -f $$dir/*.ko ]; then \
			echo "Installing $$dir driver..."; \
			insmod $$dir/*.ko || true; \
		fi; \
	done
	@echo "$(GREEN)✅ Installation completed$(NC)"

# 드라이버 제거
uninstall:
	@echo "$(YELLOW)📦 Uninstalling drivers...$(NC)"
	@if [ "$$EUID" -ne 0 ]; then \
		echo "$(RED)❌ Please run as root (sudo make uninstall)$(NC)"; \
		exit 1; \
	fi
	@rmmod i2c_lcd1602_driver 2>/dev/null || true
	@rmmod stepper_motor_driver 2>/dev/null || true  
	@rmmod ultrasonic_driver 2>/dev/null || true
	@echo "$(GREEN)✅ Uninstallation completed$(NC)"

# 프로젝트 상태 확인
status:
	@echo "$(BLUE)📊 Project Status$(NC)"
	@echo "=================="
	@echo "🔧 Available drivers:"
	@for dir in $(DRIVER_DIRS); do \
		if [ -d $$dir ]; then \
			echo "  ✅ $$dir"; \
			if [ -f $$dir/*.ko ]; then \
				echo "     (built)"; \
			else \
				echo "     (not built)"; \
			fi; \
		fi; \
	done
	@echo ""
	@echo "🧪 Available tests:"
	@for dir in $(TEST_DIRS); do \
		if [ -d $$dir ]; then \
			echo "  ✅ $$dir"; \
		fi; \
	done
	@echo ""
	@echo "📋 Loaded kernel modules:"
	@lsmod | grep -E "(lcd|motor|ultrasonic)" || echo "  (none loaded)"

# 개발자용 빠른 빌드
dev: clean all test
	@echo "$(GREEN)🚀 Development build completed!$(NC)"

# 릴리즈 빌드 (MISRA C 검사 포함)
release: clean all test misra-check
	@echo "$(GREEN)🎉 Release build completed!$(NC)"

# MISRA C 검사 (옵션)
misra-check:
	@echo "$(YELLOW)🔍 Running MISRA C compliance check...$(NC)"
	@for dir in $(DRIVER_DIRS); do \
		if [ -d $$dir ] && [ -f $$dir/*.c ]; then \
			echo "Checking $$dir..."; \
			cppcheck --addon=misra $$dir/*.c 2>&1 | tee $$dir/misra-report.txt || true; \
		fi; \
	done

# CI/CD용 타겟
ci: banner all test
	@echo "$(GREEN)✅ CI build successful$(NC)"

# 도움말
help:
	@echo "$(BLUE)Automatic Door System - Available Commands$(NC)"
	@echo "=========================================="
	@echo ""
	@echo "$(YELLOW)Build Commands:$(NC)"
	@echo "  make all         - Build all drivers"  
	@echo "  make clean       - Clean all build artifacts"
	@echo "  make dev         - Quick development build (clean + build + test)"
	@echo "  make release     - Release build with MISRA C check"
	@echo ""
	@echo "$(YELLOW)Test Commands:$(NC)"
	@echo "  make test        - Run all tests"
	@echo "  make misra-check - Run MISRA C compliance check"
	@echo ""
	@echo "$(YELLOW)Deployment Commands:$(NC)"
	@echo "  make install     - Install drivers (requires sudo)"
	@echo "  make uninstall   - Remove installed drivers (requires sudo)"
	@echo ""
	@echo "$(YELLOW)Info Commands:$(NC)"
	@echo "  make status      - Show project status"
	@echo "  make help        - Show this help"
	@echo ""
	@echo "$(YELLOW)Individual Driver Build:$(NC)"
	@echo "  make -C drivers/lcd     - Build LCD driver only"
	@echo "  make -C tests/lcd       - Run LCD tests only"

# 개별 드라이버 빌드 (편의 명령어)
lcd:
	@$(MAKE) -C drivers/lcd

lcd-test:
	@$(MAKE) -C tests/lcd run-tests

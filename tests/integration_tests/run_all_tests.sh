#!/bin/bash
# Run: chmod +x run_all_tests.sh && sudo ./run_all_tests.sh

PROJECT_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
KERNEL_DIR="$PROJECT_DIR/kernel_module"
UTEST_DIR="$PROJECT_DIR/tests/userspace_tests"
ITEST_DIR="$PROJECT_DIR/tests/integration_tests"

RED='\033[0;31m'; GREEN='\033[0;32m'
YELLOW='\033[1;33m'; BLUE='\033[0;34m'; NC='\033[0m'
PASS=0; FAIL=0

run_test() {
	local name="$1" cmd="$2"
	echo -e "${BLUE}[RUN]${NC} $name"
	if eval "$cmd" > /tmp/test_out.txt 2>&1; then
		echo -e "${GREEN}[DONE]${NC} $name"
		cat /tmp/test_out.txt
		((PASS++))
	else
		echo -e "${RED}[FAIL]${NC} $name"
		cat /tmp/test_out.txt
		((FAIL++))
	fi
	echo "----------------------------------------"
}

echo "========================================"
echo "  Linux Driver Project - Full Test Run  "
echo "  Kernel : $(uname -r)"
echo "  PROJECT: $PROJECT_DIR"
echo "========================================"

echo -e "\n${YELLOW}[STEP 1] Build Kernel Modules${NC}"
run_test "Build string_norm.ko + usb_bridge.ko" "make -C $KERNEL_DIR"

echo -e "\n${YELLOW}[STEP 2] Load string_norm Module${NC}"
run_test "Load string_norm" \
	"sudo insmod $KERNEL_DIR/string_norm.ko 2>/dev/null || true; \
	 MAJOR=\$(awk '/string_norm/{print \$1}' /proc/devices); \
	 [ -n \"\$MAJOR\" ] && \
	 sudo rm -f /dev/string_norm; \
	 sudo mknod /dev/string_norm c \$MAJOR 0 && \
	 sudo chmod 666 /dev/string_norm && \
	 echo \"Loaded: major=\$MAJOR\""

echo -e "\n${YELLOW}[STEP 3] Load usb_bridge Module${NC}"
run_test "Load usb_bridge" \
	"sudo insmod $KERNEL_DIR/usb_bridge.ko 2>/dev/null || true; \
	 MAJOR=\$(awk '/usb_bridge/{print \$1}' /proc/devices); \
	 [ -n \"\$MAJOR\" ] && \
	 sudo rm -f /dev/usb_bridge; \
	 sudo mknod /dev/usb_bridge c \$MAJOR 0 && \
	 sudo chmod 666 /dev/usb_bridge && \
	 echo \"Loaded: major=\$MAJOR\""

echo -e "\n${YELLOW}[STEP 4] Normalize Mock Tests (no driver needed)${NC}"
run_test "Compile mock test" \
	"gcc -Wall -o /tmp/test_normalize_mock \
	 $UTEST_DIR/test_normalize_mock.c"
run_test "Run mock test" "/tmp/test_normalize_mock"

echo -e "\n${YELLOW}[STEP 5] SHA-256 Authentication Tests${NC}"
run_test "Compile auth test" \
	"gcc -Wall -Wno-deprecated-declarations \
	 -o /tmp/test_auth $UTEST_DIR/test_auth.c -lssl -lcrypto"
run_test "Run auth test" "/tmp/test_auth"

echo -e "\n${YELLOW}[STEP 6] Student CRUD Unit Tests${NC}"
run_test "Compile student test" \
	"gcc -Wall -g -DUNIT_TESTING -I$PROJECT_DIR/userspace_app -o /tmp/test_student $UTEST_DIR/test_student.c $PROJECT_DIR/userspace_app/student.c"
run_test "Run student test" "/tmp/test_student"

echo -e "\n${YELLOW}[STEP 7] String Norm Driver Integration Tests${NC}"
run_test "Compile driver IO test" \
	"gcc -Wall -o /tmp/test_driver_io $ITEST_DIR/test_driver_io.c"
run_test "Run driver IO test" "sudo /tmp/test_driver_io"

echo -e "\n${YELLOW}[STEP 8] USB Bridge Driver Integration Tests${NC}"
run_test "Compile USB bridge test" \
	"gcc -Wall -I$PROJECT_DIR/kernel_module -o /tmp/test_usb_bridge_io $ITEST_DIR/test_usb_bridge_io.c"
run_test "Run USB bridge test" "sudo /tmp/test_usb_bridge_io"

echo -e "\n${YELLOW}[STEP 9] Kernel Log Check${NC}"
echo "--- Kernel messages ---"
dmesg | grep -E "(string_norm|usb_bridge)" | tail -20 || true

echo -e "\n${YELLOW}[STEP 10] Cleanup${NC}"
run_test "Unload drivers" \
	"sudo rmmod usb_bridge 2>/dev/null || true; \
	 sudo rm -f /dev/usb_bridge; \
	 sudo rmmod string_norm 2>/dev/null || true; \
	 sudo rm -f /dev/string_norm; echo done"

echo ""
echo "========================================"
echo -e "TOTAL: ${GREEN}${PASS} PASSED${NC} | ${RED}${FAIL} FAILED${NC}"
echo "========================================"
exit $FAIL

CC     = gcc
CFLAGS = -Wall -g -Wno-deprecated-declarations

all: build_mock build_auth build_student build_integration build_usb_bridge

build_mock:
	$(CC) $(CFLAGS) -DUNIT_TESTING -I../userspace_app \
	    -o userspace_tests/test_normalize_mock \
	    userspace_tests/test_normalize_mock.c ../userspace_app/student.c

build_auth:
	$(CC) $(CFLAGS) -DUNIT_TESTING -I../userspace_app \
	    -o userspace_tests/test_auth \
	    userspace_tests/test_auth.c ../userspace_app/auth.c -lssl -lcrypto

build_student:
	$(CC) $(CFLAGS) -DUNIT_TESTING -I../userspace_app \
	    -o userspace_tests/test_student \
	    userspace_tests/test_student.c ../userspace_app/student.c

build_integration:
	$(CC) $(CFLAGS) \
	    -o integration_tests/test_driver_io \
	    integration_tests/test_driver_io.c

build_usb_bridge:
	$(CC) $(CFLAGS) -I../kernel_module \
	    -o integration_tests/test_usb_bridge_io \
	    integration_tests/test_usb_bridge_io.c

run_mock: build_mock
	./userspace_tests/test_normalize_mock

run_auth: build_auth
	./userspace_tests/test_auth

run_student: build_student
	./userspace_tests/test_student

run_integration: build_integration
	sudo ./integration_tests/test_driver_io

run_usb_bridge: build_usb_bridge
	sudo ./integration_tests/test_usb_bridge_io

run_all:
	sudo bash integration_tests/run_all_tests.sh

clean:
	rm -f userspace_tests/test_normalize_mock
	rm -f userspace_tests/test_auth
	rm -f userspace_tests/test_student
	rm -f integration_tests/test_driver_io
	rm -f integration_tests/test_usb_bridge_io

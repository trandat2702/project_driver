CC     = gcc
CFLAGS = -Wall -Wextra -g -Wno-deprecated-declarations -I../kernel_module
LIBS   = -lssl -lcrypto
GUI_TARGET = student_manager_gui
GUI_SRCS   = gui_app.c auth.c student.c usb_file.c usb_driver_client.c audit.c
GTK_CFLAGS = $(shell pkg-config --cflags gtk+-3.0)
GTK_LIBS   = $(shell pkg-config --libs gtk+-3.0)

all: $(GUI_TARGET)

$(GUI_TARGET): $(GUI_SRCS)
	$(CC) $(CFLAGS) $(GTK_CFLAGS) -o $(GUI_TARGET) $(GUI_SRCS) $(LIBS) $(GTK_LIBS)

clean:
	rm -f student_manager $(GUI_TARGET)

run-gui: $(GUI_TARGET)
	./$(GUI_TARGET)

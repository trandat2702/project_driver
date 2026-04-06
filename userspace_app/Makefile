CC     = gcc
CFLAGS = -Wall -Wextra -g -Wno-deprecated-declarations
LIBS   = -lssl -lcrypto
TARGET = student_manager
SRCS   = main.c auth.c student.c usb_file.c audit.c
GUI_TARGET = student_manager_gui
GUI_SRCS   = gui_app.c auth.c student.c usb_file.c audit.c
GTK_CFLAGS = $(shell pkg-config --cflags gtk+-3.0)
GTK_LIBS   = $(shell pkg-config --libs gtk+-3.0)

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRCS) $(LIBS)

$(GUI_TARGET): $(GUI_SRCS)
	$(CC) $(CFLAGS) $(GTK_CFLAGS) -o $(GUI_TARGET) $(GUI_SRCS) $(LIBS) $(GTK_LIBS)

clean:
	rm -f $(TARGET) $(GUI_TARGET)

run: $(TARGET)
	./$(TARGET)

run-gui: $(GUI_TARGET)
	./$(GUI_TARGET)

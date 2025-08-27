CC = gcc
PKGS = gstreamer-1.0
TARGET = hello_gst
SRC = hello_world.c

CFLAGS += -g -O0 $(shell pkg-config --cflags $(PKGS))
LIBS += $(shell pkg-config --libs $(PKGS))

.PHONY: all clean run debug

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

run: $(TARGET)
	./$(TARGET)

debug: $(TARGET)
	GST_DEBUG=2 ./$(TARGET)

clean:
	rm -f $(TARGET)



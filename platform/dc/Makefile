TARGET = GM2IMG.elf
OBJS = src/main.o src/scanner.cpp src/qoi_stream.cpp \
	   romdisk.o
KOS_ROMDISK_DIR = romdisk
KOS_CFLAGS += -I${KOS_PORTS}/include/raylib -I${KOS_PORTS}/include/bzlib -I${KOS_PORTS}/include/GL

CXX = kos-c++
CC = kos-cc

CFLAGS += -Wall -std=c++14 -D_DEFAULT_SOURCE -Wno-missing-braces -BZ_NO_STDIO -D__DC__

all: rm-elf $(TARGET)

include $(KOS_BASE)/Makefile.rules

clean: rm-elf
	-rm -f $(OBJS)

rm-elf:
	-rm -f $(TARGET) romdisk.*

$(TARGET): $(OBJS)
	$(CXX) -o $(TARGET) $(OBJS) -lraylib -lGL -lm -lkosutils -lADX -lbz2

run: $(TARGET)
	$(KOS_LOADER) $(TARGET)

dist: $(TARGET)
	-rm -f $(OBJS) romdisk.img
	$(KOS_STRIP) $(TARGET)
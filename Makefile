ifeq ($(host),)
    CC = gcc
else
    CC = $(host)-gcc
endif

CFLAGS += -g -Wall -Wextra -Wno-unused-parameter -fstack-protector-all -D_FILE_OFFSET_BITS=64
CFLAGS += -I ./include -I./include/compile -L./lib

LDFLAGS += -lfuse -lpthread -ldl

TARGET := main

all:$(TARGET)

SRCS := main.c usfs/usfs.c usfs/utils.c usfs/opera.c
OBJS := $(SRCS:%.c=%.o)

$(TARGET):$(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS) $(CFLAGS)

clean:
	@rm -f $(OBJS)
	@rm -f $(TARGET)
    

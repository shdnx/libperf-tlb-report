# Hack to disable implicit rules to VASTLY improve performance; see https://stackoverflow.com/a/4126617/128240
.SUFFIXES:

BUILD_DIR := build
OBJS_DIR := $(BUILD_DIR)/obj

BIN := $(BUILD_DIR)/libperf-tlb-report.a

CC ?= gcc
AR ?= ar
CFLAGS += -O0 -std=gnu11 -Wall -Wextra -Wno-macro-redefined -MMD -MP -Iinclude

SRCS := $(wildcard src/*.c)
OBJS := $(patsubst src/%.c,$(OBJS_DIR)/%.o,$(SRCS))
DEPS := $(OBJS:.o=.d)

$(OBJS_DIR)/%.o: src/%.c
	mkdir -p $(OBJS_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BIN): $(OBJS)
	$(AR) rcs $@ $^

.PHONY: all clean

all: $(BIN)

clean:
	rm -rf $(BUILD_DIR)

-include $(DEPS)

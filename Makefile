# minivm Makefile
#   make         build bin/minivm
#   make clean   delete all build outputs

CC      = gcc
CFLAGS  = -std=c11 -Wall -Wextra -O2 -g -Isrc

BUILD   = build
BIN     = bin

SRCS    = src/main.c src/bytecode.c
OBJS    = $(SRCS:src/%.c=$(BUILD)/%.o)

all: $(BIN)/minivm

# link: all object files -> one executable
$(BIN)/minivm: $(OBJS) | $(BIN)
	$(CC) $(CFLAGS) -o $@ $^

# compile: each src/X.c -> build/X.o
$(BUILD)/%.o: src/%.c | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD) $(BIN):
	mkdir -p $@

clean:
	rm -rf $(BUILD) $(BIN)

.PHONY: all clean

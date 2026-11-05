# minivm Makefile
#   make         build bin/minivm and bin/langc
#   make clean   delete all build outputs

CC      = gcc
CFLAGS  = -std=c11 -Wall -Wextra -O2 -g -D_POSIX_C_SOURCE=200809L -Isrc
FLEX    = flex

BUILD   = build
BIN     = bin

# code shared by every program
CORE_SRCS = src/bytecode.c src/value.c src/vm.c src/frontend.c
CORE_OBJS = $(CORE_SRCS:src/%.c=$(BUILD)/%.o) $(BUILD)/lex.yy.o

all: $(BIN)/minivm $(BIN)/langc

# link: shared objects + the file with main() -> one executable each
$(BIN)/minivm: $(CORE_OBJS) $(BUILD)/main.o | $(BIN)
	$(CC) $(CFLAGS) -o $@ $^

$(BIN)/langc: $(CORE_OBJS) $(BUILD)/langc.o | $(BIN)
	$(CC) $(CFLAGS) -o $@ $^

# flex: lexer.l -> lex.yy.c (generated C code, lives in build/)
$(BUILD)/lex.yy.c: src/lexer.l src/tokens.h src/frontend.h | $(BUILD)
	$(FLEX) -o $@ $<

# generated code is not warning-clean with -Wextra; relax for it only
$(BUILD)/lex.yy.o: $(BUILD)/lex.yy.c
	$(CC) $(CFLAGS) -Wno-unused-function -Wno-sign-compare -c -o $@ $<

# compile: each src/X.c -> build/X.o
$(BUILD)/%.o: src/%.c | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD) $(BIN):
	mkdir -p $@

clean:
	rm -rf $(BUILD) $(BIN)

.PHONY: all clean

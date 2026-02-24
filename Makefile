CC      = gcc
CFLAGS  = -Wall -Wextra -Werror -pedantic -std=c11 -D_XOPEN_SOURCE=700 -D_DEFAULT_SOURCE -Iinclude
LDFLAGS = -lncursesw -lpthread -lm

SRC_DIR = src
OBJ_DIR = obj
BIN_DIR = bin

SRCS    = $(wildcard $(SRC_DIR)/*.c)
OBJS    = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SRCS))
TARGET  = $(BIN_DIR)/bytes

.PHONY: all clean windows clean-windows clean-all

all: $(TARGET)

$(TARGET): $(OBJS) | $(BIN_DIR)
	$(CC) $(CFLAGS) $(OBJS) -o $@ $(LDFLAGS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)

# ── Windows cross-compile via mingw-w64 ────────────────────────────

WIN_CC       = x86_64-w64-mingw32-gcc
WIN_CFLAGS   = -Wall -Wextra -Werror -std=c11 -Iinclude -Ideps/PDCurses -DPDC_WIDE
WIN_LDFLAGS  = deps/PDCurses/wincon/pdcurses.a -lws2_32 -liphlpapi -lm -static
WIN_OBJ_DIR  = obj/win64
WIN_BIN_DIR  = bin
WIN_OBJS     = $(patsubst $(SRC_DIR)/%.c,$(WIN_OBJ_DIR)/%.o,$(SRCS))
WIN_TARGET   = $(WIN_BIN_DIR)/bytes.exe

windows: $(WIN_TARGET)

$(WIN_TARGET): $(WIN_OBJS) | $(WIN_BIN_DIR)
	$(WIN_CC) $(WIN_CFLAGS) $(WIN_OBJS) -o $@ $(WIN_LDFLAGS)

$(WIN_OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(WIN_OBJ_DIR)
	$(WIN_CC) $(WIN_CFLAGS) -c $< -o $@

$(WIN_OBJ_DIR):
	mkdir -p $(WIN_OBJ_DIR)

clean-windows:
	rm -rf $(WIN_OBJ_DIR) $(WIN_BIN_DIR)/bytes.exe

clean-all: clean clean-windows

CC      = gcc
CFLAGS  = -Wall -Wextra -Werror -pedantic -std=c11 -D_XOPEN_SOURCE=700 -D_DEFAULT_SOURCE -Iinclude
LDFLAGS = -lncursesw -lpthread -lm

SRC_DIR = src
OBJ_DIR = obj
BIN_DIR = bin

SRCS    = $(wildcard $(SRC_DIR)/*.c)
OBJS    = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SRCS))
TARGET  = $(BIN_DIR)/bytes

.PHONY: all clean

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

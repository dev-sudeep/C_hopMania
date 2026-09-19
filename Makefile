CC ?= gcc
CFLAGS ?= -Wall -Wextra -pedantic -std=c99 -O2 -Iinclude
LDFLAGS ?= -lm

SRC_DIR = src
INC_DIR = include
OBJ_DIR = obj
BIN_DIR = bin

TARGET = $(BIN_DIR)/hopmania
TEST_TARGET = $(BIN_DIR)/test_game

SRCS = $(SRC_DIR)/main.c \
       $(SRC_DIR)/game.c \
       $(SRC_DIR)/render.c \
       $(SRC_DIR)/terminal.c

OBJS = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SRCS))

.PHONY: all clean run test dirs install

all: dirs $(TARGET)

dirs:
	@mkdir -p $(OBJ_DIR) $(BIN_DIR)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

test: dirs $(OBJ_DIR)/game.o $(OBJ_DIR)/render.o
	$(CC) $(CFLAGS) -o $(TEST_TARGET) tests/test_game.c $(OBJ_DIR)/game.o $(LDFLAGS)
	@./$(TEST_TARGET)
	$(CC) $(CFLAGS) -o $(BIN_DIR)/test_render tests/test_render.c $(OBJ_DIR)/game.o $(OBJ_DIR)/render.o $(LDFLAGS)
	@./$(BIN_DIR)/test_render

run: all
	@./$(TARGET)

clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)

install: all
	install -d $(DESTDIR)/usr/local/bin
	install -m 755 $(TARGET) $(DESTDIR)/usr/local/bin/hopmania

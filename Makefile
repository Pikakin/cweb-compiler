# CWeb Compiler Makefile

# コンパイラとフラグ
CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -g -Wno-format-truncation
LDFLAGS =

# ディレクトリ
SRC_DIR = src
BUILD_DIR = build
BIN_DIR = bin

# ソースファイル
SRCS = $(SRC_DIR)/cweb_compiler.c $(SRC_DIR)/cweb_generator.c
OBJS = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRCS))

# 実行可能ファイル
TARGET = $(BIN_DIR)/cweb

# デフォルトターゲット
all: directories $(TARGET)

# ディレクトリの作成
directories:
	@mkdir -p $(BUILD_DIR)
	@mkdir -p $(BIN_DIR)

# 実行可能ファイルのビルド
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# オブジェクトファイルのコンパイル
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# サンプルのコンパイル
example: $(TARGET)
	./$(TARGET) examples/example_todo.cweb examples/example_todo.jsx

# クリーン
clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR)

# 依存関係
$(BUILD_DIR)/cweb_compiler.o: $(SRC_DIR)/cweb_parser.h
$(BUILD_DIR)/cweb_generator.o: $(SRC_DIR)/cweb_parser.h

.PHONY: all directories clean example

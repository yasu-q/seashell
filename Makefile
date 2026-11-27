# Compiler and flags
CC = gcc
CFLAGS = -pedantic-errors -Wall -fstack-protector-all -Werror

# paths
INCLUDE_DIR = include
SRC_DIR = src
BIN_DIR = bin

# include headers
INCLUDES = -I$(INCLUDE_DIR)

# all at some point
all: $(BIN_DIR) $(BIN_DIR)/seashell

# check if bin exists
$(BIN_DIR):
	mkdir -p $(BIN_DIR)

# =---------- SRC ----------=

# main.o
$(BIN_DIR)/main.o: $(SRC_DIR)/main.c $(INCLUDE_DIR)/seashell.h | $(BIN_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) -c $(SRC_DIR)/main.c -o $(BIN_DIR)/main.o

# executable
$(BIN_DIR)/seashell: $(BIN_DIR)/main.o $(BIN_DIR)/seashell.o | $(BIN_DIR)
	$(CC) $(BIN_DIR)/main.o $(BIN_DIR)/seashell.o -o $(BIN_DIR)/seashell

# seashell.o
$(BIN_DIR)/seashell.o: $(SRC_DIR)/seashell.c $(INCLUDE_DIR)/seashell.h | $(BIN_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) -c $(SRC_DIR)/seashell.c -o $(BIN_DIR)/seashell.o

# =---------- CLEAN ----------=

# remove all .o and .out files in bin
clean:
	@rm -f $(BIN_DIR)/*.o $(BIN_DIR)/*.out $(BIN_DIR)/main

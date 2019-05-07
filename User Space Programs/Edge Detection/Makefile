CC=g++
INCLUDE_DIR := ./include
SRC_DIR := ./src/*
CFLAGS := -lintelfpgaup -lm
W_LVL := -Wall
EXE_FILE := edge_detect

part1: clean_bkp
	$(CC) $(W_LVL) -o $(EXE_FILE) $(SRC_DIR) -I $(INCLUDE_DIR) $(CFLAGS)

clean: clean_bkp
	rm -f part1

clean_bkp:
	rm -rf ./src/*.*~

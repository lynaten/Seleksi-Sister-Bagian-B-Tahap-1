CC = gcc
CFLAGS = -O0 -g -std=gnu11 -Wall

TARGET = main
GENERATOR = generate_bin

SRC = main.c
GEN_SRC = generate.c

.PHONY: all run clean test generate dirs

all: clean build generate run

build: $(TARGET) $(GENERATOR)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

$(GENERATOR): $(GEN_SRC)
	$(CC) $(CFLAGS) -o $(GENERATOR) $(GEN_SRC)

generate: $(GENERATOR)
	./$(GENERATOR)

run:
	@/usr/bin/time -f "Execution time: %E (%es)" ./$(TARGET) < input.txt > output.txt


clean:
	rm -f $(TARGET) $(GENERATOR) input.txt output.txt
	rm -rf outputs/*

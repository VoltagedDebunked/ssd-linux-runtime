CC = clang
SRC = src/main.c
OUTPUT = bin/ssd-linux-runtime
CFLAGS = -Wall -Wextra -O2
BIN = bin

all: $(OUTPUT)

mkdir $(BIN)
$(OUTPUT): $(SRC)
	$(CC) $(CFLAGS) -o $(OUTPUT) $(SRC)

format:
	clang-format -i $(SRC)

clean:
	rm -f $(OUTPUT)

.PHONY: all format clean
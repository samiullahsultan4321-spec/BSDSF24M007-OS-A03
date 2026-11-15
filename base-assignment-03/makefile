CC = gcc
CFLAGS = -Wall -Wextra -g -Iinclude

TARGET = bin/myshell
OBJECTS = obj/main.o obj/shell.o obj/execute.o

all: $(TARGET)

$(TARGET): $(OBJECTS) | bin
	$(CC) $(CFLAGS) $(OBJECTS) -o $(TARGET)

bin:
	mkdir -p bin

obj:
	mkdir -p obj

obj/%.o: src/%.c include/shell.h | obj
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f obj/*.o bin/myshell

run: $(TARGET)
	./bin/myshell

.PHONY: all clean run
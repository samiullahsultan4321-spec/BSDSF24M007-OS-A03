CC = gcc

CFLAGS = -Wall -Wextra -g -Iinclude
LDLIBS = -lreadline

TARGET = bin/myshell1

OBJECTS = obj/main.o obj/shell.o obj/execute.o obj/builtins.o

all: $(TARGET)

$(TARGET): $(OBJECTS) | bin
	$(CC) $(CFLAGS) $(OBJECTS) -o $(TARGET) $(LDLIBS)

bin:
	mkdir -p bin

obj:
	mkdir -p obj

obj/%.o: src/%.c include/shell.h | obj
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f obj/*.o bin/myshell1

run: $(TARGET)
	./bin/myshell1

.PHONY: all clean run
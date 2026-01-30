CC = gcc
CFLAGS = -Wall -Wextra -g
TARGET = main
OBJS = main.o Library.o

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

main.o: main.c Library.h
	$(CC) $(CFLAGS) -c main.c

Library.o: Library.c Library.h
	$(CC) $(CFLAGS) -c Library.c

clean:
	rm -f $(OBJS) $(TARGET)

rebuild: clean all

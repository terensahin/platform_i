CC = gcc
CFLAGS = -c -g -Wall -Wextra
OBJS = c_vector.o common.o
BUILD = ../build/

all: move

move: $(OBJS)
	mv *.o $(BUILD)

%.o: %.c
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f $(OBJS)

.PHONY: all clean $(BUILD)

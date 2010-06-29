CFLAGS = -O3 --std=c99

OBJS = fifo.o mencoder.o
LIB = libmencoder.a

all: $(LIB) example

$(LIB): $(OBJS)
	ar rcs $@ $^

example: example.o $(LIB)
	$(CC) -o $@ $< -L. -lmencoder -lm

clean:
	rm -f $(OBJS) $(LIB) example.o example

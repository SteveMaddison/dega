CFLAGS = -O3 --std=c99

OBJS = libvfw.o
LIB = libvfw.a

all: $(LIB) example.exe

$(LIB): $(OBJS)
	$(AR) rcs $@ $^

example.exe: example.o $(LIB)
	$(CC) -o $@ $< -L. -lvfw -lvfw32 -lmsacm32 -lm

clean:
	rm -f $(OBJS) $(LIB) example.o example.exe

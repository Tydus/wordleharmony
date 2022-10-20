
CFLAGS ?= -Ofast -mtune=native -openmp

all: harmony

clean:
	$(RM) harmony

harmony: harmony.c



CFLAGS ?= -Ofast -mtune=native -fopenmp

all: harmony

clean:
	$(RM) harmony

harmony: harmony.c


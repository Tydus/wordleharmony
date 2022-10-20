
CFLAGS ?= -Ofast -mtune=native

all: harmony

clean:
	$(RM) harmony

harmony: harmony.c


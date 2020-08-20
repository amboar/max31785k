CFLAGS=-std=gnu11 -Wall -Wextra -Werror -O2

ds3900: ds3900.o

.PHONY: clean
clean:
	$(RM) ds3900 ds3900.o

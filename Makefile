CFLAGS=-std=gnu11 -Wall -Wextra -Werror -O2

max31785k: ds3900.o max31785k.o smbus.o pmbus.o

.PHONY: clean
clean:
	$(RM) max31785k ds3900.o max31785k.o smbus.o

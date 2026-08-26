CC      = gcc
CFLAGS  = -std=c11 -Wall -Wextra -Werror -I ../include

.PHONY: all clean

all: afas

afas: afas.c ../include/opcodes.h ../include/alf.h ../src/load.c
	$(CC) $(CFLAGS) -o afas afas.c ../src/load.c

clean:
	rm -f afas afas.exe

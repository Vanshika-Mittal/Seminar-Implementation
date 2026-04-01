CC=gcc
CFLAGS=-Wall -I. -Wno-pointer-sign -Wno-unused-function -Wno-int-conversion -D_DEFAULT_SOURCE -g
DEPS = ral.h linux_xcompat.h opal_api.h opal.h storage_if.h

OBJ = main.o opal.o ral_freertos.o storage_if.o

all: opal_test

opal.c opal.h opal_api.h: strip_linux_final.py uapi_sed-opal.h sed-opal.c
	python3 strip_linux_final.py

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

opal_test: opal.c opal.h opal_api.h $(OBJ)
	$(CC) -o $@ $(OBJ) $(CFLAGS) -lpthread

clean:
	rm -f *.o opal_test opal.c opal.h opal_api.h

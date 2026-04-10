CC=gcc
CFLAGS=-Wall -Wno-pointer-sign -Wno-unused-function -D_DEFAULT_SOURCE -g -I.
DEPS = ral.h opal_api.h opal_core.h storage_if.h mock_responses.h
OBJ = main.o opal_core.o ral_freertos.o storage_if.o
all: opal_test
%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)
opal_test: $(OBJ)
	$(CC) -o $@ $(OBJ) $(CFLAGS) -lpthread
clean:
	rm -f *.o opal_test
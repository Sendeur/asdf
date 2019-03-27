CC = gcc
SRCFLAGS = -fPIC -c
LIBFLAGS = -shared

build: libso_stdio.so

libso_stdio.so: tema2.o
	$(CC) $(LIBFLAGS) $^ -o $@

tema2.o: tema2.c
	$(CC) $(SRCFLAGS) $^

.PHONY: clean
clean:
	rm -f *.o *.so

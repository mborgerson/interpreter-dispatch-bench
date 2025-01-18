CC = clang
CFLAGS = -Wall -Werror -O2 -g #-fno-gcse

bench: bench.c
	$(CC) -o $@ $(CFLAGS) $<

.PHONY: clean
clean:
	rm bench

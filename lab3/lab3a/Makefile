# NAME:  Thilan Tran, Collin Prince
# EMAIL: thilanoftran@gmail.com, cprince99@g.ucla.edu
# ID:    605140530, 505091865

CFLAGS=-Wall -Wextra
TARGET=lab3a
DEPS=ext2_fs.h
DIST=lab3a-605140530.tar.gz

.PHONY: dist clean

$(TARGET): $(TARGET).c
	gcc $(CFLAGS) $@.c -o $@

dist: $(TARGET).c $(DEPS) Makefile README
	tar -czf $(DIST) $^

clean:
	rm -f $(TARGET) $(DIST)

# For linux only, at the moment

CC=gcc
LD=gcc
CFLAGS= -c -W -Wall -Wextra -ggdb

MAINPROG=l2h
OBS=\
	 l2h_main.o

.PHONY: buildinfo

all: $(MAINPROG) buildinfo.txt

buildinfo.txt:
	@echo TARGET=`gcc -dumpmachine` > $@
	@echo "COMPILER_NAME=`gcc -v 2>&1 | tail -n 1 | cut -f 1 -d \  `" >> $@
	@echo "COMPILER_VERSION=`gcc -v 2>&1 |tail -n 1 |  cut -f 3 -d \  `" >> $@

$(MAINPROG): $(OBS)
	$(LD) $(OBS) -o $@

%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -rfv buildinfo $(OBS) $(MAINPROG) `find . | grep "\.html\$$"`


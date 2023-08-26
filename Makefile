# For linux only, at the moment

CC=gcc
LD=gcc
CFLAGS= -c -W -Wall -Wextra -ggdb

MAINPROG=l2h
OBS=\
	 l2h_main.o


$(MAINPROG): $(OBS)
	$(LD) $(OBS) -o $@

%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -rfv $(OBS) $(MAINPROG) `find . | grep "\.html\$$"`


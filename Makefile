includes = headers

VPATH = src:headers
CC = gcc
CFLAGS = -I$(includes) -Wall -pedantic -O2

objects = main.o mysyslog.o stresstester.o
progs = mysyslog stresstester libmysyslog.a

## Default target, build all progs
all : $(progs)

# Make predefined pattern rule, which compiles .c source files to
# .o object files. Repeated here for convenience.
%.o : %.c
	$(CC) -c $(CFLAGS) $(CPPFLAGS) $< -o $@

## Program executives
mysyslog : main.o mysyslog.o
	$(CC) -o $@ $^ -pthread

stresstester : stresstester.o libmysyslog.a
	$(CC) -o $@ $^ -pthread

## Libraries
libmysyslog.a : mysyslog.o
	ar rv $@ $^
	ranlib $@

## Header file dependencies
main.o : mysyslog.h
mysyslog.o : mysyslog.h
stresstester.o : mysyslog.h

## Phonies
.PHONY : clean debug dist
clean :
	-rm -f $(progs) $(objects) mysyslog.log mysyslog.pipe

debug : clean all
debug : CFLAGS += -g -DDEBUG

dist : clean
	cd .. && tar czvf mysyslog.tar.gz mysyslog/

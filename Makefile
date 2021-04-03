DESTDIR?=
PREFIX?=/usr/local
 
#CFLAGS=-g -Wall -ansi -pedantic
CFLAGS=-O3 -Wall -ansi -pedantic
LDFLAGS=-s
SRC=inflate.c inflate_simple.c deflate.c \
    sflate.c crc.c adler.c
OBJ=${SRC:.c=.o}
EXE=sflate inflate_simple

all: ${EXE}
sflate: sflate.o crc.o adler.o inflate.o deflate.o
	${CC} -o $@ $^ ${LDFLAGS}
 inflate_simple: inflate_simple.o
	${CC} -o $@ $^ ${LDFLAGS}
${OBJ}: Makefile flate.h
.c.o:
	${CC} -c ${CFLAGS} $<
clean:
	rm -f ${OBJ} ${EXE}

prof: profinf profdef

profinf: inflate.c deflate.c sflate.c crc.c adler.c
	gcc -O3 -fprofile-arcs -ftest-coverage -pg -g -Wall $+
	./a.out -d <a.z >/dev/null
	gcov -b $+ >/dev/null
	gprof a.out >inflate.gprof
#	gcc -g -Wall $+
#	valgrind -v --leak-check=yes ./a.out -d <a.z >/dev/null 2>a.valgrind
#	grep ERROR a.valgrind
#	grep alloc a.valgrind
	rm -f a.out a.valgrind *.gcno *.gcda gmon.out

 profdef: inflate.c deflate.c sflate.c crc.c adler.c
	gcc -O0 -fprofile-arcs -ftest-coverage -pg -g -Wall $+
 	./a.out <a.u >/dev/null
	gcov -b $+ >/dev/null
	gprof a.out >deflate.gprof
	rm -f a.out *.gcno *.gcda gmon.out

install:
	mkdir -p ${DESTDIR}${PREFIX}/bin
	cp sflate ${DESTDIR}${PREFIX}/bin
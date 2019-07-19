SOURCES = cmdexec.c fileio.c page_list.c pdf_lib.c ps_lib.c vldoc.c common.c main.c\
    pdf_doc.c pdf_parser.c vdoc.c dummydoc.c pdf_filters.c ps_doc.c vdocerror.c
OBJS = $(SOURCES:.c=.o)
CC = gcc
CFLAGS = -Wall -O2 -s -fno-strict-aliasing
INCLUDES =
LIBS = -lm -lz

pspdftool: ${OBJS}
	${CC} ${CFLAGS} -o $@ ${OBJS} ${LIBS}

clean:
	rm -f *.o

.cpp.o:
	${CC} ${CFLAGS} ${INCLUDES} -c $<

install:
	install pspdftool /usr/local/bin

uninstall:
	rm /usr/local/bin/pspdftool

installman:
	cp ../pspdftool.1 /usr/share/man/man1

removeman:
	rm /usr/share/man/man1/pspdftool.1

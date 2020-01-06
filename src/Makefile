SOURCES = cmdexec.c fileio.c page_list.c pdf_lib.c vldoc.c common.c main.c\
    pdf_doc.c pdf_parser.c vdoc.c pdf_filters.c vdocerror.c dummydoc.c
OBJS = $(SOURCES:.c=.o)
CC = gcc
CFLAGS = -Wall -O2 -fno-strict-aliasing
INCLUDES =
LIBS = -lm -lz

pdfcook: ${OBJS}
	${CC} -s -o $@ ${OBJS} ${LIBS}

clean:
	rm -f *.o

.cpp.o:
	${CC} ${CFLAGS} ${INCLUDES} -c $<

install:
	install pdfcook /usr/local/bin

uninstall:
	rm /usr/local/bin/pdfcook

installman:
	cp ../pdfcook.1 /usr/share/man/man1

removeman:
	rm /usr/share/man/man1/pdfcook.1

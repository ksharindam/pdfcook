#pragma once
/* This file is a part of pdfcook program, which is GNU GPLv2 licensed */

#include <cstdio>

// returns current seek position
#define myftell(f) ((f->pos)-((f->end) - (f->ptr)))
// returns true if seek pos is at the end
#define myfeof(f) (f->eof==EOF && (f->ptr==f->end))
// returns the current char and seek 1 byte forward
#define mygetc(f) ((f->ptr < f->end) ? *(f->ptr)++:slow_mygetc(f))
// seek 1 byte backward
#define myungetc(f) (f->ptr=((f->ptr)>(f->buf))?(f->ptr-1):(f->buf))


enum {UNDEF=0,CR,LF,CRLF};

extern char lend_str[4][3];

typedef struct {
	FILE * f;
	unsigned char * buf;
	unsigned char * ptr;
	unsigned char * end;
	long pos;
	int eof;
	int column;
	int row;
	int lastc;
	int scratch;
} MYFILE;

MYFILE * myfopen(const char * filename, const char *mode);

int myfclose(MYFILE *stream);

int myfseek(MYFILE *stream, long offset, int origin);

size_t myfsize(MYFILE * stream);

size_t myfread(void * where, size_t size, size_t nmemb, MYFILE *stream);
// read reverse size*nmemb bytes
size_t myfrread(void * where, size_t size, size_t nmemb, MYFILE * stream);
size_t myfwrite(void * where, size_t size, size_t nmemb, MYFILE * stream);
// read string upto next newline
long myfgets(char *string, int len, MYFILE *f, int *eoln);
int slow_mygetc(MYFILE * f);

int swrite (FILE *dst, char *co, size_t delka, int lend);

int bwrite(FILE *src, FILE *dst, long poz, size_t len);

MYFILE * stropen(const char *str);
MYFILE * streamopen(const char *str, size_t len);


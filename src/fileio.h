#pragma once
/* This file is a part of pdfcook program, which is GNU GPLv2 licensed */
#include <cstdio>
#include <cctype> // toupper() isspace() etc

typedef struct {
    FILE *f;
    char *buf;
    char *ptr;
    char *end;
    long pos;// how much we have read the file/string until now
    int eof;
    int column;
    int row;
    int lastc;
} MYFILE;

// returns current seek position
#define myftell(f) ((f->pos)-((f->end) - (f->ptr)))
// returns true if seek pos is at the end
#define myfeof(f) (f->eof==EOF && (f->ptr==f->end))
// returns the current char and seek 1 byte forward
#define mygetc(f) ((f->ptr < f->end) ? *(f->ptr)++ : slow_mygetc(f))
// seek 1 byte backward
#define myungetc(f) (f->ptr = ((f->ptr)>(f->buf)) ? (f->ptr-1) : (f->buf))


// open a file stream by given filename
MYFILE * myfopen(const char * filename, const char *mode);
// close a stream
int myfclose(MYFILE *stream);

// returns 0 on success and EOF on failure
int myfseek(MYFILE *stream, long offset, int origin);
// read size*nmemb bytes from *stream and put data in *where
size_t myfread(void *where, size_t size, size_t nmemb, MYFILE *stream);
// read string upto next newline
char* myfgets(char *line, int size, MYFILE *stream);
// it is getc() for MYFILE when re-reading the file is needed to fill buffer
int slow_mygetc(MYFILE * f);

// create a MYFILE from null terminated string
MYFILE * stropen(const char *str);
// create a MYFILE any stream with given len
MYFILE * streamopen(const char *str, size_t len);

inline void skipspace(MYFILE *f) {
    int c;
    while ((c = mygetc(f))!=EOF && isspace(c));
    if (c!=EOF)
        myungetc(f);
}

bool file_exist (const char *name);

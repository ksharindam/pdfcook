/* This file is a part of pdfcook program, which is GNU GPLv2 licensed */
#include <cstdlib>
#include <cstring>
#include "fileio.h"
#include "debug.h"
#include "common.h"

// 16KB buffer for MYFILE
#define BUFSIZE 16384

#define crlf(x) (((x)=='\r') || ((x)=='\n'))


MYFILE * stropen(const char *str)
{
    if (str==NULL)
        return NULL;
    size_t len = strlen(str);
    return streamopen(str, len);
}

// read the whole string in buffer, MYFILE uses the buffer to read data
MYFILE * streamopen(const char *str, size_t len)
{
    if (str==NULL){
        return NULL;
    }
    MYFILE *f = (MYFILE*) malloc2(sizeof(MYFILE));
    f->f = NULL;

    // allocate buffer and read whole string
    f->buf = (char *) malloc(len);
    if (f->buf==NULL){
        free(f);
        return NULL;
    }
    memcpy(f->buf, str, len);
    f->ptr = f->buf;
    f->end = f->buf + len;
    f->pos = len;
    f->eof = EOF;// means no data left to read from string
    f->row = 1;
    f->column = 0;
    f->lastc = 0;
    return f;
}

/* Open a file from filename and mode, creates a buffer.
 this buffer is used to store and read file data. */
MYFILE * myfopen(const char *filename, const char *mode)
{
    MYFILE *f = (MYFILE*) malloc2(sizeof(MYFILE));

    f->f = fopen(filename, mode);

    if (f->f==NULL){
        free(f);
        return NULL;
    }

    f->buf = (char*) malloc(BUFSIZE);
    if (f->buf==NULL){
        fclose(f->f);
        free(f);
        return NULL;
    }
    f->ptr = f->end = f->buf;// this indicates we have not read buffer
    f->pos = 0;
    f->eof = 0;
    return f;
}


int myfclose(MYFILE *stream)
{
    int ret = stream->f ? fclose(stream->f) : 0;
    free(stream->buf);
    free(stream);
    if (ret==EOF){
        return -1;
    }
    return 0;
}

int myfseek(MYFILE *stream, long offset, int origin)
{
    if (stream->f==NULL)
    {
        switch (origin){
            case SEEK_SET:
                stream->ptr = stream->buf + offset;
                break;
            case SEEK_END:
                stream->ptr = stream->end + offset;
                break;
            case SEEK_CUR:
                stream->ptr = stream->ptr + offset;
                break;
        }
        if (stream->ptr < stream->buf || stream->ptr > stream->end){
            return -1;
        }
        return 0;
    }
    stream->eof = fseek(stream->f, offset, origin);
    if (stream->eof==0) {
        stream->pos = ftell(stream->f);
        stream->ptr = stream->end = stream->buf;
        return 0;
    }
    stream->eof = EOF;
    return -1;
}

// it gets called only when re-reading of the whole buffer needed.
int slow_mygetc(MYFILE *f)
{
    if (myfeof(f)){
        return EOF;
    }
    // does not reach here if MYFILE created from file (not from string)
    size_t len = fread(f->buf, 1, BUFSIZE, f->f);
    f->pos += len;
    f->ptr = f->buf;
    f->end = f->buf + len;
    if (len!=BUFSIZE){
        f->eof = EOF;
        if (len==0)
            return EOF;
    }
    return *(f->ptr)++;
}


size_t myfread (void *where, size_t size, size_t nmemb, MYFILE *stream)
{
    char *str = (char *) where;
    size_t read;
    int c;
    long pos = myftell(stream);

    if (stream->f != NULL){
        if (myfseek(stream, pos, SEEK_SET)==-1){
            message(FATAL,"seek error");
        }
        read = fread(where, size, nmemb, stream->f);
        stream->pos = ftell(stream->f);
        stream->ptr = stream->end = stream->buf;
        return read;
    }
    // if MYFILE was created from string
    for (read=0; read<size*nmemb; ++read){
        if ((c=mygetc(stream))==EOF){
            return read/size;
        }
        *str = c;
        ++str;
    }
    return read/size;
}

/*
 read maximum len-1 bytes or until newline or EOF is reached.
 Stores the data in line buffer. Last byte is '\0' (NULL).
 Unlike fgets() it does not write newline characters.
 On success returns given line pointer and on fail returns NULL
 */
char* myfgets(char *line, int len, MYFILE *f)
{
    char *buf = line;
    --len;
    int llen = len-1;

    if (len<=0 || myfeof(f)) {
        *line = 0;
        return NULL;
    }
    do {
        *line = mygetc(f);
        ++line; --len;
    }
    while (len && !myfeof(f) &&  !crlf(*(line-1)));

    if (llen==len && myfeof(f))// could not read any byte before EOF
    {
        f->eof = EOF;
        *(line-1) = 0;
        return NULL;
    }

    switch (*(line-1)) {
        case '\n':
            *(line-1) = 0;
            break;
        case '\r':
            *(line-1) = 0;

            if (mygetc(f)!='\n') {
                myungetc(f);
            }
            break;
    }
    *line = 0;
    return buf;
}

bool file_exist (const char *name)
{
    FILE *f = fopen(name,"r");
    if (f==NULL) {
        return false;
    }
    fclose(f);
    return true;
}

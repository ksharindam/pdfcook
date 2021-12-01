/* This file is a part of pdfcook program, which is GNU GPLv2 licensed */
#include <cstdlib>
#include <cstring>
#include "fileio.h"
#include "debug.h"

// 16KB buffer for MYFILE
#define BUFSIZE 16384

#define crlf(x) (((x)=='\r') || ((x)=='\n'))

// it is getc() for MYFILE that is created from file (not from string)
// it gets called only when re-reading of the whole buffer needed.
int slow_mygetc(MYFILE *f)
{
    size_t len;

    if (myfeof(f)){
        return EOF;
    }
    len = fread(f->buf, sizeof(char), BUFSIZE, f->f);
    f->pos += len;
    f->ptr = f->buf;
    f->end = f->buf + (len>0 ? len:0);
    if (len!=BUFSIZE){
        f->eof = EOF;
    }
    if (myfeof(f)){
        return EOF;
    }
    f->ptr = f->buf;
    f->end = f->buf + len;
    return *(f->ptr)++;
}

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
    MYFILE * f;
    if (str==NULL){
        return NULL;
    }
    f = (MYFILE *) malloc (sizeof(MYFILE));
    if (f==NULL){
        return NULL;
    }
    f->row = 1;
    f->column = 0;
    f->lastc = 0;
    // allocate buffer and read whole string
    f->buf = (char *) malloc(len+1);
    if (f->buf==NULL){
        free(f);
        return NULL;
    }
    memcpy(f->buf, str, len+1);
    f->pos = len;// means we have read whole string
    f->f = NULL;
    f->ptr = f->buf;
    f->end = f->buf + len;
    f->eof = EOF;
    return f;
}

/* Open a file from filename and mode, creates a buffer.
 this buffer is used to store and read file data. */
MYFILE * myfopen(const char *filename, const char *mode)
{
    MYFILE *f = (MYFILE *) malloc(sizeof(MYFILE));
    if (f==NULL){
        return NULL;
    }
    f->buf = (char *) malloc(BUFSIZE);
    if (f->buf==NULL){
        free(f);
        return NULL;
    }
    f->f = fopen(filename, mode);

    if (f->f==NULL)
    {
        free(f->buf);
        free(f);
        return NULL;
    }
    f->pos = 0;
    f->ptr = f->buf + BUFSIZE;// this indicates we have not read buffer
    f->end = f->buf + BUFSIZE;
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
                stream->ptr = stream->end - offset;
            break;
            case SEEK_CUR:
                stream->ptr = stream->ptr + offset;
            break;
        }
        if (stream->ptr >= stream->end){
            return EOF;
        }
        return 0;
    }
    if (fseek(stream->f,offset,origin)!=EOF)
    {
        stream->pos = ftell(stream->f);
        stream->ptr = stream->buf + BUFSIZE;
        stream->end = stream->buf + BUFSIZE;
        stream->eof = 0;
    }
    else {// already reached end, so could not read any byte
        stream->eof = EOF;
    }
    return stream->eof;
}


size_t myfread (void *where, size_t size, size_t nmemb, MYFILE *stream)
{
    long poz = myftell(stream);
    char *str = (char *) where;
    size_t read;
    int c;
    if (stream->f != NULL){
        if (myfseek(stream, poz, SEEK_SET)==-1){
            message(FATAL,"seek error");
        }
        read = fread(where, size, nmemb, stream->f);
        if (read==nmemb){
            if (myfseek(stream, poz+read, SEEK_SET)==-1){
                message(FATAL, "seek error");
            }
            return read;
        }
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

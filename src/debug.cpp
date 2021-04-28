/* This file is a part of pdfcook program, which is GNU GPLv2 licensed */
#include "debug.h"
#include <cstdlib>
#include <cstdarg>
#include <cstring>

int quiet_mode = 0;

#define MAX_MSG_LEN 255 /* maximum formatted message length */

void message(int type, const char *format, ...)
{
    if (quiet_mode && type!=FATAL)
        return;
    char msgbuf[MAX_MSG_LEN+1] = {};    /* buffer in which to put the message */
    char *bufptr = msgbuf ; /* message buffer pointer */
    int pos = 0;
    // should put newline if column is not 0 in terminal
    if (type==WARN) {
        snprintf(bufptr, 11, "warning : ");
        bufptr += 10;
        pos += 10;
    }
    else if (type==ERROR || type==FATAL) {
        snprintf(bufptr, 9, "error : ");
        bufptr += 8;
        pos += 8;
    }
    va_list args ;
    va_start(args, format);
    vsnprintf(bufptr, MAX_MSG_LEN-pos, format, args);
    va_end(args);
    // write the string to stdout or stderr
    fwrite(msgbuf, strlen(msgbuf), 1, stderr);
    fwrite("\n", 1, 1, stderr);
    if ( type==FATAL )  // exit program after the FATAL msg
        exit(1) ;
}

void debug(const char *format, ...)
{
#ifdef DEBUG
    va_list args ;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    printf("\n");
#endif
}

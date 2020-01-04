#include "vdocerror.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

int vdoc_errno=0;

#define MAX_MSG_LEN	255	/* maximum formatted message length */

void message(int flags, const char *format, ...)
{
    char msgbuf[MAX_MSG_LEN+1] = {};	/* buffer in which to put the message */
    char *bufptr = msgbuf ;	/* message buffer pointer */
    int pos = 0;
    // should put newline if column is not 0 in terminal
    if (flags==1) {
        snprintf(bufptr, 11, "warning : ");
        bufptr += 10;
        pos += 10;
    }
    else if (flags==2) {
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
    if ( flags==2 )	// exit program after the FATAL msg
        exit(1) ;
}

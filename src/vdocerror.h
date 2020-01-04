#ifndef _VDOCERR_H_
#define _VDOCERR_H_

extern int  vdoc_errno;
/* message types */
#define LOG		0
#define WARN	1
#define FATAL	2

enum err_val {
    VDOC_ERR_NOERROR, VDOC_ERR_LIBC, VDOC_ERR_WRONG_MAGIC, VDOC_ERR_NULL_POINTER,
    VDOC_ERR_OUT_OF_RANGE, VDOC_ERR_NOT_IN_THE_LIST, VDOC_ERR_IS_NOT_ID_TOKEN
};

void message(int flags, const char *format, ...);
#endif

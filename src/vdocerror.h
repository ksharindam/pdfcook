#ifndef _VDOCERR_H_
#define _VDOCERR_H_
/**
 * \file vdocerror.h
 * \brief Knihovna pro hlaseni chyb.
 *
 * */


#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>


/* message flags */
#define MESSAGE_NL	1	/* Newline before message if necessary */
#define MESSAGE_PROGRAM	2	/* announce program name */
#define MESSAGE_EXIT	4	/* do not return */

/* message types */
#define FATAL		(MESSAGE_EXIT|MESSAGE_PROGRAM|MESSAGE_NL)
#define WARN		(MESSAGE_NL|MESSAGE_PROGRAM)
#define LOG		0



/*
 * Pokud je \a vdoc_errno rovno 0, nenastala pri volani funkce zadna chyba,
 * je-li errno vetsi nez 0 nastala pri volani funkce chyba.
 */
enum err_val{VDOC_ERR_NOERROR, VDOC_ERR_LIBC, VDOC_ERR_WRONG_MAGIC, VDOC_ERR_NULL_POINTER,
	     VDOC_ERR_OUT_OF_RANGE, VDOC_ERR_NOT_IN_THE_LIST, VDOC_ERR_IS_NOT_ID_TOKEN};
extern int  vdoc_errno;
void message(int flags, char *format, ...);
#endif

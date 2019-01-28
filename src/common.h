#ifndef _COMMON_H_
#define _COMMON_H_

#define _GNU_SOURCE 
/*#define _XOPEN_SOURCE*/

#include "config.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <assert.h>
#include <math.h>
#include <unistd.h>
#include "ps_lib.h"
#include "pdf_lib.h"

#ifdef MALOC_CHECK
extern size_t __count;
#define malloc(a) (__count+=(a),printf("%s:%d:%lu\n",__FILE__,__LINE__,(__count)),malloc(a))
#endif

#ifndef HAVE_ASPRINTF
	int asprintf(char **strp, const char *fmt, ...);
 #endif

/*format, ktery bude zvolen v pripade neplatne koncovky*/
#define starts(s1,s2)	(strncmp(s1,s2,strlen(s2)) == 0)
#define setdim(dim,lx,ly,rx,ry) dim.left.x=lx,dim.left.y=ly,dim.right.x=rx,dim.right.y=ry 
#define isdimzero(dim) ((dim).right.x==0 && (dim).right.y==0 && (dim).left.x==0 && (dim).left.y==0)
#define listcat(type,l1,l2) l1->prev->next=l2->next,l2->next->prev=l1->prev,l2->prev->next=(type *)l1,l1->prev=l2->prev
#define listinsert(what,behide) what->prev=behide, what->next=behide->next,behide->next=what,what->next->prev=what

#define max(a,b) ((a)>(b))?(a):(b)
#define min(a,b) ((a)<(b))?(a):(b)

char * skipwhspaces(char * s);
char * strtoupper(char * s);
char * strlower(char * s);

/**
	\brief Funkce prevede string na ID dle vstupniho pole \a from
*/
int strint(char * what,char * from[]);

#endif

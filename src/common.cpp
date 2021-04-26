/* This file is a part of pdfcook program, which is GNU GPLv2 licensed */
#include "common.h"

// read a big endian integer provided as char array
int arr2int(char *arr, int len)
{
    char tmp[4] = {};
    for (int i=0; i<len; i++) {
        tmp[4-len+i] = arr[i];
    }
    // network byte order is big endian, eg. int 16 is stored as 0x000010 in 3 bytes
    return ((uint)tmp[0]<<24 | (uint)tmp[1]<<16 | (uint)tmp[2]<<8 | (uint)tmp[3] );
}


#if (!HAVE_ASPRINTF)
#include <stdarg.h>
int asprintf(char **strp, const char *fmt, ...){
	/* Guess we need no more than 100 bytes. */
	 int n, size = 100;
	 char *p, *np;
	 va_list ap;

	 if ((p = (char*)malloc (size)) == NULL){
	    return -1;
	 }

	 while (1) {
	    /* Try to print in the allocated space. */
	    va_start(ap, fmt);
	    n = vsnprintf (p, size, fmt, ap);
	    va_end(ap);
	    /* If that worked, return the string. */
	    if ((n > -1) && (n < size)){
	    	*strp = p;
	       return n;
	    }
	    /* Else try again with more space. */
	    if (n > -1)    /* glibc 2.1 */
	       size = n+1; /* precisely what is needed */
	    else           /* glibc 2.0 */
	       size *= 2;  /* twice the old size */
	    if ((np = (char*)realloc (p, size)) == NULL) {
	       free(p);
	       return -1;
	    } else {
	       p = np;
	    }
	 }
	 return -1;
}
#endif


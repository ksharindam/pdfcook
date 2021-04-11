#pragma once
/* This file is a part of pdfcook program, which is GNU GPLv2 licensed */
#include "config.h"
#include <stdio.h>
#include <string.h>// memcpy and other string func
#include <stdlib.h>
#include <ctype.h>
#include <assert.h>
#include <math.h>
#include <unistd.h>
#include <arpa/inet.h> // ntohl() function


#ifndef HAVE_ASPRINTF
	int asprintf(char **strp, const char *fmt, ...);
#endif

// check if string s1 starts with s2
#define starts(s1, s2)	(strncmp(s1,s2,strlen(s2)) == 0)

#define MAX(a,b) ((a)>(b) ? (a):(b))
#define MIN(a,b) ((a)<(b) ? (a):(b))

//char * skipwhspaces(char * s);
//char * strtoupper(char * s);
//char * strlower(const char *s);

//int strint(char * what, char * from[]);

// read a big endian integer provided as char array
inline int arr2int(char *arr, int len) {
    char tmp[4] = {};
    for (int i=0; i<len; i++) {
        tmp[4-len+i] = arr[i];
    }
    // network byte order is big endian, eg. int 16 is stored as 0x000010 in 3 bytes
    return ntohl(*((int*)(&tmp[0])));
}


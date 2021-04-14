#pragma once
/* This file is a part of pdfcook program, which is GNU GPLv2 licensed */
#include "config.h"
#include <cstdio>
#include <cstring>// memcpy and other string func
#include <cassert>
#include <cmath>
//#include <cstdint> // uint32_t type
//#include <cctype> // toupper() isspace() etc


#ifndef HAVE_ASPRINTF
int asprintf(char **strp, const char *fmt, ...);
#endif

// check if string s1 starts with s2
#define starts(s1, s2)    (strncmp(s1,s2,strlen(s2)) == 0)

#define MAX(a,b) ((a)>(b) ? (a):(b))
#define MIN(a,b) ((a)<(b) ? (a):(b))

//char * skipwhspaces(char * s);
//char * strtoupper(char * s);
//char * strlower(const char *s);

//int strint(char * what, char * from[]);

// read a big endian integer provided as char array
int arr2int(char *arr, int len);

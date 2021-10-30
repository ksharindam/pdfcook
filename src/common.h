#pragma once
/* This file is a part of pdfcook program, which is GNU GPLv2 licensed */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE// for asprintf()
#endif
#include "config.h"
#include <cstdio>
#include <cstring>// memcpy and other string func
#include <cassert>
#include <cmath>
//#include <cstdint> // uint32_t type
//#include <cctype> // toupper() isspace() etc

extern bool repair_mode;

typedef unsigned int uint;
// M_PI is not available in mingw32, so using and defining PI
#define PI 3.14159265358979323846

#if (!HAVE_ASPRINTF)
int asprintf(char **strp, const char *fmt, ...);
#endif

// check if string s1 starts with s2
#define starts(s1, s2)  (strncmp(s1,s2,strlen(s2)) == 0)

#define MAX(a,b) ((a)>(b) ? (a):(b))
#define MIN(a,b) ((a)<(b) ? (a):(b))

// read a big endian integer provided as char array
int arr2int(char *arr, int len);

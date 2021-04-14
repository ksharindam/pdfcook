#pragma once
/* This file is a part of pdfcook program, which is GNU GPLv2 licensed */
#include <iostream>
#include <cstdio>
#include <cassert>


extern int quiet_mode;

/* message types */
enum
{
    LOG,//only print message
    WARN,//print message with "warning : " prefix
    ERROR,//print message with "error : " prefix
    FATAL//print message with "error : " prefix, and exits program immediately
};

void message(int type, const char *format, ...);

// print message only when DEBUG is defined
void debug(const char *format, ...);

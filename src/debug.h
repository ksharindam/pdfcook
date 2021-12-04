#pragma once
/* This file is a part of pdfcook program, which is GNU GPLv2 licensed */
#include <iostream>
#include <cstdio>
#include <cassert>


extern int quiet_mode;

/* Here we use two debug functions, debug() and message().
   debug() is only for developer. message() is for user.
   for lower level errors or warnings that a normal user won't understand,
   debug() is used.
   for fatal errors and for user understandable errors message() is used.
*/

/* message types */
enum {
    LOG,//only print message
    WARN,//print message with "warning : " prefix
    ERROR,//print message with "error : " prefix
    FATAL//print message with "error : " prefix, and exits program immediately
};

void message(int type, const char *format, ...);

// print message only when DEBUG is defined
void debug(const char *format, ...);

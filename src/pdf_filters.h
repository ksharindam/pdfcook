#pragma once
/* This file is a part of pdfcook program, which is GNU GPLv2 licensed */
#include "common.h"
#include "pdf_objects.h"


int zlib_compress_filter(char **stream, size_t *len, DictObj &dict);
int flate_decode_filter(char **stream, size_t *len, DictObj &dict);


#if (HAVE_LZW)
    int lzw_decompress_filter(char **stream, size_t *len, DictObj &dict);
#else
    #define lzw_decompress_filter NULL
#endif

typedef struct {
    const char *name;
    int (*filter)(char **stream, size_t *len, DictObj &dict);
} stream_filters;

int apply_filter(const char *name, char **stream, size_t *len, DictObj &dict, stream_filters *filters, size_t f_len);
int apply_compress_filter(const char *name, char **stream, size_t *len, DictObj &dict);
int apply_decompress_filter(const char *name, char **stream, size_t *len, DictObj &dict);


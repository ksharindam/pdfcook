#ifndef _PDF_FILTERS_H_
#define _PDF_FILTERS_H_
#include "config.h"
#include "pdf_lib.h"

#ifdef HAVE_ZLIB
	int zlib_compress_filter(char ** stream, long  * len, pdf_object * dict);
	int flate_decode_filter(char ** stream, long  * len, pdf_object * dict);
#else
	#define zlib_compress_filter NULL
	#define flate_decode_filter NULL
#endif

#ifdef HAVE_LZW
	int lzw_decompress_filter(char ** stream, long  * len, pdf_object * dict);
	#define lzw_compress_filter NULL
#else
	#define lzw_compress_filter NULL
	#define lzw_decompress_filter NULL
#endif

typedef struct {
	char * name;
	int (*filter)(char ** stream, long * len, pdf_object *dict);
} stream_filters;

int apply_filter(char *name, char **stream, long  *len, pdf_object *dict, stream_filters *filters, size_t f_len);
int apply_compress_filter(char *name, char **stream, long  *len, pdf_object *dict);
int apply_decompress_filter(char *name, char **stream, long  *len, pdf_object *dict);

int decompress_stream_object(pdf_object *stream);

#endif

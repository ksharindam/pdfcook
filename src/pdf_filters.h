#ifndef _PDF_FILTERS_H_
#define _PDF_FILTERS_H_
#include "config.h"
#include "pdf_lib.h"

#ifdef HAVE_ZLIB
	int zlib_compress_filter(char ** stream, long  * len, pdf_object * dict);
	int zlib_decompress_filter(char ** stream, long  * len, pdf_object * dict);
#else
	#define zlib_compress_filter NULL
	#define zlib_decompress_filter NULL
#endif

#ifdef HAVE_LZW
	int lzw_decompress_filter(char ** stream, long  * len, pdf_object * dict);
	#define lzw_compress_filter NULL
#else
	#define lzw_compress_filter NULL
	#define lzw_decompress_filter NULL
#endif

struct stream_filters{
	char * name;
	int (*filter)(char ** stream, long * len, pdf_object * dict);
};

#endif

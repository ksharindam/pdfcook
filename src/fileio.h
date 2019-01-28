#ifndef _fileio_h

#define _fileio_h

#include "stdio.h"
/*#include "pdftype.h"*/

#define BUFSIZE 16384 /*velikost bufferu pro IO operace*/
	
#define myftell(f) ((f->pos)-((f->end) - (f->ptr)))
#define myfeof(f) (f->eof==EOF && (f->ptr==f->end))
#define mygetc(f) ((f->ptr < f->end) ? *(f->ptr)++:slow_mygetc(f))
#define myungetc(f) (f->ptr=((f->ptr)>(f->buf))?(f->ptr-1):(f->buf))


enum {UNDEF=0,CR,LF,CRLF};

extern char lend_str[4][3];

typedef struct myfile {
	FILE * f;
	unsigned char * buf;
	unsigned char * ptr;
	unsigned char * end;
	long pos;
	int eof;
	int column;
	int row;
	int lastc;
	int scratch;

}MYFILE;

/**
 * funkce pro otevreni soubru nazvu filename, 
 * mode "r"/"w"
 * */
MYFILE * myfopen(const char * filename,const char * mode); 

/**
 * funkce pro uzavreni souboru stream
 * */
int myfclose(MYFILE *stream); 

/**
 * funkce pro seekovani v souboru stream, o offset, 
 * origin stejne jako u fseek u stdio.h
 * */
long myfseek(MYFILE *stream,long offset,int origin); 

/**
 * nacte radek ze souboru f, vraci pozici v souboru pred prectenim radku,
 * pokud je parametr eoln ruzny od NULL, je mu nastavena hodnota konce radku
 * */

size_t myfsize(MYFILE * stream);

size_t myfread(void * where, size_t size,size_t nmemb,MYFILE * stream);
size_t myfrread(void * where, size_t size,size_t nmemb,MYFILE * stream);
size_t myfwrite(void * where, size_t size,size_t nmemb,MYFILE * stream);

long myfgets(char * string,int len,MYFILE * f,int * eoln);
int slow_mygetc(MYFILE * f);

/**
 * zapise na konec do souboru dst retezec co a doplni je 
 * dle konstanty prislusnym koncem radku dle lend
 * vraci 0 pri uspechu, pri neuspechu vraci EOF
 * */
int swrite (FILE * dst,char * co,size_t delka,int lend); 

/**
 * zapise vybrany blok ze souboru src na konec souboru dst
 * */
int bwrite(FILE * src,FILE * dst,long poz,size_t len); 

MYFILE * stropen(const char * str);
	
#endif

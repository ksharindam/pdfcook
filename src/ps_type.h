#ifndef _dsctype

#define _dsctype
#include "vdoc.h"

#define UNKNOW 0
#define ATEND -1
#define FLEN 256 /**<maxdelka jmena souboru*/
#define LLEN 256 /**<max delka radku*/
#define DSCLEN 256 /**<maximalni delka komentare*/
#define DSC_VERSION_LEN 256 /**<maximalni delka prvni radky v PS souboru */

#define tBLOK 0x00
#define tTEXT 0x01
#define tPSETUP 0x02
#define tPSHOW 0x04
#define tPEND 0x08
#define tBSETUP 0x10
#define tESETUP 0x20
#define tPTRAILER 0x40


typedef struct blok{
	struct blok * next; /**<ukazatel na nasledujici*/
	struct blok * last; /**<ukazatel na predchozi*/
	long begin; /**<end==0 delka retezce data, jinak pozice zacatku bloku v souboru*/
	long end; /**<pozice konce bloku v souboru*/
	int flag;
	char * data;
} BLOK;

/** \brief informace o hlavicce ps douboru*/
typedef struct header{
	BLOK  data; /**<ukazatel na BLOK dat v souboru*/
	long begin,end; /**<zacatek, konec podsekce "header" v souboru*/
	char version [DSC_VERSION_LEN]; /**<prvni radek dokumentu*/
} HEADER;

typedef struct section{
	BLOK  data; /**<ukazatel na BLOK dat v souboru*/
	long begin,end; /**<zacatek, konec podsekce "procdef" v souboru*/
} SECTION;

typedef struct prolog{
	BLOK  data; /**<ukazatel na BLOK dat v souboru*/
	long begin,end; /**<zacatek, konec podsekce "procdef" v souboru*/
	BLOK * add;
} PROLOG;

typedef struct pg{
	long begin; /**<end==0 delka retezce data, jinak pozice zacatku BLOKu v souboru*/
	long end; /**<pozice konce BLOKu v souboru*/
	BLOK  data; /**<spojovy seznam bloku stranky*/
	BLOK * psetup; /**<ukazatel na blok, kterym se upravuji souradnice stranky*/
	BLOK * pshow; /**<ukazatel na prikaz showpage (ktery se ke kazde strance prida)*/
	BLOK * pend; /**<ukazatel na konec poscriptu stranky, kam se prida grestore*/
	BLOK * bpsetup; /**<ukazatel na komentar %%BeginPageSetup, pokud v dokumentu existuje, jinak NULL*/
	BLOK * epsetup; /**<ukazatel na komentar %%EndPageSetup, pokud v dokumentu existuje, jinak NULL*/
	BLOK * ptrailer; /**<ukazatel na komentar %%PageTrailer, pokud v dokumentu existuje, jinak NULL*/
	int eoln; /*zakoncovac radky v danem dokumentu*/
} PAGE;

typedef struct dsc {
	HEADER  header;		/*Header*/
	SECTION defaults; /*Defaults*/
	PROLOG  prolog;		/*Procedure Definitions*/
	SECTION  docsetup;	/*Document Setup*/
	page_list_head * pages;		/*Pages spojovy seznam se strankama*/
	SECTION  doctrailer;	/*Document Trailer*/
	int ref_counter; /*Pocitadlo referenci na strukturu.*/
	int eoln; /*zakoncovac radky v danem dokumentu*/
} DSC;
	
#endif

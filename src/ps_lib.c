#define _GNU_SOURCE
#include "vdoc.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "ps_lib.h"

#include "fileio.h"
    
#define isDSC(s) (s[0]=='%' && s[1]=='%')

#define get_doc_info \
		/*zpracovani jednotlivych zajimavych polozek v hlavice traileru*/\
			case porder: /*PageOrder: ascendent|descendent ...*/\
				addblok(blok,lwpoz,*fpoz,NULL,0);\
				lwpoz=myftell(f);\
				doc->doc->order=strint(line+strlen(DSClib[dsc_code])+2,doc_p_order_str);\
				break;\
			case orientation_: /*Orientation: portrait|landscape*/\
				addblok(blok,lwpoz,*fpoz,NULL,0);\
				lwpoz=myftell(f);\
				doc->doc->orient=strint(line+strlen(DSClib[dsc_code])+2,doc_p_orientation_str);\
				doc->doc->orient=doc->doc->orient==-1?DOC_ORD_UNKNOWN:doc->doc->orient;\
				break;\
			case dpsizes: /*DocumentPaperizes: a4|letter ...*/\
				addblok(blok,lwpoz,*fpoz,NULL,0);\
				lwpoz=myftell(f);\
				doc_get_pformat_name_to_dimensions(line+strlen(DSClib[dsc_code])+2,&(doc->doc->paper));\
				break;\
			case hbbox:\
				addblok(blok,lwpoz,*fpoz,NULL,0);\
				lwpoz=myftell(f);\
				break;\
			case bbox: /*%BoundingBox: %d %d %d %d*/\
				addblok(blok,lwpoz,*fpoz,NULL,0);\
				lwpoz=myftell(f);\
				if (4!=sscanf(line+strlen(DSClib[dsc_code])+2,"%d %d %d %d",\
					&(doc->doc->bbox.left.x),\
					&(doc->doc->bbox.left.y),\
					&(doc->doc->bbox.right.x),\
					&(doc->doc->bbox.right.y)\
				))\
				{\
					(doc->doc->bbox.right.x)=0;\
					(doc->doc->bbox.right.y)=0;\
					(doc->doc->bbox.left.x)=0;\
					(doc->doc->bbox.left.y)=0;\
				}\
				break;\
/*			case requirements:\
				addblok(blok,lwpoz,*fpoz,NULL,0);\
				lwpoz=myftell(f);\
				break;*/\
			case pages:/*%Pages: %d*/\
				{\
				int pages_count;\
				addblok(blok,lwpoz,*fpoz,NULL,0);\
				lwpoz=myftell(f);\
				if (1!=sscanf(line+strlen(DSClib[dsc_code])+2,"%d", &pages_count)){\
					pages_count=0;\
				}\
				if ( doc->doc->pages_count &&  doc->doc->pages_count!=pages_count ){\
					perror("Pocet stranek v dokuentu je jiny  nez je uvedeno v %%Pages:\n");\
				}\
				else{\
					doc->doc->pages_count=pages_count;\
				}\
				}\
				break;\


#define get_page_info \
			case pbbox:\
				addblok(&(pom->data),lwpoz,*fpoz,NULL,0);\
				*fpoz=myfgets(line,LLEN,f,NULL);\
				lwpoz=*fpoz;\
				break;\
				if (4!=sscanf(line+strlen(DSClib[dsc_code])+2,"%d %d %d %d",\
								&(new_page->page->bbox.left.x),\
								&(new_page->page->bbox.left.y),\
								&(new_page->page->bbox.right.x),\
								&(new_page->page->bbox.right.y)\
							))\
							{\
								(new_page->page->bbox.right.x)=0;\
								(new_page->page->bbox.right.y)=0;\
								(new_page->page->bbox.left.x)=0;\
								(new_page->page->bbox.left.y)=0;\
							}\
				break;\

enum DSCintlib {ISDSC=-4,COMENT=-3,EMPTY=-2,MYPROCSET=1,porder,orientation_,porientation,dpsizes,bbox,hbbox,pages,endcomments,
	  bdefaults,edefaults,bprolog,bprocset,eprocset,bresource,eresource,
	  eprolog,bsetup,esetup,page,epgcomments,pbbox,bpsetup,epsetup,ptrailer,trailer,
	  bbinary,ebinary,bdata,edata,eof,bdoc,edoc,psize,requirements,SHOWPAGEB,SHOWPAGEE};
enum errors{critical=-1, normal=0};


/*pole se "zaznamy" DSC komentari*/
static char * DSClib[]={
			"(atend)",
		 	"BeginProcSet: PSTool",
			"PageOrder:",
			"Orientation:",
			"PageOrientation:",
			"DocumentPaperSizes:",
			"BoundingBox:",
			"HiResBoundingBox:",
			"Pages:",
			"EndComments",
			"BeginDefaults",
			"EndDefaults",
			"BeginProlog",
			"BeginProcSet:",
			"EndProcSet",
			"BeginResource:",
			"EndResource:",
			"EndProlog",
			"BeginSetup",
			"EndSetup",
			"Page:",
			"EndPageComments",
			"PageBoundingBox:",
			"BeginPageSetup",
			"EndPageSetup",
			"PageTrailer",
			"Trailer",
			"BeginBinary:",
			"EndBinary",
			"BeginData:",
			"EndData",
			"EOF",
			"BeginDocument:",
			"EndDocument",
			"PaperSize:",
			"Requirements:",
		 	"BEGIN: PSTOOL SHOWPAGE",
		 	"END: PSTOOL SHOWPAGE",
			0
};

/*<< /Duplex true >> setpagedevice %duplex delsi hrana
* << /Duplex true /Tumble true >> setpagedevice %duplex kratsi strana*/
/**definice prolugu, ktery se pridava na zacatek kazdeho upravovaneho dokumentu*/
char * MyProlog[]={
		 	"%%BeginProcSet: PSTool",
			"/pstshowpage /showpage load def",
			"/showpage {} def",
			"/pstrotate /rotate load def",
			"/pstscale /scale load def",
			"/pstrotate /rotate load def",
			"/psttranslate /translate load def",
			"/pstgsave /gsave load def",
			"/pstgrestore /grestore load def",
			"/pstmoveto /moveto load def",
			"/pstlineto /lineto load def",
			"/pststroke /stroke load def",
			"/pstnewpath /newpath load def",
			"/pstfindfont /findfont load def",
			"/pstscalefont /scalefont load def",
			"/pstsetfont /setfont load def",
			"/pstshow /show load def",
			"/pstrlineto /rlineto load def",
			"/pstconcat /concat load def",
			"/pstsetlinewidth /setlinewidth load def",
			"/pstclosepath /closepath load def",
			"/pstclip /clip load def",
			"[/letter/legal/executivepage/a4/a4small/b5/com10envelope",
			"/monarchenvelope/c5envelope/dlenvelope/lettersmall/note",
			"/folio/quarto/a5]{dup where{dup wcheck{exch{}put}",
		   	"{pop{}def}ifelse}{pop}ifelse}forall",
			"%%EndProcSet",
			0
};

/*deklarace fci*/
static int addblok(BLOK * head,long begin,long end,char * data,int flag);
/*static int insertblok(BLOK * where,int begin,int end,char * data,int flag);*/
static int listblok (BLOK * head, int (* todo) (BLOK * part,void * p1, void * p2),void * p1, void * p2);
static PAGE * ps_dsc_page_empty(void);


static int empty(char * s){
	return !*(skipwhspaces(s));
}/*END empty*/

/**
	\brief Funkce spoji stranky \a p1 a \a p2 do jedne
	\param p1 ukazatel na prvni stranku
	\param p2 ukazatel na druhou stranku
	\retval 0 vse probehlo v poradku
	\retval -1 nastala nejaka chyba
*/ 
int ps_dsc_2pages_to_one(PAGE * p1, PAGE * p2){
	p1->data.last->next=p2->data.next;
	p2->data.next->last=p1->data.last;
	p2->data.last->next=&(p1->data);
	p1->data.last=p2->data.last;

	if (p1->ptrailer){
		p1->ptrailer->last->next=p1->ptrailer->next;
		p1->ptrailer->next->last=p1->ptrailer->last;
		free(p1->ptrailer);
	}
	if (p2->bpsetup){
		p2->bpsetup->last->next=p2->bpsetup->next;
		p2->bpsetup->next->last=p2->bpsetup->last;
		free(p2->bpsetup);
	}
	if (p2->epsetup){
		p2->epsetup->last->next=p2->epsetup->next;
		p2->epsetup->next->last=p2->epsetup->last;
		free(p2->epsetup);
	}

	if (p2->psetup){
		p2->psetup->last->next = p2->psetup->next;
		p2->psetup->next->last = p2->psetup->last;
		free(p2->psetup);
	}

	p1->ptrailer=p2->ptrailer;

	/*opdstraneni pstshowpage z p1*/
	p1->pshow->last->next=p1->pshow->next;
	p1->pshow->next->last=p1->pshow->last;
	if (p1->pshow->data)
		free(p1->pshow->data);
	free(p1->pshow);
	/*END odstraneni pstshowpage z p1*/

    	/*opdstraneni pstshowpage z p1*/
	p1->pend->last->next=p1->pend->next;
	p1->pend->next->last=p1->pend->last;
	if (p1->pend->data)
		free(p1->pend->data);
	free(p1->pend);
	/*END odstraneni pstshowpage z p1*/
    

	p1->pshow=p2->pshow;
	p1->pend=p2->pend;
	memset(p2,0,sizeof(PAGE));
	p2->data.last=p2->data.next=&(p2->data);
	return 0;
}/*END mergepage()*/

/**
	\brief vytvori pole obsahujici pocdef, s odelovaci radku uvedenem v "lend"
*/ 
static int addMyProcdef(DSC * dsc){
	size_t i;
	char * procset;
	for (i=0;*(MyProlog + i);++i){
		if (asprintf(&procset,"%s%s",MyProlog[i],lend_str[dsc->eoln])<0 || procset==NULL){
			vdoc_errno=VDOC_ERR_LIBC;
			return -1;
		}
		addblok(&(dsc->prolog.data),0,0,procset,0);
		if (i==0){
			dsc->prolog.add=dsc->prolog.data.last;
		}
	}
	return 0;
}

static int addMyShowPage(PAGE * page, int lend){
	char * shw_page;
	if (page->pshow!=NULL){
		return 0;
	}
	if (asprintf(&shw_page,"%%%%%s%s%s%s%%%%%s%s",DSClib[SHOWPAGEB],lend_str[lend],
				"pstshowpage",lend_str[lend],
				 DSClib[SHOWPAGEE],lend_str[lend]
				)<0 || shw_page==NULL){
		vdoc_errno=VDOC_ERR_LIBC;
		return -1;
	}
		addblok(&(page->data),0,0,shw_page,tPSHOW);
		page->pshow=page->data.last;
	return 0;
}/*END createMyProcdef*/



/**
\brief funkce vraci konstantu odpovidajici retezci "what"
*/
static int decodeDSC (char * what){
	int i;
	if (isDSC(what)) 
	{
		for (i=1;*(DSClib+i);++i)
		{
			if (strstr(what+2,DSClib[i])==what+2)
				return i;
		}
		return ISDSC;
	}
	if (what[0]=='%')
		return COMENT;
	if (empty(what))
		return EMPTY;

	return 0;
}/*END decodeDSC()*/




/**
\brief funkce zapise ze souboru p1 do souboru p2 blok (viz struktura BLOK)
*/
static int  writeblok(BLOK * blok,void * p1,void * p2){
	size_t written;
	if (blok->data){
		written=fwrite(blok->data,sizeof(char),strlen(blok->data),(FILE *) p2);
		if (written==strlen(blok->data)){
			return 0;
		}
		else {
			vdoc_errno = VDOC_ERR_LIBC;
			return -1;
		}
	}
	else{
		return bwrite((FILE *) p1,(FILE *) p2,blok->begin,blok->end-blok->begin);
	}
}

static long putDefaults(DSC * dsc, FILE * in, FILE * out){
	return bwrite(in,out,dsc->defaults.begin,dsc->defaults.end-dsc->defaults.begin);
}

/**
\brief zapise Prolog ze soubru in do souboru out dle struktury dsc
*/
static long putProlog(DSC * dsc, FILE * in, FILE * out){
	return listblok((BLOK *)&(dsc->prolog.data),writeblok,in,out);
}

/**
\brief zapise DocumentSetup ze soubru in do souboru out dle struktury dsc
*/
static long putSetup(DSC * dsc, FILE * in, FILE * out){
	if (dsc->docsetup.data.next==dsc->docsetup.data.last){
		return bwrite(in,out,dsc->docsetup.begin,dsc->docsetup.end-dsc->docsetup.begin);
	}
	else {
		return listblok((BLOK *)&(dsc->docsetup.data),writeblok,in,out);
	}
}/*END putSetup()*/


/**
\brief zapise stranky ze soubru in do souboru out dle struktury dsc
vraci pocet zapsanych stranek
*/
static int putPages(page_list_head * p_doc, FILE * in, FILE * out){
	page_list * pg;
	page_handle * page_handle;
	int i;
	for (pg=page_begin(p_doc), pg=page_next(pg),i=1;pg && pg!=page_end(p_doc);pg=page_next(pg),++i){
		/*printf("[%d] ",i);*/
		page_handle=pg->page;
		if (fprintf(out,"%%%%Page: (%d) %d%s",i,i,lend_str[((PAGE *)(page_handle->page))->eoln])<0){
			vdoc_errno = VDOC_ERR_LIBC;
			return -1;
		}
		/*TODO: dopsat vypis dalsich informaci z hlavicky stranky do PS souboru*/
		if (listblok((BLOK *) &(((PAGE *)(page_handle->page))->data),writeblok,in,out)==-1){
			return -1;
		}
	}
	return 0;
}/*END putPages()*/


/**
\brief zapise trailer ze soubru in do souboru out dle struktury dsc
*/
static long putTrailer(DSC * dsc, FILE * in, FILE * out){
	if  ( listblok((BLOK *)&(dsc->doctrailer.data),writeblok,in,out)==-1 ||  fprintf(out,"%%%%EOF%s",lend_str[dsc->eoln])<0){
		fclose(out);
		fclose(in);
		vdoc_errno = VDOC_ERR_LIBC;
		return -1;
	}
	return 0;
}/*END putTrailet()*/

/**
\brief zapise Header ze soubru in do souboru out dle struktury dsc
*/
static long putHeader(page_list_head * p_doc, FILE * in, FILE * out){
	DSC * dsc = (DSC *) p_doc->doc->doc;
	if (fprintf(out,"%s%s",dsc->header.version,lend_str[dsc->eoln])<0){
		vdoc_errno = VDOC_ERR_LIBC;
		return -1;
	}
	/*
	if (p_doc->doc->order!=DOC_ORD_UNKNOWN){
		if (fprintf(out,"%%%%PageOrder: %s%s",doc_p_order_str[p_doc->doc->order],lend_str[dsc->eoln])<0){
			vdoc_errno = VDOC_ERR_LIBC;
			return -1;
		}
	}
	*/

	if (p_doc->doc->orient!=DOC_O_UNKNOWN){
		p_doc->doc->orient=p_doc->doc->orient==DOC_O_SEASCAPE?DOC_O_LANDSCAPE:p_doc->doc->orient;
		p_doc->doc->orient=p_doc->doc->orient==DOC_O_UPSIDE_DOWN?DOC_O_PORTRAIT:p_doc->doc->orient;
		if (!fprintf(out,"%%%%Orientation: %s%s",doc_p_orientation_str[p_doc->doc->orient],lend_str[dsc->eoln])<0){ 
			vdoc_errno = VDOC_ERR_LIBC;
			return -1;
		}
	}
	if (doc_get_pformat_name(&(p_doc->doc->paper))!=-1){
		if (!fprintf(out,"%%%%DocumentPaperSizes: %s%s",dimensions_str(doc_get_pformat_name(&(p_doc->doc->paper))),lend_str[dsc->eoln])){
			vdoc_errno = VDOC_ERR_LIBC;
			return -1;
		}	
	}
	if (p_doc->doc->bbox.right.x 
	    	|| p_doc->doc->bbox.right.y 
		|| p_doc->doc->bbox.left.x
		|| p_doc->doc->bbox.left.y
	   )
	{
	   	if (!fprintf(out,"%%%%BoundingBox: %d %d %d %d%s",
			     p_doc->doc->bbox.left.x,p_doc->doc->bbox.left.y,
			     p_doc->doc->bbox.right.x,p_doc->doc->bbox.right.y,
			     lend_str[dsc->eoln])<0
		    )
		{
			vdoc_errno = VDOC_ERR_LIBC;
			return -1;
		}
	}
	
	if (fprintf(out,"%%%%Pages: %d%s",p_doc->doc->pages_count,lend_str[dsc->eoln])<0){
		vdoc_errno = VDOC_ERR_LIBC;
		return -1;
	}

	#if 0
	if (listblok((BLOK *)&(dsc->header.data),writeblok,in,out)<0){
		return -1;
	#endif

	if (fprintf(out,"%%%%EndComments%s",lend_str[dsc->eoln])<0){
			vdoc_errno = VDOC_ERR_LIBC;
			return -1;
	}

	return 0;

}


/**
\brief ulozi zmeny v dokumentu p_doc do souboru se jmenem fnameout
*/
int ps_dsc_save (page_list_head * p_doc, const char * fnameout){
	DSC * dsc = (DSC *) p_doc->doc->doc;
	FILE * in;
	FILE * out;

	if ((in = fopen(p_doc->doc->file_name,"rb"))==NULL){
		return -1;
		vdoc_errno = VDOC_ERR_LIBC;
	}

	if ((out = fopen(fnameout,"wb"))==NULL){
		fclose(in);
		return -1;
		vdoc_errno = VDOC_ERR_LIBC;
	}

	if (putHeader(p_doc,in,out)==-1 || putDefaults(dsc,in,out)==-1
	     || putProlog(dsc,in,out)==-1 || putSetup(dsc,in,out)==-1
	     || putPages(p_doc,in,out)==-1 || putTrailer(dsc,in,out)==-1)
	{
		fclose(out);
		fclose(in);
		return -1;
	}

	

	
	if (fclose(in)){
		fclose(out);
		vdoc_errno = VDOC_ERR_LIBC;
		return -1;
	}

	if (fclose(out)){
		vdoc_errno = VDOC_ERR_LIBC;
		return -1;
	}

	return 0;
}
/**
\brief nacte hlavicku ps-filu 
*/
static int getHeader(page_list_head * doc,MYFILE * f,char * iobuffer, long *fpoz)
{
	char * line = iobuffer;
	long lwpoz;
	int dsc_code;
	DSC * dsc  = (DSC *) doc->doc->doc;
	BLOK * blok =  &(dsc->header.data);
	/*skip lines without header signature*/
	while ((*fpoz=myfgets(line,LLEN,f,&(dsc->eoln))!=EOF) && !starts(line,"%!PS") && !starts(line,"%PDF-"))
	;
	
	if (*fpoz==-1 || starts(line,"%PDF-")){
		return -1;
	}
	strncpy(dsc->header.version,line,DSC_VERSION_LEN);
	dsc->header.begin=*fpoz;
	
	*fpoz=lwpoz=myfgets(line,LLEN,f,NULL);
	while (*fpoz!=-1){
		switch (dsc_code=decodeDSC(line)){
			/*komentar, nebo neznamy DSC komentar*/
			case ISDSC:
			case COMENT:
				break;
			/*konec hlavicky ps-souboru*/
			case EMPTY:
			case endcomments:
				*fpoz=myfgets(line,LLEN,f,NULL);
			case MYPROCSET:
			case bprocset:
			case bprolog:
			case bdefaults: 
			case 0:
				addblok(&(dsc->header.data),lwpoz,*fpoz,NULL,0);
				if (doc->doc->paper.right.x==0 || doc->doc->paper.right.y==0){
					doc->doc->paper.right.x=doc->doc->bbox.right.x;
					doc->doc->paper.right.y=doc->doc->bbox.right.y;
				}
				return 0;
				break;
			/*konec ps-souboru po hlavicce*/
			case eof:
				goto wh_end;
			get_doc_info
			}
		*fpoz=myfgets(line,LLEN,f,NULL);
	}
wh_end:
	/*neocekavany konec souboru*/
	return -1;
}
/**
\brief nacte sekci defaults ps-filu
*/
static int getDefaults(page_list_head * doc,MYFILE * f,char * iobuffer,long * fpoz){
	int t;
	char * line = iobuffer;
	DSC * dsc = (DSC *)  doc->doc->doc;
	dsc->defaults.begin=*fpoz;
	/*preskocime prazdne radky*/
	while (EOF!=*fpoz && (EMPTY==(t=decodeDSC(line)))){
		*fpoz=myfgets(line,LLEN,f,NULL);
	}
	/*konec souboru*/
	if (*fpoz==EOF){ 		
		return -1;
	}

	/*v dokumentu se nenachazi sekce defaults*/
	if (t!=bdefaults){ 		
		dsc->defaults.end=dsc->defaults.begin;
		return 0;
	}
	/*nalezneme endidefaults*/
	while (((*fpoz=myfgets(line,LLEN,f,NULL))!=EOF) && *fpoz!=EOF && ((t=decodeDSC(line))!=edefaults) && (t!=eof));
		dsc->defaults.end=*fpoz;

	if (t==edefaults){
		*fpoz=myfgets(line,LLEN,f,NULL);
		return 0;
	}
	
	return -1;
}/*END getDefaults*/

/**
\brief nacte sekci prolog ps-filu
*/
static int getProlog(page_list_head * doc,MYFILE * f,char * iobuffer, long * fpoz)
{
	int lwpoz;
	int t;
	char * line = iobuffer;
	DSC * dsc = (DSC *)  doc->doc->doc;
	
	*fpoz=lwpoz=dsc->prolog.begin=*fpoz;

	while (*fpoz!=EOF && ((EMPTY==(t=decodeDSC(line))) || COMENT==t)
			&& t!=bprocset && t!=bresource && t!=eof && t!=page
			&& t!=MYPROCSET)
	{
		*fpoz=myfgets(line,LLEN,f,NULL);
	}
	if (*fpoz==EOF){
		return -1;
	}
		
	switch (t)
	{
		case eof:
			dsc->prolog.end=*fpoz;
			return 0; /*neocekavany konec souboru*/
		case MYPROCSET:
			addblok(&(dsc->prolog.data),lwpoz,*fpoz,NULL,0);
			/*najit EndProcset*/
			do{
				*fpoz=myfgets(line,LLEN,f,NULL);
				t=decodeDSC(line);
			}
			while (*fpoz!=EOF && t!=eprocset);
			
			if (t==eprocset){
				*fpoz=myfgets(line,LLEN,f,NULL);
				t=decodeDSC(line);
			}
			else{
				return -1; /*neocekavany konec souboru*/
			}
			lwpoz=*fpoz;
			break;
		case bprocset:
		default:		
			addblok(&(dsc->prolog.data),lwpoz,*fpoz,NULL,0);
			lwpoz=*fpoz;
	}
	/*misto, kam se zapise nas procset*/
	if (addMyProcdef(dsc)==-1){
		return -1;
	}
	
	while (*fpoz!=EOF && (t=decodeDSC(line))!=eprolog 
		   && t!=bsetup && t!=page )
	{
		*fpoz=myfgets(line,LLEN,f,NULL);
	}

	/*neocekavany konec souboru*/
	if (*fpoz==EOF || t==eof)
		return -1;
	
	if (t==eprolog)
	{
		*fpoz=myfgets(line,LLEN,f,NULL);
	}
	
	addblok(&(dsc->prolog.data),lwpoz,*fpoz,NULL,0);
	dsc->prolog.end=*fpoz;
	return 0;
}

/**
\brief nacte sekci setup ps-filu
*/
static int getSetup(page_list_head * doc,MYFILE * f,char * iobuffer,long * fpoz)
{
	char * line = iobuffer;
	int t;
	long lpoz;
	DSC * dsc = (DSC *)  doc->doc->doc;
    /*vzhledem k tomu ze v nekterych PS souborech
	vyskytuje za EndSetup nejaky PS kod, pred %%Page:,
	proto budu vse mezi koncem prologu a dsc %%Page: povazovat
	jako DocumentSetup.
	*/
	dsc->docsetup.begin=*fpoz;
	lpoz=*fpoz;
	t=decodeDSC(line);
	while (*fpoz!=EOF && page!=(t=decodeDSC(line)) && t!=eof)
	{	  
		if (t==psize){
			addblok(&(dsc->docsetup.data),lpoz,*fpoz,NULL,0);
			lpoz=myftell(f);
			doc_get_pformat_name_to_dimensions(line+2+strlen(DSClib[t]),&(doc->doc->paper));
		}
		*fpoz=myfgets(line,LLEN,f,NULL);
	}
	addblok(&(dsc->docsetup.data),lpoz,(*fpoz),NULL,0);
	dsc->docsetup.end=*fpoz;

	if (t==page){
		return 0;
	}
	return -1;
}/*END getSetup*/

/**
\brief precte nazev stranky z DSC komentare
*/
static void getPname(char * kam, const char * odkud){
	int i=0,j=0;

	while (odkud[i] && odkud[i]!=':')
		++i;
	++i;
	while (odkud[i] && isspace((int)(odkud[i])))
		++i;

	if (odkud[i]=='('){
		++i;
		while (odkud[i] && odkud[i]!=')'){
			kam[j]=odkud[i];
			++i;++j;
		}
		kam[j]=0;
	}
	else {
		while (odkud[i] && !isspace((int)(odkud[i]))){
			kam[j]=odkud[i];
			++i;++j;
		}
		kam[j]=0;
		
	}
}/*END getPname()*/

/**
\brief preskoci oblast ohranicenou  %%BeginDocument %%EndDocument
*/
static int skipEPS(PAGE * pg,MYFILE *f,char * iobuffer,long * fpoz){
	int doc=1;
	int t;
	long lpoz=*fpoz;
	char * line = iobuffer;

	while ((*fpoz)!=EOF && doc){
		(*fpoz)=myfgets(line,LLEN,f,NULL);
		t=decodeDSC(line);
		if (t==bdoc)
			++doc;
		if (t==edoc)
			--doc;
	}
	(*fpoz)=myfgets(line,LLEN,f,NULL);
	addblok(&(pg->data),lpoz,(*fpoz),NULL,0);
	return 0;
}/*END skipEPS()*/


/**
\brief dsc ukazatel na strukturu celeho dokumentu, f vsupni soubor, buffer s posledne nactenym radkem
lpoz naposledy ulozena pozice do souboru,fpoz aktualni pozice v souboru*/
static int getPage(page_list_head * p_doc,MYFILE * f,char * iobuffer,long * fpoz)
{ /*nacte jednu stranku do "pameti"*/
	int t;
	long lwpoz; /*posledni zapsana pozice, aktualni pozice v souboru*/
	int dsc_code;
	char * line=iobuffer;
	char * out_line;
	PAGE * pom;
	page_list * new_page;
	t=decodeDSC(line);
	/*preskocit praznde radky ...*/
	while (*fpoz!=EOF && (t=decodeDSC(line))==EMPTY){
		*fpoz=myfgets(line,LLEN,f,NULL);
	}
	if (*fpoz==EOF ||  t!=page 
	   || ((new_page=page_new(NULL,0)) == NULL)
	   )
	{
		return 1;
	}

	new_page->page->type=p_doc->doc->type;
	new_page->page->page=ps_dsc_page_empty();
	pages_list_add_page(p_doc,new_page,pg_add_end);
	pom=(PAGE *)new_page->page->page;
	getPname(new_page->page->name,line); /*precteni nazvu stranky*/
	*fpoz=lwpoz=pom->begin=myfgets(line,LLEN,f,NULL);

	new_page->page->orient=p_doc->doc->orient;
	memcpy(&(new_page->page->bbox),&(p_doc->doc->bbox),sizeof(dimensions));
	memcpy(&(new_page->page->paper),&(p_doc->doc->paper),sizeof(dimensions));
	
	/*preskocit souvislou oblast komentaru*/
	while ( *fpoz!=EOF &&  (t=decodeDSC(line)) 
		   && t!=page && t!=eof && t!=ptrailer 
		   && t!=trailer && t!=bdoc)
	{	
		t=decodeDSC(line);
		dsc_code=t;
		switch(t){
			get_page_info
			case eof:
				/*neocekevany komentar EOF*/
				return -1;
			case bpsetup:
				addblok(&(pom->data),lwpoz,*fpoz,NULL,0);
				lwpoz=*fpoz;
				*fpoz=myfgets(line,LLEN,f,NULL);
				addblok(&(pom->data),lwpoz,*fpoz,NULL,tBSETUP);
				pom->bpsetup=pom->data.last;
				lwpoz=*fpoz;
				goto end_wh;
			case page:
			case ptrailer:
			case bdoc:
				*fpoz=myfgets(line,LLEN,f,NULL);
			case SHOWPAGEB: /*vynechani SHOWPAGE, pokud se v dokumentu vyskytuje*/
			case 0:
				goto end_wh;
			
		}
		*fpoz=myfgets(line,LLEN,f,NULL);
	}
end_wh:
	if (*fpoz==EOF){
		return -1;
	}
	addblok(&(pom->data),lwpoz,*fpoz,NULL,0);
	lwpoz=*fpoz;
	if (asprintf(&out_line,"%s%s","pstgsave",lend_str[((DSC *)(p_doc->doc->doc))->eoln])<0 || out_line==NULL){
		vdoc_errno=VDOC_ERR_LIBC;
	}

	addblok(&(pom->data),0,0,NULL,tPSETUP); /*misto, kam bude PS-tool pripisovat neco na zacatek stranky*/
	pom->psetup=pom->data.last; /*nastavit ukazatel na oblast, kam se bude pridavat transformaci matice ...*/
	addblok(&(pom->data),0,0,out_line,0);
	do{
		switch(t){
			case bdoc:/*vlozeny dokument, nebude se parsovat*/
				addblok(&(pom->data),lwpoz,*fpoz,NULL,0);
				skipEPS(pom,f,line,fpoz);
				lwpoz=*fpoz;
			break;
			case epsetup: /*v souboru se nachazi komentar %%EndPageSetup*/
				addblok(&(pom->data),lwpoz,*fpoz,NULL,0);
				lwpoz=*fpoz;
				*fpoz=myfgets(line,LLEN,f,NULL);
				addblok(&(pom->data),lwpoz,*fpoz,NULL,tESETUP);
				lwpoz=*fpoz;
				pom->epsetup=pom->data.last;
			break;
			
			case SHOWPAGEB: /*vynechani SHOWPAGE, pokud se v dokumentu vyskytuje*/
				addblok(&(pom->data),lwpoz,*fpoz,NULL,0);
				while((*fpoz=myfgets(line,LLEN,f,NULL))!=EOF && (t=decodeDSC(line))!=SHOWPAGEE);
				if (*fpoz==EOF){
					return -1;
				}
				*fpoz=myfgets(line,LLEN,f,NULL);
				t=decodeDSC(line);
				lwpoz=*fpoz;
			case ptrailer:
				addblok(&(pom->data),lwpoz,*fpoz,NULL,0);
				lwpoz=*fpoz;
				addMyShowPage(pom,((DSC*)(p_doc->doc->doc))->eoln);
				*fpoz=myfgets(line,LLEN,f,NULL);
				t=decodeDSC(line);
				addblok(&(pom->data),lwpoz,*fpoz,NULL,tPTRAILER);
				lwpoz=*fpoz;
				pom->ptrailer=pom->data.last;
				goto out;
			case page:
			case trailer:
			case eof:
				goto end_do_while;
			default:
				*fpoz=myfgets(line,LLEN,f,NULL);
			break;
		}
		t=decodeDSC(line);
	}
	while(*fpoz!=EOF);
end_do_while:
	addblok(&(pom->data),lwpoz,*fpoz,NULL,0);
	lwpoz=*fpoz;
	if (*fpoz==EOF){
		return -1;
	}
	/*misto pro vlozeni nove showpage*/
	addMyShowPage(pom,((DSC*)(p_doc->doc->doc))->eoln);
	
out:
	while(*fpoz!=EOF && t!=page && t!=trailer && t!=eof){
		dsc_code=t;
		switch (t){
			get_page_info
		}
		*fpoz=myfgets(line,LLEN,f,NULL);
		t=decodeDSC(line);
	}

	addblok(&(pom->data),lwpoz,*fpoz,NULL,0);
	lwpoz=*fpoz;
	/*sekce pro umisteni GRESTORE*/
	if (asprintf(&out_line,"%s%s","pstgrestore",lend_str[((DSC *)(p_doc->doc->doc))->eoln])<0 || out_line==NULL){
		vdoc_errno=VDOC_ERR_LIBC;
		return -1;
	}
	addblok(&(pom->data),0,0,NULL,tPEND);
	pom->pend=pom->data.last;
	addblok(&(pom->data),0,0,out_line,0);
	pom->end=*fpoz;
	return 0;
}/*EDN getPage*/

/**
\brief nacte jednotlive stranky z psfilu
*/
static int getPages(page_list_head * doc,MYFILE * f,char * iobuffer, long * fpoz)
{
	char * line = iobuffer;
	int pages_count=doc->doc->pages_count;
	int ret_val;
	doc->doc->pages_count=0;
	while ((ret_val=getPage(doc,f,line,fpoz))==0);

	if (pages_count && doc->doc->pages_count!=pages_count){
		perror("Pocet stranek v dokuentu je jiny  nez je uvedeno v %%Pages:\n");
	}
	
	return ret_val==1?0:-1;
}
/**
\brief nacte sekci trailer z psfilu
*/
static int getTrailer(page_list_head * doc,MYFILE * f,char * iobuffer, long * fpoz){
	DSC * dsc = (DSC *)  doc->doc->doc;
	BLOK * blok =  &(dsc->doctrailer.data);
	char * line = iobuffer;
	long lwpoz;
	int dsc_code;
	if (*fpoz==EOF || (dsc_code=decodeDSC(line))!=trailer || dsc_code==eof){
		dsc->doctrailer.begin=dsc->doctrailer.end=0;
		return 0;
	}
	dsc->doctrailer.begin=lwpoz=*fpoz;
	while ((*fpoz=myfgets(line,LLEN,f,NULL))!=EOF){
		switch(dsc_code=decodeDSC(line)){
			case eof:
				goto wh_end;
			get_doc_info
		default:
				break;

		}
	}/*END while()*/
wh_end:
	addblok(&(dsc->doctrailer.data),lwpoz,*fpoz,NULL,0);
	return 0;
}/*END getTrailer()*/




/*vytvori praznou DSC strukturu*/
DSC * ps_dsc_structure_new(DSC * dsc_old){
	DSC * dsc;
	/*kopirovat dsc strukturu nema smysl, pouze zvedneme referenci ...*/
	if (dsc_old){
		dsc_old->ref_counter+=1;
		return dsc_old;
	}

	dsc=(DSC *) malloc(sizeof(DSC));
        if (dsc){
	        memset(dsc,0,sizeof(DSC));
		dsc->ref_counter=1;
		dsc->header.data.last=dsc->header.data.next = &(dsc->header.data);
		dsc->defaults.data.last=dsc->defaults.data.next = &(dsc->defaults.data);
	        dsc->prolog.data.last=dsc->prolog.data.next = &(dsc->prolog.data);
	        dsc->docsetup.data.last=dsc->docsetup.data.next = &(dsc->docsetup.data);
	        dsc->doctrailer.data.last=dsc->doctrailer.data.next = &(dsc->doctrailer.data);
	}
	return dsc;
}

int ps_dsc_open(page_list_head * doc, const char * fname){
	MYFILE * f;
	char iobuffer[LLEN];
	long fpoz=0;
	DSC * dsc = ps_dsc_structure_new(NULL);
	page_list * page;
	if (dsc==NULL){
		return -1;
	}
	doc->doc->doc=dsc;	
    	/*soubor se nepodarilo otevrit pro cteni*/
	if ((f=myfopen(fname,"rb"))==NULL){
		ps_dsc_structure_delete(dsc);
		return -1; 	
	}
	strncpy(doc->doc->file_name,fname,PATH_MAX);

	if (getHeader(doc,f,iobuffer,&fpoz)==-1){
		myfclose(f); /*uzavreme vstupni soubor*/
		ps_dsc_structure_delete(dsc);
		return -1;
	}
	if (    getDefaults(doc,f,iobuffer,&fpoz)==-1
		|| getProlog(doc,f,iobuffer,&fpoz)==-1
		|| getSetup(doc,f,iobuffer,&fpoz)==-1
		|| getPages(doc,f,iobuffer,&fpoz)==-1
		|| getTrailer(doc,f,iobuffer,&fpoz)==-1
	)
	{
		myfclose(f); /*uzavreme vstupni soubor*/
		ps_dsc_structure_delete(dsc);
		message(FATAL,"Something is wrong in PostScrip file.\n");
		return -1;
	}
	myfclose(f); /*uzavreme vstupni soubor*/
	if (!isdimzero(doc->doc->paper) && isdimzero(doc->doc->bbox)){
		copy_dimensions(&(doc->doc->bbox),&(doc->doc->paper));
	}

	if (!isdimzero(doc->doc->bbox) && isdimzero(doc->doc->paper)){
		copy_dimensions(&(doc->doc->paper),&(doc->doc->bbox));
	}

	for(page=page_next(page_begin(doc));page!=page_end(doc);page=page_next(page)){
		if (!isdimzero(doc->doc->bbox) && isdimzero(page->page->bbox)){
			copy_dimensions(&(page->page->bbox),&(doc->doc->bbox));
		}

		if (!isdimzero(doc->doc->paper) && isdimzero(page->page->paper)){
			copy_dimensions(&(page->page->paper),&(doc->doc->paper));
		}


#if 0
		printf("<e>%d %d %d %d\n",page->page->paper.left.x,page->page->paper.left.y,page->page->paper.right.x,page->page->paper.right.y);
		printf("<e>%d %d %d %d\n",page->page->bbox.left.x,page->page->bbox.left.y,page->page->bbox.right.x,page->page->bbox.right.y);
#endif
	}
			
	return 0;
}/*END readpsfile()*/

/**
\brief odstrani zrusi spojovy seznam*/
static void freeblok(BLOK * blok){
	BLOK * pom=blok->next;
	do{	
		pom=pom->next;
		if (pom->last!=blok){
			if (pom->last->data)
				free(pom->last->data);
			free(pom->last);
		}
	}while (pom!=blok);
	
}/*END freeblok()*/

/**
\brief uvolni datovou strukturu DSC z pameti*/
int ps_dsc_structure_delete(DSC * dsc){
	if (dsc==NULL)
		return 0;
	if (dsc->ref_counter>1){
		dsc->ref_counter-=1;
		return 0;
	}
	freeblok(&(dsc->header.data));
	freeblok(&(dsc->defaults.data));
	freeblok(&(dsc->docsetup.data));
	freeblok(&(dsc->doctrailer.data));
	freeblok(&(dsc->prolog.data));
	free(dsc);
	return 0;
}/*END freedsc()*/

/**
\brief stranku page ze spojoveho seznamu*/
int ps_dsc_page_delete(PAGE * page){
	if (page==NULL){
		return 0;
	}
	freeblok(&((page)->data));
	free(page);
	return 0;

}/*END deletepage()*/

/*
\brief vytvori prototip prazdne stranky, tj. nova stranka + inicializace spojaku
*/
static PAGE * ps_dsc_page_empty(void)
{   PAGE * pom;
	pom =(PAGE *)malloc(sizeof(PAGE));
	if (pom==NULL)
		return NULL;
	memset(pom,0,sizeof(PAGE));
	pom->data.last=pom->data.next=&(pom->data);
	return pom;
}
/**
\brief vytvori novou stranku, pokud parametr what ukazuje na nejakou, tak ji zkopiruje, lend konec radky
*/
PAGE * ps_dsc_page_new(const PAGE * what){
	PAGE * pom;
	BLOK * blok;
	char * retezec;
	pom=ps_dsc_page_empty();
	if (pom==NULL)
		return NULL;
	if (what){
		pom->begin=what->begin;
		pom->end=what->end;
		blok=what->data.next;
		pom->psetup=NULL;
		pom->pshow=NULL;
		pom->bpsetup=NULL;
		pom->epsetup=NULL;
		pom->ptrailer=NULL;
		while (blok!=&(what->data)){
			if (blok->data){
				retezec=(char *)malloc(sizeof(char) * (strlen(blok->data)+1));
				if (retezec==NULL){
					free(pom);
					return NULL;
				}
				strncpy(retezec,blok->data,strlen(blok->data) + 1);
			}
			else{
				
				retezec=NULL;
			}
			addblok(&(pom->data),blok->begin,blok->end,retezec,blok->flag);
			switch (blok->flag & 0x7f){
				case tPSETUP:
					pom->psetup=pom->data.last;					
					break;
				case tPSHOW:
					pom->pshow=pom->data.last;
					break;
				case tPEND:
					pom->pend=pom->data.last;
					break;
				case tBSETUP:
					pom->bpsetup=pom->data.last;
					break;
				case tESETUP:
					pom->epsetup=pom->data.last;
					break;
				case tPTRAILER:
					pom->ptrailer=pom->data.last;
					break;
			}
			blok=blok->next;
		}
		

	}
	else{
		char * out_line;
		pom->begin=0;
		pom->end=0;
		pom->bpsetup=pom->epsetup=pom->ptrailer=NULL;
		if (asprintf(&out_line,"%s%s","pstgsave",lend_str[LF])<0 || out_line==NULL){
			vdoc_errno=VDOC_ERR_LIBC;
		}
		addblok(&(pom->data),0,0,NULL,tPSETUP);
		pom->psetup=pom->data.last;
		addblok(&(pom->data),0,0,out_line,0);

		if (asprintf(&out_line,"%s%s","pstgrestore",lend_str[LF])<0 || out_line==NULL){
			vdoc_errno=VDOC_ERR_LIBC;
		}
		addblok(&(pom->data),0,0,out_line,0);
		addblok(&(pom->data),0,0,NULL,tPEND);
		pom->pend=pom->data.last;
		addMyShowPage(pom,LF);
	}
	return pom;

}
/**
\brief prida na konec prvek do spojoveho seznamu
*/
static int addblok(BLOK * head,long begin,long end,char * data,int flag){
	
	BLOK * pom;
	/*blok s nulovou, nebo zapornou velikosti*/
	if (begin>=end && begin!=0)
		return 0;
	
	pom = (BLOK *) malloc(sizeof(BLOK));
	/*neuspesna alokace pameti*/
	if (pom==NULL)
		return -1;
#ifdef __DEBUG
	printf("::addblok() %ld  %ld\n",begin,end);
#endif

	pom->begin=begin;
	pom->end=end;
	pom->data=data;
	pom->flag=flag;

	pom->next=head;
	pom->last=head->last;
	head->last=pom;
	(pom->last)->next=pom;
    
	return 0;
}/*END addblok()*/

/**
\brief prida za where prvek do spojoveho seznamu
*/
/*
static int insertblok(BLOK * where,int begin,int end,char * data,int flag){
	
	BLOK * pom = (BLOK *) malloc(sizeof(BLOK));
	
	if (pom==NULL)
		return 0;


	pom->begin=begin;
	pom->end=end;
	pom->data=data;
	pom->flag=flag;

	pom->next=where->next;
	pom->last=where;
	where->next=pom;
	(pom->next)->last=pom;
    
	return 1;

}*//*EDN insertblok()*/


/**
\brief projde spojovy seznam a na kazdem prvku zavola fci ...
*/
static int listblok (BLOK * head,  int(* todo) (BLOK * part,void * p1, void * p2),void * p1, void * p2){
	BLOK * pom = head->next;

	while (pom!=head){
		if (todo(pom,p1,p2)==-1)
			return -1;
		pom=pom->next;
	}
	return 0;
}

int ps_dsc_draw_to_page_line(page_handle * pg_handle, const coordinate * begin, const coordinate * end,int l_width)
{
	char * line;
	char * eoln = lend_str[((PAGE *)(pg_handle->page))->eoln];
	PAGE * page = (PAGE *) (pg_handle->page);
	if (asprintf(&line,"pstgsave %d pstsetlinewidth pstnewpath %d %d pstmoveto%s %d %d pstlineto pststroke pstgrestore%s",l_width,begin->x,begin->y,eoln,
				end->x,end->y,eoln)<0
	||  line==NULL){ 
		vdoc_errno = VDOC_ERR_LIBC;
		return -1;
	}
	addblok(page->psetup->next,0,0,line,0);
	return 0;
}

int ps_dsc_draw_to_page_text(page_handle * pg_handle, const coordinate * where, int size, const char * font, const char * text)
{
	char * line;
	char * eoln = lend_str[((PAGE *)(pg_handle->page))->eoln];
	PAGE * page = (PAGE *) (pg_handle->page);
	font=(font==NULL)?"Times-Roman":font;
	if (asprintf(&line,"pstgsave /%s pstfindfont %d pstscalefont  pstsetfont pstnewpath  %s  %d %d pstmoveto %s(%s)%s pstshow pstgrestore%s",
		font,size,eoln,where->x,where->y,eoln,text,eoln,eoln)<0
	||  line==NULL){ /*nepodarena alokace pameti*/
		vdoc_errno = VDOC_ERR_LIBC;
		return -1;
	}
	addblok(page->psetup->next,0,0,line,0);
	return 0;
}
int ps_dsc_page_transform(page_handle * pg_handle, transform_matrix * matrix){
	char * line;
	char * eoln = lend_str[((PAGE *)(pg_handle->page))->eoln];
	PAGE * page = (PAGE *) (pg_handle->page);
	if (asprintf(&line,"pstgsave  [%.2f %.2f %.2f %.2f %.2f %.2f] pstconcat%s",   (*matrix)[0][0], (*matrix)[0][1], 
								 (*matrix)[1][0], (*matrix)[1][1], 
								 (*matrix)[2][0], (*matrix)[2][1],
								 eoln)<0
	|| line==NULL){ 
		vdoc_errno = VDOC_ERR_LIBC;
		return -1;
	}

	addblok(page->psetup->next,0,0,line,0);
	if (asprintf(&line,"pstgrestore%s",eoln)<0
	|| line==NULL){ 
		vdoc_errno = VDOC_ERR_LIBC;
		return -1;
	}
	addblok(page->pend,0,0,line,0);
	return 0;
}

int ps_dsc_set_crop_page(page_handle * pg_handle, dimensions * dimensions){
	char * line;
	char * eoln = lend_str[((PAGE *)(pg_handle->page))->eoln];
	PAGE * page = (PAGE *) (pg_handle->page);
	if (asprintf(&line,"pstgsave pstnewpath %d %d pstmoveto %d %d lineto %s"
			   "%d %d lineto %d %d lineto pstclosepath pstclip%s",
			   					 dimensions->left.x,dimensions->left.y,
								 dimensions->right.x,dimensions->left.y,
								 eoln,
								 dimensions->right.x,dimensions->right.y,
								 dimensions->left.x,dimensions->right.y,
								 eoln)<0
	|| line==NULL){ 
		vdoc_errno = VDOC_ERR_LIBC;
		return -1;
	}
	addblok(page->psetup->next,0,0,line,0);

	if (asprintf(&line,"pstgrestore%s",eoln)<0
	|| line==NULL){ 
		vdoc_errno = VDOC_ERR_LIBC;
		return -1;
	}
	addblok(page->pend,0,0,line,0);

	return 0;
}

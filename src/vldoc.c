#include "common.h"
#include "vldoc.h"
#include "dummydoc.h"
#include "ps_doc.h"
#include "pdf_doc.h"
#include "magic.h"
#include <math.h>

#ifndef M_PI
	# define M_PI		3.14159265358979323846	/* pi */
#endif
/*v pripade ze neexistuje funkce, zavolat prislusnou dummy funkci*/
#define callfunc(name,p_doc)							\
	func_implement_array.functions[                                        \
	     ( ((p_doc)->type < func_implement_array.size)                     \
	       && ((p_doc)->type > 0)                                          \
	       && (func_implement_array.functions[(p_doc)->type].name) )       \
	     ? ((p_doc)->type)                                                 \
	     : (doc_register_format(NULL), 0)                                  \
           ].name



/*rozmery papiru ...*/
typedef struct dimensions_ent{
   char *  str_name;
   coordinate dimensions;	/* width, height in points */
}dimensions_ent;

/* list of paper sizes supported */
/* sizes are points (72 * sizes in inch=2.54cm) */
/*static Paper papersizes[] = {*/
static dimensions_ent _doc_page_sizes[]= {
   { "a0", {2382, 3369 }},    /* 84.1cm * 118.8cm */
   { "a1", {1684, 2382 }},    /* 59.4cm * 84.1cm */
   { "a2", {1191, 1684 }},    /* 42cm * 59.4cm */
   { "a3", {842, 1191 }},		/* 29.7cm * 42cm */
   { "a4", {595, 842 }},		/* 21cm * 29.7cm */
   { "a5",  {421,  595 }},    /* 14.85cm * 21cm */
   { "a6",  {297,  420 }},	/* 10.5cm * 14.85 cm */
   { "a7",  {210,  297 }},	/* 7.4cm * 10.5cm */
   { "a8",  {148,  210 }},	/* 5.2cm * 7.4cm */
   { "a9",  {105,  148 }},	/* 3.7cm * 5.2cm */
   { "a10",  {73,  105 }},	/* 2.6cm * 3.7cm */
   { "isob0", {2835, 4008 }},
   { "b0",    {2835, 4008 }},	/* 100cm * 141.4cm */
   { "isob1", {2004, 2835 }},
   { "b1",    {2004, 2835 }},	/* 70.7cm * 100cm */
   { "isob2", {1417, 2004 }},
   { "b2",    {1417, 2004 }},	/* 50cm * 70.7cm */
   { "isob3", {1001, 1417 }},
   { "b3",    {1001, 1417 }},	/* 35.3cm * 50cm */
   { "isob4",  {709, 1001 }},
   { "b4",     {709, 1001 }},	/* 25cm * 35.3cm */
   { "isob5",  {499,  709 }},
   { "b5",     {499,  709 }},	/* 17.6cm * 25cm */
   { "isob6",  {354,  499 }},
   { "b6",     {354,  499 }},	/* 12.5cm * 17.6cm */
   { "jisb0", {2920, 4127 }}, /* 103.0cm * 145.6cm */
   { "jisb1", {2064, 2920 }},	/* 72.8cm * 103.0cm */ 
   { "jisb2", {1460, 2064 }},	/* 51.5cm * 72.8cm */  
   { "jisb3", {1032, 1460 }},	/* 36.4cm * 51.5cm */  
   { "jisb4",  {729, 1032 }},	/* 25.7cm * 36.4cm */  
   { "jisb5",  {516,  729 }},	/* 18.2cm * 25.7cm */  
   { "jisb6",  {363,  516 }},	/* 12.8cm * 18.2cm */  
   { "c0", {2599, 3677 }},	/* 91.7cm * 129.7cm */  
   { "c1", {1837, 2599 }},	/* 64.8cm * 91.7cm */  
   { "c2", {1298, 1837 }},	/* 45.8cm * 64.8cm */  
   { "c3",  {918, 1298 }},	/* 32.4cm * 45.8cm */  
   { "c4",  {649, 918 }},		/* 22.9cm * 32.4cm */  
   { "c5",  {459, 649 }},		/* 16.2cm * 22.9cm */  
   { "c6",  {323, 459 }},		/* 11.4cm * 16.2cm */  
   { "A0", {2382, 3369 }},    /* 84cm * 118.8cm */
   { "A1", {1684, 2382 }},    /* 59.4cm * 84cm */
   { "A2", {1191, 1684 }},    /* 42cm * 59.4cm */
   { "A3", {842, 1191 }},		/* 29.7cm * 42cm */
   { "A4", {595, 842 }},		/* 21cm * 29.7cm */
   { "A5", { 421,  595 }},     /* 14.85cm * 21cm */
   { "A6",  {297,  420 }},	/* 10.5cm * 14.85 cm */
   { "A7",  {210,  297 }},	/* 7.4cm * 10.5cm */   
   { "A8",  {148,  210 }},	/* 5.2cm * 7.4cm */    
   { "A9",  {105,  148 }},	/* 3.7cm * 5.2cm */    
   { "A10",  {73,  105 }},	/* 2.6cm * 3.7cm */    
   { "ISOB0", {2835, 4008 }},
   { "B0",    {2835, 4008 }},	/* 100cm * 141.4cm */
   { "ISOB1", {2004, 2835 }},	                     
   { "B1",    {2004, 2835 }},	/* 70.7cm * 100cm */ 
   { "ISOB2", {1417, 2004 }},	                     
   { "B2",    {1417, 2004 }},	/* 50cm * 70.7cm */  
   { "ISOB3", {1001, 1417 }},	                     
   { "B3",    {1001, 1417 }},	/* 35.3cm * 50cm */  
   { "ISOB4",  {709, 1001 }},	                     
   { "B4",     {709, 1001 }},	/* 25cm * 35.3cm */  
   { "ISOB5",  {499,  709 }},	                     
   { "B5",     {499,  709 }},	/* 17.6cm * 25cm */  
   { "ISOB6",  {354,  499 }},	                     
   { "B6",     {354,  499 }},	/* 12.5cm * 17.6cm */
   { "JISB0", {2920, 4127 }},	/* 103.0cm * 145.6cm */
   { "JISB1", {2064, 2920 }},	/* 72.8cm * 103.0cm */
   { "JISB2", {1460, 2064 }},	/* 51.5cm * 72.8cm */
   { "JISB3", {1032, 1460 }},	/* 36.4cm * 51.5cm */
   { "JISB4",  {729, 1032 }},	/* 25.7cm * 36.4cm */
   { "JISB5",  {516,  729 }},	/* 18.2cm * 25.7cm */
   { "JISB6",  {363,  516 }},	/* 12.8cm * 18.2cm */
   { "C0", {2599, 3677 }},	/* 91.7cm * 129.7cm */
   { "C1", {1837, 2599 }},	/* 64.8cm * 91.7cm */
   { "C2", {1298, 1837 }},	/* 45.8cm * 64.8cm */
   { "C3",  {918, 1298 }},	/* 32.4cm * 45.8cm */
   { "C4",  {649, 918 }},		/* 22.9cm * 32.4cm */
   { "C5",  {459, 649 }},		/* 16.2cm * 22.9cm */
   { "C6",  {323, 459 }},		/* 11.4cm * 16.2cm */
   { "letter", {612, 792 }},	/* 8.5in * 11in */
   { "legal", {612, 1008 }},	/* 8.5in * 14in */
   { "ledger", {1224, 792 }},	/* 17in * 11in */
   { "tabloid", {792, 1224 }},	/* 11in * 17in */
   { "statement",{396, 612 }},	/* 5.5in * 8.5in */
   { "executive", {540, 720 }},	/* 7.6in * 10in */
   { "folio", {612, 936 }},	/* 8.5in * 13in */
   { "quarto", {610, 780 }},	/* 8.5in * 10.83in */
   { "10x14", {720, 1008 }},	/* 10in * 14in */
   { "archE", {2592, 3456 }},	/* 34in * 44in */
   { "archD", {1728, 2592 }},	/* 22in * 34in */
   { "archC", {1296, 1728 }},	/* 17in * 22in */
   { "archB",  {864, 1296 }},	/* 11in * 17in */
   { "archA",  {648,  864 }},	/* 8.5in * 11in */
   { "flsa",   {612,  936 }},  /* U.S. foolscap: 8.5in * 13in */
   { "flse",   {612,  936 }},  /* European foolscap */
   { "halfletter", {396, 612 }}, /* 5.5in * 8.5in */
   { NULL, {0, 0 }}
};

static dimensions_ent * doc_page_sizes = NULL;
static int doc_page_sizes_len = 0;
static int doc_page_sizes_end = 0;

static int doc_page_transform_(page_list * pg_handle);
static int doc_apply_page_transform(page_list_head * p_doc);

char * doc_p_order_str[]={"Ascend","Descend","Special",0};
char * doc_p_orientation_str[]={"Landscape","Portrait","Seascape","Upside-Down",0};

/*"pevne" registrovane formaty*/
static int (*doc_format_static[])(doc_function_implementation *) = {dummy_doc_register_format, ps_doc_register_format,pdf_doc_register_format};

/*pole funci pro praci s jednotlivymi formaty*/
static doc_function_implementation_array func_implement_array={NULL,0,0};

/*funkci se da jako odkaz funkce pro registraci danneho formatu*/
int doc_register_format(int (*reg_function)(doc_function_implementation *)){
	doc_function_implementation * pom;
	int ret_val;
	int i,j;
	/*alokace pameti pro dalisi format*/
	if (func_implement_array.functions==NULL){
		/*prvni volani == registrace formatu ...*/
		func_implement_array.a_size=sizeof(doc_format_static)/sizeof(int(*)(doc_function_implementation *)) + 1;
		func_implement_array.functions=(doc_function_implementation *)malloc(sizeof(doc_function_implementation)*func_implement_array.a_size);
		/*malloc fail*/
		if (func_implement_array.functions==NULL){
			vdoc_errno=VDOC_ERR_LIBC;
			/*doc_error("doc_register_format():malloc() fail",0);*/
			return -1;
		}
		for (i=0 ,j=0;i<(func_implement_array.a_size-1);++i){
			ret_val=doc_format_static[i]((func_implement_array.functions)+j);
			if (ret_val==0){
				++j;
			}
		}
		func_implement_array.size=j;
	}
	else{
		if (func_implement_array.a_size==func_implement_array.size){
			func_implement_array.a_size*=2;
			pom=(doc_function_implementation *)realloc(func_implement_array.functions,func_implement_array.a_size);
			if (pom==NULL){
				func_implement_array.a_size/=2;
				vdoc_errno=VDOC_ERR_LIBC;
				return -1;
			}else{
				func_implement_array.functions=pom;
			}
		}
	}
	if (reg_function==NULL){
		return 0;
	}
	/*registrace dalisho formatu*/
	if (reg_function(func_implement_array.functions+func_implement_array.size)==-1){
		return -1;
	}
	func_implement_array.size+=1;
	return 0;
}

void doc_free_format(void){
	if (func_implement_array.functions){
		free(func_implement_array.functions);
		func_implement_array.functions=NULL;
	}
}


/*nacte soubor filename, v pripade neuspechu vraci NULL*/
page_list_head * doc_open(const char * name){
	page_list_head *  p_doc;
	char * sep;
	char ext_str[DOC_EXT_LEN];
	int i;
	int start=1;

	if (func_implement_array.size==0){
		doc_register_format(NULL);
	}
	if (name==NULL){
		return pages_list_new(NULL, 0);
	}

	sep=strrchr(name,'.');
	if (sep!=NULL){
		strncpy(ext_str,sep+1,DOC_EXT_LEN);
		ext_str[DOC_EXT_LEN-1]=0;
		strtoupper(ext_str);	
		for (i=1;i<func_implement_array.size;i++){
			if (strcmp(ext_str,func_implement_array.functions[i].ext_str)==0){
				start=i;
				break;
			}
		}
	}
	
	/*pro kazdy podporovany format zkusit nacist soubor*/
	p_doc=pages_list_new(NULL, 0);
	assert(p_doc!=NULL);
	for (i=1;i<func_implement_array.size;i++){
		p_doc->doc->type=start;
		if (func_implement_array.functions[start].doc_open==NULL){
			continue;
		}
		if (func_implement_array.functions[start].doc_open(p_doc, name)==0){
			if (isdimzero((p_doc->doc->paper))){
				copy_dimensions(&p_doc->doc->paper,&p_doc->doc->bbox);
			}
			if (isdimzero(p_doc->doc->bbox)){
				copy_dimensions(&p_doc->doc->bbox,&p_doc->doc->paper);
			}
			if (isdimzero(p_doc->doc->bbox)){
				message(WARN,"In document is missing information about paper sizes and bbox sizes.\n");
			}
			assert(!isdimzero(p_doc->doc->bbox));
			return p_doc;
		}
		++start;
		start=(start==func_implement_array.size)?1:start;
	}
	/*dummy doc*/
	p_doc->doc->type=0;
	pages_list_delete(p_doc);
	return NULL;
}


doc_handle *doc_handle_new(doc_handle *d_handle, int type){
	doc_handle * pom=NULL;
	assert(d_handle==NULL || is_doc_handle(d_handle));
	if (is_doc_handle(d_handle)){
		d_handle->ref+=1;	
		pom=d_handle;
	}else
	{
		 pom = (doc_handle *) malloc(sizeof(doc_handle));
		 assert(pom!=NULL);
		 if (pom)
		 {
			 pom->id=M_ID_DOC_HANDLE;
			 pom->ref=1;
			 pom->type=type;
			 pom->bbox.right.x=0;
			 pom->bbox.right.y=0;
			 pom->bbox.left.x=0;
			 pom->bbox.left.y=0;
			 pom->paper.right.x=0;
			 pom->paper.right.y=0;
			 pom->paper.left.x=0;
			 pom->paper.left.y=0;
			 pom->orient=DOC_O_UNKNOWN;
			 pom->pages_count=0;
			 pom->doc=callfunc(doc_structure_new,pom)(d_handle?d_handle->doc:NULL);
			 assert(pom->doc!=NULL);
		 }
	}
	return pom;
}
 
doc_handle * doc_handle_copy_w(doc_handle * d_handle){
	doc_handle * pom;
	assert(is_doc_handle(d_handle));
	if (d_handle->ref==1){
		return d_handle;
	}
	d_handle->ref-=1;
	pom=doc_handle_new(NULL,0);
	memcpy(pom,d_handle,sizeof(doc_handle));
	pom->ref=1;
	pom->doc=callfunc(doc_structure_new,d_handle)(pom->doc);
	assert(pom->doc!=NULL);
	return pom;
}

int doc_handle_delete(doc_handle * d_handle){
	assert(is_doc_handle(d_handle));
	 if (d_handle->ref>1){
		 d_handle->ref-=1;
	 }else{
		 /*uvolneni struktury dokumentu stranky*/
		 callfunc(doc_structure_delete,d_handle)(d_handle->doc);
		 free(d_handle);
	 }
	return 0;	
 }
	 
int doc_handle_convert_format(page_list_head * h1, page_list_head * h2){
	if (h1->doc->type==0 || h2->doc->type==0){
		return 0;
	}
	if (h1->doc->type==h2->doc->type){
		return 0;
	}
	if ((h2->doc->type<func_implement_array.size) 
	&& (h2->doc->type>0)){
		return callfunc(doc_convert,h1->doc)(h2,func_implement_array.functions[h2->doc->type].ext_str); 
	}
	return -1;
}


int doc_handle_merge(doc_handle * h1,doc_handle * h2){
	assert(is_doc_handle(h1) && is_doc_handle(h2));
	if (h1->type==0 && h2->type!=0){
		h1->doc=callfunc(doc_structure_new,h2)(h2->doc);
		h1->type=h2->type;
	}
	
	/*assert(doc_convert_format(h1,h2)==0);*/
	if (h2->type==0 && h1->type!=0){
		h2->doc=callfunc(doc_structure_new,h1)(h1->doc);
		h2->type=h1->type;
	}
	/*zatim neumime konvertovat formaty mezi sebou*/

	/*assert(doc_handle_convert_format(h1, h2)!=-1);*/
	assert(h1->type==h2->type);
	max_dimensions(&h1->bbox,&h2->bbox);
	max_dimensions(&h1->paper,&h2->paper);
	if(h1->orient==DOC_O_UNKNOWN){
		h1->orient=h2->orient;
	}
	return callfunc(doc_structure_merge,h1)(h1->doc,h2->doc);
}

/*fce vytvori novy page_handle, nebo kopii pouze pro cteni!!*/
page_handle * page_handle_new_ext(page_handle * p_handle, int type, doc_handle * doc){
	page_handle * pom;
	assert(p_handle==NULL || is_page_handle(p_handle));
	if (is_page_handle(p_handle)){
		p_handle->ref+=1;
		return p_handle;
	 }
	 else{
		 pom=new_page_handle(NULL);
		 pom->type=doc?doc->type:type;
		 pom->doc=doc_handle_new(doc,0);
		 pom->page=callfunc(page_new,pom)(NULL,doc?doc->doc:NULL);
		 assert(pom->page!=NULL);
		 return pom;
	 }

}
 page_handle * page_handle_new(page_handle * p_handle, int type){
	return page_handle_new_ext(p_handle, type,NULL);
 }

void copy_page_handle(page_handle * p1,const page_handle * p2){
	transform_matrix  matrix = {{1,0,0},{0,1,0},{0,0,1}};
	if (is_page_handle(p2)){
		memcpy(p1,p2,sizeof(page_handle));
		p1->ref=1;
	}
	else{
		 p1->id=M_ID_DOC_PAGE_HANDLE;
		 p1->ref=1;
		 zero_dimensions(&(p1->bbox));
		 zero_dimensions(&(p1->paper));
		 p1->orient=DOC_O_UNKNOWN;
		 p1->number=0;
		 strncpy(p1->name,"noname",PAGE_MAX);
		 p1->doc=NULL;
		 copy_matrix(&(p1->matrix),&matrix);
	}
}

page_handle * new_page_handle(page_handle * p1){
	page_handle * pom;
	if (p1!=NULL){
		p1->ref+=1;
		return p1;
	}
	pom=(page_handle *) malloc(sizeof(page_handle));
	if (pom==NULL){
	  message(FATAL,"malloc() error");
	}
	
#if 0
	printf("NPH $%p\n",pom);
#endif
	copy_page_handle(pom,p1);

	return pom;
}

page_handle * page_handle_copy_w_ext(page_handle * p_handle, doc_handle * doc){
	page_handle * pom;
	assert(is_page_handle(p_handle));

#if 0
	printf("CPW %p %d \n",p_handle,p_handle->ref);
#endif

	if (p_handle->ref==1){
		return p_handle;
	}
	p_handle->ref-=1;

	pom=new_page_handle(NULL);
	copy_page_handle(pom,p_handle);
	pom->doc=pom->doc?doc_handle_new(pom->doc,0):NULL;
	pom->page=callfunc(page_new,p_handle)(p_handle->page,doc?doc->doc:NULL);/*kopirovani struktury stranky*/
	return pom;

}

page_handle * page_handle_copy_w(page_handle * p_handle){
	return page_handle_copy_w_ext(p_handle,NULL);
}

 int  page_handle_delete(page_handle * p_handle){
	assert(is_page_handle(p_handle));
#if 0
	printf("PHD %p %d \n",p_handle,p_handle->ref);
#endif
	if (p_handle->ref>1){
		 p_handle->ref-=1;
		 return 0;
	 }
	else{
		 /*uvolneni struktury dokumentu stranky*/
		if (p_handle->page!=NULL){
			 callfunc(page_delete,p_handle)(p_handle->page);
		}
		doc_handle_delete(p_handle->doc);
		free(p_handle);
	 }
	return 0;	
}


int doc_close(page_list_head * p_doc){
	return callfunc(doc_close,p_doc->doc)(p_doc);
}

int doc_save(page_list_head * p_doc, const char * name,void * extra_args){/*extra args, pro urcite formaty ...*/
	doc_apply_page_transform(p_doc);	
	return callfunc(doc_save,p_doc->doc)(p_doc, name, extra_args);
}


/*-operace se strankami*/

int pages_to_one(page_list_head *pglist){          /*spoji dve posobe jdouci stranky do jedne*/
	page_list * next;
	page_list * next_next;

	doc_apply_page_transform(pglist);	

	while(pages_count(pglist)>1){
		next=page_next(page_begin(pglist));
		next_next=page_next(next);
		

		next->page=page_handle_copy_w(next->page);
		next_next->page=page_handle_copy_w(next_next->page);
		max_dimensions(&next->page->bbox,&next_next->page->bbox);
		max_dimensions(&next->page->paper,&next_next->page->paper);

	/*printf(">%d %d %d %d\n",next->page->bbox.left.x,next->page->bbox.left.y,next->page->bbox.right.x,next->page->bbox.right.y);*/
		callfunc(page_merge,pglist->doc)(next->page,next_next->page);


		page_delete(next_next);
	}
	return 0;
}

int doc_draw_to_page_line(page_list * pg_handle, const coordinate * begin, const coordinate * end, int width){
	pg_handle->page=page_handle_copy_w(pg_handle->page);
	doc_page_transform_(pg_handle);
	pg_handle->page->bbox.left.x=min(pg_handle->page->bbox.left.x,begin->x);
	pg_handle->page->bbox.left.x=min(pg_handle->page->bbox.left.x,end->x);
	pg_handle->page->bbox.left.y=min(pg_handle->page->bbox.left.y,begin->y);
	pg_handle->page->bbox.left.y=min(pg_handle->page->bbox.left.y,end->y);

	pg_handle->page->bbox.right.x=max(pg_handle->page->bbox.right.x,begin->x);
	pg_handle->page->bbox.right.x=max(pg_handle->page->bbox.right.x,end->x);
	pg_handle->page->bbox.right.y=max(pg_handle->page->bbox.right.y,begin->y);
	pg_handle->page->bbox.right.y=max(pg_handle->page->bbox.right.y,end->y);

	pg_handle->page->paper.left.x=min(pg_handle->page->paper.left.x,pg_handle->page->bbox.left.x);
	pg_handle->page->paper.left.y=min(pg_handle->page->paper.left.y,pg_handle->page->bbox.left.y);

	pg_handle->page->paper.right.x=max(pg_handle->page->paper.right.x,pg_handle->page->bbox.right.x);
	pg_handle->page->paper.right.y=max(pg_handle->page->paper.right.y,pg_handle->page->bbox.right.y);

	/*TODO: dopsat zmenu bboxu ...*/
	return callfunc(draw_line,pg_handle->page->doc)(pg_handle->page, begin, end, width);
}

int doc_draw_to_page_text(page_list * pg_handle, const coordinate * where,  const char * text,int size, const char * font){
	pg_handle->page=page_handle_copy_w(pg_handle->page);
	doc_page_transform_(pg_handle);
	/*TODO: dopsat zmenu bboxu ...*/
	return callfunc(draw_text,pg_handle->page->doc)(pg_handle->page, where ,text, size, font);
}

static int doc_apply_page_transform(page_list_head * p_doc){
	page_list * page=page_begin(p_doc);
	for (page=page_next(page);page!=page_end(p_doc);page=page_next(page)){
		doc_page_transform_(page);
	}

	return 0;

}

int update_global_dimensions(page_list_head * p_doc){
	dimensions dim_paper;
	dimensions dim_bbox;
	page_list * page=page_next(page_begin(p_doc));
	copy_dimensions(&dim_bbox,&page->page->bbox);
	copy_dimensions(&dim_paper,&page->page->paper);
#if 0
	printf("<b>%d %d %d %d\n",dim_bbox.left.x,dim_bbox.left.y,dim_bbox.right.x,dim_bbox.right.y);
#endif
	
	for (page=page_next(page);page!=page_end(p_doc);page=page_next(page)){
		max_dimensions(&dim_bbox,&page->page->bbox);
		max_dimensions(&dim_paper,&page->page->paper);
	}
#if 0
	printf("<e>%d %d %d %d\n",dim_bbox.left.x,dim_bbox.left.y,dim_bbox.right.x,dim_bbox.right.y);
	printf("<e>%d %d %d %d\n",dim_paper.left.x,dim_paper.left.y,dim_paper.right.x,dim_paper.right.y);
#endif
	copy_dimensions(&p_doc->doc->bbox,&dim_bbox);
	copy_dimensions(&p_doc->doc->paper,&dim_paper);
	return 0;
}

int doc_page_transform(page_list * pg_handle, transform_matrix * matrix){      /*transformace stranky pomoci transformacni matice*/
	pg_handle->page=page_handle_copy_w(pg_handle->page);
	/*printf("%d %d %d %d\n",pg_handle->page->bbox.left.x,pg_handle->page->bbox.left.y,pg_handle->page->bbox.right.x,pg_handle->page->bbox.right.y);*/
	transform_dimensions(&pg_handle->page->bbox,matrix);
	transform_dimensions(&pg_handle->page->paper,matrix);
	
	/*printf("%d %d %d %d\n",pg_handle->page->bbox.left.x,pg_handle->page->bbox.left.y,pg_handle->page->bbox.right.x,pg_handle->page->bbox.right.y);*/
	max_dimensions(&pg_handle->page->doc->bbox,&pg_handle->page->bbox);
	max_dimensions(&pg_handle->page->doc->paper,&pg_handle->page->paper);

	transform_matrix_multi(&(pg_handle->page->matrix),matrix);
/*
	transform_matrix_multi(matrix,&(pg_handle->page->matrix));
	memcpy(&(pg_handle->page->matrix),matrix,sizeof(transform_matrix));*/
	
	return  1;
}

static int doc_page_transform_(page_list * pg_handle){
	transform_matrix  matrix = {{1,0,0},{0,1,0},{0,0,1}};
	int retval;
	if (memcmp(pg_handle->page->matrix,matrix,sizeof(transform_matrix))==0){
		return 0;
	}

	retval = callfunc(page_transform, pg_handle->page->doc)(pg_handle->page, &(pg_handle->page->matrix));
	memcpy(pg_handle->page->matrix,matrix,sizeof(transform_matrix));
	return retval;
}

int doc_page_crop(page_list * pg_handle, dimensions * dimensions){   /*orizne stranku*/
	pg_handle->page=page_handle_copy_w(pg_handle->page);
	doc_page_transform_(pg_handle);
	copy_dimensions(&pg_handle->page->bbox,dimensions);
	max_dimensions(&pg_handle->page->paper,&pg_handle->page->bbox);
	return callfunc(page_crop,pg_handle->page->doc)(pg_handle->page, dimensions);
}

int doc_update_bbox(page_list_head * handle){
	return callfunc(update_bbox,handle->doc)(handle);
}

static void doc_init_pformat_dimensions(void) {
	if (doc_page_sizes == NULL) {
		doc_page_sizes_len = sizeof(_doc_page_sizes) / sizeof(dimensions_ent);
		doc_page_sizes_end = doc_page_sizes_len -2;
		doc_page_sizes  = (dimensions_ent *)  malloc(doc_page_sizes_len * sizeof(dimensions));	
		if (doc_page_sizes == NULL) {
			message(FATAL, "malloc :: fail ()");
		}
		memcpy(doc_page_sizes, _doc_page_sizes, sizeof(_doc_page_sizes));

	}
}

void doc_set_pformat_dimensions(char * name, int x, int y) {
	int new_size;
	dimensions_ent * new_arr;
	int i;
	doc_init_pformat_dimensions();
	name=skipwhspaces(name);
	{/*pouze jedno slovo*/
		char * pom = name;
		while (*pom && !isspace((int)(*pom))){
			++pom;
		}
		*pom=0;
	}
	name=strlower(name);

	for (i=0;doc_page_sizes[i].str_name!=NULL;++i){
		if (strcmp(doc_page_sizes[i].str_name,name)==0){
			doc_page_sizes[i].dimensions.x = x;
			doc_page_sizes[i].dimensions.y = y;
			return;
		}
	}



	if (doc_page_sizes_end +2 == doc_page_sizes_len) {
		new_size = doc_page_sizes_len + 5;
		new_arr = realloc (doc_page_sizes, new_size * sizeof(dimensions));
		if (new_arr == NULL) {
			return;
		}
		doc_page_sizes = new_arr;
		doc_page_sizes_len = new_size;
	}
	asprintf(&(doc_page_sizes[doc_page_sizes_end].str_name), "%s", name);
	doc_page_sizes[doc_page_sizes_end].dimensions.x = x;
	doc_page_sizes[doc_page_sizes_end].dimensions.y = y;
	++doc_page_sizes_end;
	doc_page_sizes[doc_page_sizes_end].str_name = NULL;
	doc_page_sizes[doc_page_sizes_end].dimensions.x = 0;
	doc_page_sizes[doc_page_sizes_end].dimensions.y = 0;
}

int doc_get_pformat_dimensions(int p_size,dimensions * dim){  /*vrati rozmery formatu papiru (A4 ...)*/
	doc_init_pformat_dimensions();
	dim->right.x=doc_page_sizes[p_size].dimensions.x;
	dim->right.y=doc_page_sizes[p_size].dimensions.y;
	dim->left.x=dim->left.y=0;
	return 0;
}

/*metrika pro vypocet nejvhodnejsiho rozmeru do stranky*/
#define doc_pformat_quality(x,y,fx,fy) ((x<=fx && y<=fy)? (fx-x) + (fy-y):-1)	

char * dimensions_str(int i){
	doc_init_pformat_dimensions();
	return doc_page_sizes[i].str_name;
}


void doc_get_pformat_name_to_dimensions(char * name, dimensions *dim){
	int i;
	doc_init_pformat_dimensions();
	name=skipwhspaces(name);
	{/*pouze jedno slovo*/
		char * pom = name;
		while (*pom && !isspace((int)(*pom))){
			++pom;
		}
		*pom=0;
	}
	name=strlower(name);
	zero_dimensions(dim);
	for (i=0;doc_page_sizes[i].str_name!=NULL;++i){
		if (strcmp(doc_page_sizes[i].str_name,name)==0){
			copy_coordinate(&dim->right, &doc_page_sizes[i].dimensions);
			return;
		}
	}
	zero_dimensions(dim);
}
int doc_get_pformat_name(dimensions * dim){ /*vrati podle rozmeru format papiru (ten nejtesnejsi)*/
	int kvalita, kvalita_tmp;
	int i, format=-1;
	doc_init_pformat_dimensions();
	if (dim->right.x==0 && dim->right.y==0){
		return -1;
	}
	kvalita=-1;
	for (i=0;doc_page_sizes[i].str_name;++i){
		 kvalita_tmp=doc_pformat_quality(dim->right.x, dim->right.y,
				 doc_page_sizes[i].dimensions.x, doc_page_sizes[i].dimensions.y);
		if (kvalita<0){
			kvalita=kvalita_tmp;
			format=i;
		}else {
			if ((kvalita_tmp>=0) && (kvalita_tmp<kvalita)){
				format=i;
				kvalita=kvalita_tmp;
			}
			
		}
	}
	if (kvalita<0){
		/*
		doc_page_sizes[DOC_D_CUSTOM].dimensions.x=dim->right.x;
		doc_page_sizes[DOC_D_CUSTOM].dimensions.y=dim->right.y;
		*/
		return -1;
	}
	else{
		return format;
	}
  
}

transform_matrix * transform_matrix_new(transform_matrix * old){
	transform_matrix * pom;
	pom= (transform_matrix *) malloc(sizeof(transform_matrix));
	assert(pom!=NULL);
	if (old){
		memcpy(pom,old,sizeof(transform_matrix));
	}
	else{
		/*neni korettni pouzivat memset na double?*/
		memset(pom,0,sizeof(transform_matrix));
		*pom[0][0]=*pom[1][1]=*pom[2][2]=1;
	}
	return pom;
}


transform_matrix * transform_matrix_multi(transform_matrix * a, transform_matrix *c){
	int i,j,k;
	transform_matrix  b;
	memcpy(&b,a,sizeof(transform_matrix));
	for (i=0;i<3;++i){
		for(j=0;j<3;++j){
			(*a)[i][j]=0;
			for (k=0;k<3;++k){
				(*a)[i][j]+=b[i][k]*(*c)[k][j];	
			}
		}
	}
	return a;
}

transform_matrix *transform_matrix_scale(transform_matrix * what, double scale){
	transform_matrix matrix;
	assert(what!=NULL);
	matrix[0][0]=scale; matrix[0][1]=0; matrix[0][2]=0;
	matrix[1][0]=0; matrix[1][1]=scale; matrix[1][2]=0;
	matrix[2][0]=0; matrix[2][1]=0; matrix[2][2]=1;
	transform_matrix_multi(&matrix,what);
	memcpy(what,&matrix,sizeof(transform_matrix));
	return what;
}

transform_matrix *transform_matrix_move_xy(transform_matrix * what, double x, double y){
	transform_matrix matrix;
	assert(what!=NULL);
	matrix[0][0]=1; matrix[0][1]=0; matrix[0][2]=0;
	matrix[1][0]=0; matrix[1][1]=1; matrix[1][2]=0;
	matrix[2][0]=x; matrix[2][1]=y; matrix[2][2]=1;
	transform_matrix_multi(&matrix,what);
	memcpy(what,&matrix,sizeof(transform_matrix));
	return what;
}

transform_matrix *transform_matrix_rotate(transform_matrix * what, double angle_deg){
	transform_matrix matrix;
	assert(what!=NULL);
	matrix[0][0]=cos((M_PI*angle_deg)/180.0); matrix[0][1]=-sin((M_PI*angle_deg)/180.0);
	matrix[1][0]=sin((M_PI*angle_deg)/180.0); matrix[1][1]=cos((M_PI*angle_deg)/180.0);
							 matrix[0][2]=0;
							 matrix[1][2]=0; 	
        matrix[2][0]=0; matrix[2][1]=0; matrix[2][2]=1;

	transform_matrix_multi(&matrix,what);
	memcpy(what,&matrix,sizeof(transform_matrix));
	return what;
}

void transform_matrix_point(coordinate * point,transform_matrix * matrix){
	double x=point->x;
	double y=point->y;
	point->x=x*(*matrix)[0][0] + y*(*matrix)[1][0] + (*matrix)[2][0]; 
	point->y=x*(*matrix)[0][1] + y*(*matrix)[1][1] + (*matrix)[2][1]; 
}
/*mozna to slo napsat lepe!!!*/
void transform_dimensions(dimensions * dim, transform_matrix * matrix){
	coordinate  p1,p2,p3,p4, max,min;
	assert(dim!=NULL && matrix!=NULL);

	p1.x=dim->right.x;
	p1.y=dim->right.y;
	p2.x=dim->right.x;
	p2.y=dim->left.y;
	p3.x=dim->left.x;
	p3.y=dim->left.y;
	p4.x=dim->left.x;
	p4.y=dim->right.y;

	transform_matrix_point(&p1,matrix);	
	transform_matrix_point(&p2,matrix);	
	transform_matrix_point(&p3,matrix);	
	transform_matrix_point(&p4,matrix);	

	max.x=p1.x;
	max.x=max(p2.x,max.x);
	max.x=max(p3.x,max.x);
	max.x=max(p4.x,max.x);

	max.y=p1.y;
	max.y=max(p2.y,max.y);
	max.y=max(p3.y,max.y);
	max.y=max(p4.y,max.y);

	min.x=p1.x;
	min.x=min(min.x,p2.x);
	min.x=min(min.x,p3.x);
	min.x=min(min.x,p4.x);

	min.y=p1.y;
	min.y=min(min.y,p2.y);
	min.y=min(min.y,p3.y);
	min.y=min(min.y,p4.y);

	copy_coordinate(&(dim->right), &max);
	copy_coordinate(&(dim->left), &min);
}



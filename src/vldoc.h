#ifndef _VLDOC_H_
#define _VLDOC_H_
/**
 * \file vldoc.h
 * Working library with the implementation part of the document
 * For each document format, you need to implement a specific interface.
 * Retrieve the document, store it, work with its contents
 *
 * */

#include "common.h"
#include "page_list.h"
#include "vdoc.h"
/*
 * vdoc.h (virtual document higher implementation)
 * vldoc.h (virtual lower implementation document)
 * pdfdoc.h (pdf implementation)
 * cmdexec.h (command interpreter)
 */

/**the maximum length of the file extension*/
#define DOC_EXT_LEN 4

extern char * doc_p_order_str[];
extern char * doc_p_orientation_str[];

/**
 *\brief a structure containing implementation-dependent functions on a format
 */
typedef struct doc_function_implementation {
	int (*doc_open)(page_list_head * p_doc, const char * filename);
	int (*doc_save)(page_list_head * p_doc, const char * name, void * extra_args);
	int (*doc_close)(page_list_head * doc);
	int (*doc_convert)(page_list_head * p_doc, char * ext);/*the page trimming feature*/
	void* (*doc_structure_new)(void *structure);/*copies the document structure*/
	void (*doc_structure_delete)(void *structure);
	int (*doc_structure_merge)(void * s1, void * s2);/*combines two document structures*/

	int (*update_bbox)(page_list_head * doc);
	void* (*page_new)(const void *, void *);/*feature to create a copy of the page*/
	void (*page_delete)(void *);
	int (*page_merge)(page_handle * p1, page_handle * p2);/*link a list of pages to one*/
	int (*draw_line)(page_handle * handle, const coordinate * begin, const coordinate * end, int width);
	int (*draw_text)(page_handle * handle, const coordinate * where, const char * text,int size, const char * font);
	int (*page_transform)(page_handle * handle, transform_matrix * matrix);/*transform pages using 3x3 matrix*/
	int (*page_crop)(page_handle * handle, dimensions * dimensions);
	char ext_str[DOC_EXT_LEN]; /**<standardni koncovka dokumentu pro danny format*/
}doc_function_implementation;

typedef struct  doc_function_implementation_array{
	doc_function_implementation *functions; /* pointer to array of doc_function_implementation*/
	int size; /* the size of the previous field */
	size_t a_size; /* memory allocated for a_size number of elements */
}doc_function_implementation_array;

/* "LowLevel implementace"*/

/**
 * \brief function for registering a new document format.
 * \param reg_function ukazatel na funkci, ktera naplni prislusnou datovou strukturu
 * \retval 0 vse v poradku
 * \retval -1 pri registraci nastal problem */
int doc_register_format(int (*reg_function)(doc_function_implementation *));
void doc_free_format(void);
/* operace s celym dokumentem*/

// brief Use the name file to create a list of pages.
page_list_head * doc_open(const char * name);
/* ulozeni seznamu stranek do souboru */
/** \brief Funkce pro ulozeni seznamu stranek \a p_doc do souboru \a name.
 * \param p_doc ukazatel na seznam stranek
 * \param name nazev souboru
 * \param extra_args specielni argumenty pro konkretni format, implementacne zavisle
 * \retval 0 vse probehlo v poradku
 * \retval -1 pri ukladani nastala chyba
 */
int doc_save(page_list_head * p_doc,const char * name,void * extra_args);/*extra args, pro urcite formaty ...*/
/** \brief Funkce uvolni z pameti dokument.
 * \param p_doc ukazatel na seznam stranek
 * \retval 0 vse probehlo v poradku
 * \retval -1 nastal problem
 */
int doc_close(page_list_head * p_doc);

doc_handle *doc_handle_new(doc_handle *d_handle, int doc_type);

doc_handle * doc_handle_copy_w(doc_handle * d_handle);

int doc_handle_delete(doc_handle * d_handle);

int doc_handle_merge(doc_handle * h1,doc_handle * h2);

int doc_handle_convert_format(page_list_head * h1, page_list_head * h2);

int update_global_dimensions(page_list_head * p_doc);

/* -operace se strankami*/
/** \brief Funkce vytvori kopii stranky.
 * \param handle ukazatel na stranku, je-li NULL, vytvori se prazdna stranka
 * \param type typ dokumentu
 * \retval ukazatel na novou  stranku
 * \retval NULL nastal problem
 */
page_handle * page_handle_new(page_handle * handle, int type);

page_handle * page_handle_copy_w(page_handle * handle);

/** \brief Funkce smaze stranku.
 * \param handle ukazatel na stranku, je-li NULL, vytvori se prazdna stranka
 * \retval 0 v poradku
 * \retval -1 nastala chyba
 */
int page_handle_delete(page_handle *handle);            /*smaze stranku*/

/** \brief Funkce posklada seznam stranek \a pglist do jedne.
 * \param pglist ukazatel na seznam stranek
 * \retval ukazatel na vyslednou stranku
 * \retval NULL nastala chyba
 */
int pages_to_one(page_list_head *pglist);          /*spoji list stranek do jedne*/

 /*-operace s obsahem stranky*/
int doc_draw_to_page_line(page_list * handle, const coordinate * begin, const coordinate * end, int width);

int doc_draw_to_page_text(page_list* handle, const coordinate * where,  const char * text,int size, const char * font);

void doc_page_transform(page_list * handle, transform_matrix * matrix);

/**
 * \brief Funkce nastavi orez prislusne strance
 * \param handle ukazatel na stranku
 * \param dimensions rozmery orezoveho obdelniku
 * \retval 0 vse v poradku
 * \retval -1 nastal problem
 */
int doc_page_crop(page_list * handle, dimensions * dimensions);   /*orizne stranku*/


void copy_page_handle(page_handle * p1,const page_handle * p2);
page_handle * new_page_handle(page_handle * p1);
page_handle * page_handle_new_ext(page_handle * p_handle, int type, doc_handle * doc);
page_handle * page_handle_copy_w_ext(page_handle * p_handle, doc_handle * doc);
int doc_update_bbox(page_list_head * handle);
void doc_set_pformat_dimensions(char * name, int x, int y);


/*-ostatni "pomocne" funkce*/
int doc_get_pformat_dimensions(int p_size, dimensions *);  /*vrati rozmery formatu papiru (A4 ...)*/
int doc_get_pformat_name(dimensions * dimensions); /*vrati podle rozmeru format papiru (ten nejtesnejsi)*/
char * dimensions_str(int i);
void doc_get_pformat_name_to_dimensions(char * name, dimensions *dim);
#endif

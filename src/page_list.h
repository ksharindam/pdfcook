/**
 * \file page_list.h
 * \brief Knihovna pro praci se seznamem stranek.
 * Jednotlive stranky se uchovavaji ve spojovem seznamu.
 * Seznam ma hlavu \a doc_page_list_head a jenotlive jeho prvky jsou \a doc_page_list.
 *
 * */
#ifndef _PAGE_LIST_H_
#define _PAGE_LIST_H_
#include "doc_type.h"

/**\brief zacatek seznamu*/
#define page_begin(pg) ((page_list *) pg)
/**\brief konec seznamu*/
#define page_end(pg)   ((page_list *) pg)
/**\brief predchozi prvek*/
#define page_prev(pg)  (pg->prev)
/**\brief nasledujici prevk*/
#define page_next(pg)  (pg->next)
/**\brief pocet stranek v seznamu*/
#define pages_count(pg) (pg->doc->pages_count)
/** \brief zkopiruje souradnice z \a b do \a a*/
#define copy_coordinate(a,b) memcpy((a),(b),sizeof(struct coordinate)) 
/** \brief vynuluje souradnice*/
#define zero_coordinate(a) memset((a),0,sizeof(struct coordinate))
/** \brief zkopiruje rozmery z \a b do \a a*/
#define copy_dimensions(a,b) memcpy((a),(b),sizeof(struct dimensions)) 
/** \brief vynuluje souradnice*/
#define zero_dimensions(a) memset((a),0,sizeof(struct dimensions))
/** \brief zkopiruje transformacni matici z \a b do \a a*/
#define copy_matrix(a,b) memcpy((a),(b),sizeof(transform_matrix)) 


/**
 * \brief prvek seznamu stranek
 **/
typedef struct page_list{
	int id; /**<jednoznacny indentifikator struktury*/
	struct page_list * next; /**<nasledujici stranka v seznamu*/
	struct page_list * prev; /**<predchozi stranka v seznamu*/
	page_handle * page; /**<ukazatel na konkretni stranku*/
} page_list;

/**
 * \brief hlavicka seznamu stranek
 **/
typedef struct page_list_head{
	int id; /**<jednoznacny indentifikator struktury*/
	struct page_list * next; /**<nasledujici stranka v seznamu*/
	struct page_list * prev;/**<predchozi stranka v seznamu*/
	doc_handle * doc; /**<ukazatel na dokument, ze ktereho jsou jednotlive stranky*/
} page_list_head;

/**
 * \brief zpusob pridani stranky do seznamu
 * */
typedef enum {	pg_add_begin, /**< pridat stranku na zacatek seznamu */ 
		pg_add_end /**< pridat stranku na konec seznamu */
	     }pg_add_mode;

/**
 * \brief  vytvori kopii seznamu stranek ze souboru \a name.
 * \param name jmeno souboru
 * \retval ukazatel na seznam stranek, vse probehlo v poradku
 * \retval NULL nastala chyba
 * */
page_list_head * pages_list_open(const char * name);

/**
 * \brief ulozi seznam stranek \a list do souboru \a name
 * \param list seznam stranek
 * \param name nazev souboru
 * \retval 0 vse probehlo v poradku
 * \retval -1 nastala chyba
 */
int pages_list_save(page_list_head * list, const char * name);

/**
 * \brief fce vytvori kopii seznamu stranek \a from, je-li \a from vytvori prazdny seznam typu \a type
 * \param from seznam stranek
 * \param type typ
 * \retval !NULL ukazatel na novy seznam stranek
 * \retval NULL nastala chyba
 * */
page_list_head * pages_list_new(const page_list_head * from, int type);

/**
 * \brief  vytvori kopii seznamu \a from
 * \param from seznam stranek
 * \retval !NULL ukazatel na novy seznam stranek
 * \retval NULL nastala chyba
 * */
page_list_head * pages_list_copy(const page_list_head * from);

/**
 * \brief  odstrani seznam stranek z pameti. 
 * \param which ukazatel na mazany seznam stranek
 * */
void pages_list_delete(page_list_head * which);

/**
 * \brief  zkrati zeznam na nulovou delku. 
 * \param which ukazatel na mazany seznam stranek
 * */
void pages_list_empty(page_list_head * which);

/**
 * \brief  odstrani stranku \a what ze seznamu, ve kterem se nachazi
 * \param what stranka
 * \retval stranka uvolnena ze seznamu
 * */
page_list * page_list_remove_page(page_list * what);

/**
 * \brief  prida stranku \a what do seznamu stranek \a where.
 * \param where ukazatel na seznam stranek
 * \param what ukazatel na pridavanou stranku
 * \param how  zpusob pridani stranky*/
void pages_list_add_page(page_list_head * where, page_list * what, pg_add_mode how);


/**
 * \brief  odstrani stranku \a what do seznamu stranek \a where.
 * \param where ukazatel na seznam stranek
 * \param what ukazatel na stranku
 * \retval ukazatel na odebranou stranku*/
page_list * pages_list_remove_page(page_list_head * where, page_list * what);


/**
 * \brief  prida stranku \a what do seznamu stranek \a where, za stranku \a behide.
 * \param where ukazatel na seznam stranek
 * \param what ukazatel na pridavanou stranku
 * \param behide ukazatel na stranku, za kterou se bude stranka \a what priddavat
 */
void pages_list_insert_page(page_list_head * where, page_list * what, page_list * behide);

page_list * page_num_to_ptn(page_list_head * plist, int pnumber);

/**
 * \brief  vrati cislo poradi stranky \a page v seznamu \a plist.
 * \param plist ukazatel na seznam stranek
 * \param page ukazatel na stranku
 * \retval cislo stranky
 * \retval -1 stranka \a page neni v seznamu \a plist
 * */
int page_ptn_to_num(page_list_head * plist, page_list * page);

/**
 * \brief  spoji seznamy stranek \a l1, \a l2 do seznamu \a l1.
 * \param l1 seznam stranek
 * \param l2 seznam stranek
 * \retval 0 vse probehlo v poradku
 * \retval -1 nastala chyba
 */
int pages_list_cat(page_list_head * l1, page_list_head * l2);

/**
 *  \brief  vytvori kopi stranky \a from.
 *  \param from ukazatel na kopirovanou stranku, je-li \a from NULL vytvori se prazdna stranka
 *  \param doc_type typ dokumentu
 *  \retval ukazatel na kopii stranky
 *  \retval NULL neuspech
 */
page_list * page_new(page_list * from,int doc_type);

/**
 *  \brief  vytvori kopi stranky \a from.
 *  \param from ukazatel na kopirovanou stranku, je-li \a from NULL vytvori se prazdna stranka \param doc ukazatel na strukturu dokumentu
 *  \param doc_type identifikator typu dokumentu
 *  \param doc ukazatel na hlavicku dokumentu
 *  \retval ukazatel na kopii stranky
 *  \retval NULL neuspech
 */
page_list * page_new_ext(page_list * from,int doc_type, doc_handle * doc);


/**
 * \brief  odstrani stranku \a from z pameti.
 * \param from ukazatel na stranku
 * \retval 0 vse probehlo v poradku
 * \retval -1 nastala chyba
 */
int  page_delete(page_list * from);

/** \brief spocita maximalni rozmery z \a d1 a \a d2 a vrati je v \a d1
 * \param d1 rozmery
 * \param d2 rozmery
 */
void max_dimensions(dimensions * d1, dimensions *d2);

#endif

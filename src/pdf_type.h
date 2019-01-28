/** \file pdf_type.h 
*   \brief Definice zakladnich datovych struktur pro praci s PDF dokumentem
**/

#ifndef _PDF_TYPE_H_
#define _PDF_TYPE_H_

/** 
*\brief struktura reprezentujici objekt z PDF dokumentu
*/
typedef struct pdf_object pdf_object;

/**\brief zaznam tabulky objektu*/
typedef struct pdf_object_table_elm{
	pdf_object * obj;/**<ukazatel na objekt*/
	long offset;/**<pozice v souboru, kde se objekt nachazi*/
	int major;/**<major cislo objektu*/
	int minor;/**<minor  cislo objektu*/
	char flag;/**<'n'objekt se nachazi v dokumentu 'f' objekt neni v dokumentu*/
	char used;/**<1 pokud je pouzit objekt v dokumentu, 0 jinak*/
}pdf_object_table_elm;

/**\brief tabulka objektu*/
typedef struct pdf_object_table{
	int alocated; /**<pocet naalokonych prvku*/
	int count; /**<pocet vyuzitycj prvku*/
	pdf_object_table_elm * table;/**<ukazatel na tabulku objektu*/
}pdf_object_table;

/**\brief zaznam pro hlavicku dokumentu*/
typedef struct pdf_doc_handle {
	pdf_object * trailer; /**<ukazatel na trailer*/
	long v_major; /**<major verze PDF*/
	long v_minor; /**<minor verze PDF*/
	int refcounter;/**<pocet referenci na strukturu*/
	pdf_object_table table; /**<tabulka odkazu na obekty v dokumentu*/
	page_list_head * p_doc; /**<ukazatel na hlavu seznamu stranek*/
}pdf_doc_handle;

/**\brief zaznam pro jednu stranku dokumentu*/
typedef struct pdf_page_handle{
	int major;/**<major cislo objektu stranky*/
	int minor;/**<minor cislo objektu stranky*/
	int compressed;/**<1 je-li obsah stranky komprimovany, 0 jinak*/
	int number;/**<cislo stranky v dokumentu*/
}pdf_page_handle;
#endif

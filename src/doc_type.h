/** 
 * \file doc_type.h
 * \brief Definici zakladnich typu pro praci s dokumentem.
 */

#ifndef _DOC_TYPE_H_ 
#define _DOC_TYPE_H_

#include "limits.h"

#ifndef PATH_MAX
	/**maximalni delka nazvu souboru s cestou*/
	#define PATH_MAX 4096
#endif

/**maximalni delka nazvu stranky*/
#define PAGE_MAX 255

/**konstanty popisujici zrcadleni*/
typedef enum mirror_mode{DOC_M_HORIZONTAL,DOC_M_VERTICAL} mirror_mode;
/**konstatnty popisujici rotaci*/
typedef enum rotate_mode{DOC_RR_90,DOC_RR_180,DOC_RR_270, DOC_RL_90,DOC_RL_180,DOC_RL_270} rotate_mode;
/**konstanty popisujici umisteni textu na strance*/
typedef enum orientation{DOC_O_LANDSCAPE, DOC_O_PORTRAIT, DOC_O_SEASCAPE, DOC_O_UPSIDE_DOWN , DOC_O_UNKNOWN} orientation;
/**konstanty popisujici usporadani stranek*/
typedef enum p_order{DOC_ORD_ASCENDENT, DOC_ORD_DESCENDENT, DOC_ORD_UNKNOWN} p_order;

/**textove nazvy popisujici usporadani stranek*/
extern char * p_order_str[];
/**texttove nazvy popisujici orientaci texttu na strance*/
extern char * p_orientation_str[];

/**\brief transformacni matice*/
 typedef  double transform_matrix[3][3];

/**\brief souradnicovy typ, souradnice se uchovavaji v pixelech 1/72"*/
typedef struct coordinate{
	int x;/**<x-ova souradnice*/
	int y;/**<y-ova souradnice*/
}coordinate;

/** \brief rozmery papiru, rozmery boundingboxu ...*/
 typedef struct dimensions{
	 coordinate right; /**<horni levy roh*/
	 coordinate left; /**<dolni pravy roh*/
 }dimensions;


/** \brief handle na virtualni dokument*/
typedef struct doc_handle {
	int id; /**<id_struktury, pro runtime typovou kontrolu*/
	int ref; /**<pocet odkazu na dokument*/
	int type;     /**<typ formatu stranek v seznamu*/
	dimensions bbox; /**<rozmery bboxu*/
	dimensions paper; /**<rozmery papiru*/
	orientation orient; /**<orientace obsahu stranky*/
	p_order order;
	char file_name[PATH_MAX];/**<nazev stranky*/
   	int pages_count; /**<pocet stranek v seznamu*/
	void * doc;/**<ukazatel na interni srukturu dokumentu*/
}doc_handle;

/**\brief ukazatel na stranku*/
typedef struct page_handle {
	int id; /**<id_struktury, pro runtime typovou kontrolu???*/
	int ref; /**< pocet odkazu na dokument*/
	int type;  /**<typ stranky PS, PDF ...*/
	dimensions bbox; /**<rozmery b_boxu stranky*/
	dimensions paper; /**<rozmery papiru*/
	orientation orient; /**<orientace obsahu stranky*/
	int number; /**<cislo stranky*/
	char name[PATH_MAX];/**<nazev stranky*/
	void * page; /**<ukazatel na stukturu stranky stranku*/
	doc_handle * doc; /**<ukazatel na handle dokumentu*/
	transform_matrix matrix; /**<tarnsformacni matice konkretni stranky*/
 } page_handle;
#endif

#ifndef _cmdexec
/**\file cmdexec.h
 * \brief Prikazovy interpret pro knihovnu.
 * Jedna se o implementaci jednoducheho prikazoveho interpretu.
 * Funkci cmd_exec se preda soubor s prikazy, ktere se maji aplikovat na predany seznam stranek.
 */


#define _cmdexec
#include "common.h"	
#include "vdoc.h"
#include "fileio.h"

typedef struct cmd_ent_struct_head{
	struct cmd_ent_struct * next;
	struct cmd_ent_struct * prev;
} cmd_ent_struct_head;

int cmd_preexec(cmd_ent_struct_head * cmd_tree, MYFILE * f);

/** \brief Funkce aplikuje prikazy ze souboru f na seznam stranek p_doc.
 * \param p_doc ukazatel na seznam stranek
 * \param f file descriptor na soubor s prikazy
 * \retval 0 OK
 * \retval -1 behem zpracovani se vyskytla chyba*/
int cmd_exec(page_list_head * p_doc, cmd_ent_struct_head * cmd_tree ,MYFILE * f);
/** \brief Funkce vypise informace o prikazech interpretu
 * \param f kam se budou prikazy vypisovat*/
void cmd_print_info(FILE *f);
#endif

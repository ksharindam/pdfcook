#ifndef _PSDOC_H_
#define _PSDOC_H_
/**
 * \file ps_doc.h
 * \brief implementace rozhrani "ps" dokumentu
 * */

#include "vldoc.h"
/**koncovka dokumentu*/
#define PS_DOC_EXT "PS"
/**id dokumentu*/
#define PS_DOC_ID 1

/**
 * \brief Funkce pro registraci formatu ps dokumentu.
 * \param reg ukazatel na prislusnou datovou strukturu
 *  * \retval 0 vse v poradku
 * \retval -1 pri registraci nastal problem */
int ps_doc_register_format( doc_function_implementation * reg );

#endif

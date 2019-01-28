/**
 * \file dummydoc.h
 * \brief implementace "hloupeho" dokumentu
 * */
#ifndef _DUMMYDOC_H_
#define _DUMMYDOC_H_
#include "vldoc.h"

/**koncovka dokumentu*/
#define DUMMY_DOC_EXT ""
/**id dokumentu*/
#define DUMMY_DOC_ID 0

/**
 * \brief Funkce pro registraci formatu dummy dokumentu.
 * \param reg ukazatel na prislusnou datovou strukturu
 * \retval 0 vse v poradku
 * \retval -1 pri registraci nastal problem 
 * */
int dummy_doc_register_format( doc_function_implementation * reg );
#endif

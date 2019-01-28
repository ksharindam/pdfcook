#ifndef _PDFDOC_H_
#define _PDFDOC_H_
/**
 * \file pdf_doc.h
 * \brief implementace rozhrani "pdf" dokumentu
 * */

#include "vldoc.h"
#define PDF_DOC_EXT "PDF"
/**
 * \brief Funkce pro registraci formatu pdf dokumentu.
 * \param reg ukazatel na prislusnou datovou strukturu
 *  * \retval 0 vse v poradku
 * \retval -1 pri registraci nastal problem */
int pdf_doc_register_format( doc_function_implementation * reg );
#endif

/**
	\file pdf_lib.h
	\brief Knihovna pro praci s PDF formatem.

	V teto knihovne jsou implementovany zakladni funkce pro praci s PDF formatem.
*/

#ifndef _PDFLIB_H_
#define _PDFLIB_H_

#include "vldoc.h"
#include "pdf_type.h"

/**\brief funkce nacte PDF dokument dokument ze souboru \a name do pameti 
 * \param p_doc seznam stranek
 * \param name nazev souboru
 * \retval 0 OK
 * \retval -1 nastala chyba
 * */
int pdf_open(page_list_head * p_doc,const char * name);

/**\brief funkce vytvori ze struktury \a p_doc novy dokument \a name
 * \param p_doc seznam stranek
 * \param name nazev souboru
 * \retval 0 OK
 * \retval -1 nastala chyba
 **/
int pdf_save(page_list_head * p_doc,const char * name);

/**\brief funkce vytvori kopii hlavicky PDF dokumentu \a old, 
 * pokud je \a old NULL, vytvori novou
 *\param old hlavicka PDF dokumentu
 *\retval !NULL OK
 *\retval NULL nastala chyba
 * */
pdf_doc_handle * pdf_structure_new(pdf_doc_handle * old);

/**\brief funkce uvolni z pameti hlavicku PDF dokumentu
 * \param p_pdf hlavicka pdf_dokumentu
 * */
void pdf_structure_delete(pdf_doc_handle * p_pdf);

/**\brief funkce spoji dve hlavicky \a s1, \a s2 PDF dokumentu do jedne, z hlavicky \a s2 jsou vsecny objekty odstraneny
 * \param s1 hlavicka PDF dokumentu
 * \param s2 hlavicka PDF dokumentu
 * \retval 0 OK
 * \retval -1 nastala chyba
 * */
int pdf_structure_merge(pdf_doc_handle * s1, pdf_doc_handle * s2);

/**\brief funkce vytvori kopi stranky \a old, je-li \a old NULL, vytvori se prazdna stranka
 * \param old hlavicka PDF dokumentu
 * \param doc seznam stranek
 * \retval !NULL OK
 * \retval NULL nastala chyba
 * */
pdf_page_handle * pdf_page_new(const pdf_page_handle * old, pdf_doc_handle * doc);

/**\brief funkce uvolni stranku \a page z pameti
 * \param page handle na stranku
 * */
void pdf_page_delete(pdf_page_handle * page);

/**\brief funkce spoji stranky \a p1, \a p2 do jedne stranky \a p1, stranka \a p2 zustane nezmenena
 * \param p1 stranka 
 * \param p2 stranka
 * \retval 0 OK
 * \retval -1 nastala chyba
 * */
int pdf_page_merge(page_handle * p1, page_handle * p2);

/**\brief funkce provede transformaci stranky \a page za pomoci transformaci matice \a matrix
 * \param page stranka
 * \param matrix transformacni matice
 * \retval 0 OK
 * \retval -1 nastala chyba*/
int pdf_page_transform(page_handle * page, transform_matrix * matrix);

/**\brief funkce orizne stranku \a page na rozmery \a dim
 * \param page stranka
 * \param dim rozmery
 * \retval 0 OK
 * \retval -1 nastala chyba
 * */
int pdf_page_crop(page_handle * page, dimensions * dim);

/**\brief funkce nakresli na stranku \a page caru
 * \param page stranka
 * \param begin pocatecni bod
 * \param end koncovy bod
 * \param width sirka cary
 * \retval 0 OK
 * \retval -1 nastala chyba
 * */
int pdf_page_line(page_handle * page, const coordinate * begin, const coordinate * end, int width);

/**\brief funkce umisti text na stranku \a page
 * \param page stranka
 * \param where pozice
 * \param text vykreslovany text
 * \param size velikost textu
 * \param font rez fontu, je-li NULL vykresli se vychozi
 * \retval 0 OK
 * \retval -1 
*/  
int pdf_page_text(page_handle * page, const coordinate * where, const char * text,int size, const char * font);
#endif

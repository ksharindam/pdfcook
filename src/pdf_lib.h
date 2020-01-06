#ifndef _PDFLIB_H_
#define _PDFLIB_H_

#include "page_list.h"
#include "vldoc.h"
#include "pdf_type.h"


int pdf_open(page_list_head * p_doc,const char * name);

int pdf_save(page_list_head * p_doc,const char * name);

pdf_doc_handle * pdf_structure_new(pdf_doc_handle * old);

void pdf_structure_delete(pdf_doc_handle * p_pdf);

int pdf_structure_merge(pdf_doc_handle * s1, pdf_doc_handle * s2);

pdf_page_handle * pdf_page_new(const pdf_page_handle * old, pdf_doc_handle * doc);

void pdf_page_delete(pdf_page_handle * page);

int pdf_page_merge(page_handle * p1, page_handle * p2);

int pdf_page_transform(page_handle * page, transform_matrix * matrix);

int pdf_page_crop(page_handle * page, dimensions * dim);

int pdf_page_line(page_handle * page, const coordinate * begin, const coordinate * end, int width);

int pdf_page_text(page_handle * page, const coordinate * where, const char * text,int size, const char * font);

#endif

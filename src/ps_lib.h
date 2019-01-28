/**
	\file ps_lib.h
	\brief Knihovna pro praci s postscriprtovym souborem.
       Knohovna pro praci s postscriptovym souborem s DSC komemntary.	
*/

#ifndef _DSC_H_

#define _DSC_H_

#include "ps_type.h"
#include "vdoc.h"


DSC * ps_dsc_structure_new(DSC * dsc_old);
int ps_dsc_open(page_list_head * doc, const char * fname);
int ps_dsc_save (page_list_head * p_doc, const char * fnameout);
int ps_dsc_structure_delete(DSC * dsc);
PAGE * ps_dsc_page_new(const PAGE * what);
int ps_dsc_page_delete(PAGE * page);
int ps_dsc_draw_to_page_line(page_handle * pg_handle, const coordinate * begin, const coordinate * end, int l_width);
int ps_dsc_draw_to_page_text(page_handle * pg_handle, const coordinate * where, int size, const char * font, const char * text);
int ps_dsc_2pages_to_one(PAGE * p1, PAGE * p2);
int ps_dsc_page_transform(page_handle * pg_handle, transform_matrix * matrix);
int ps_dsc_set_crop_page(page_handle * pg_handle, dimensions * dimensions);

#endif

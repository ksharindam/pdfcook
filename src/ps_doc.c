#include "vldoc.h"
#include "ps_doc.h"
#include "page_list.h"
#include "vdocerror.h"
#include "ps_lib.h"

int  ps_doc_open(page_list_head * p_doc, const char * name){
	return ps_dsc_open(p_doc,name);
}
int ps_doc_save(page_list_head * p_doc, const char * name,void * extra_args){ /*extra args, pro urcite formaty ...*/
	return ps_dsc_save(p_doc, name);
}

void * ps_doc_page_new(const void * page, void * doc){
	return (void *) ps_dsc_page_new((PAGE *)page);
}

void  ps_doc_page_delete(void * page){
	ps_dsc_page_delete((PAGE *)page);
}

int ps_doc_delete_page(void * page){    /*smaze stranku*/
	return ps_dsc_page_delete(page);
}
void * ps_doc_structure_new(void * structure){
	return ps_dsc_structure_new(structure);
}
int ps_doc_structure_delete(void *structure){
	return ps_dsc_structure_delete(structure);
}
int ps_doc_2pages_to_one(page_handle * p1, page_handle * p2){
	return ps_dsc_2pages_to_one(p1->page, p2->page);
}

int ps_doc_draw_to_page_line(page_handle * pg_handle, const coordinate * begin, const coordinate * end, int l_width){           
	return ps_dsc_draw_to_page_line(pg_handle, begin, end, l_width);
}

int ps_doc_draw_to_page_text(page_handle * pg_handle, const coordinate * where, const char * text,int size, const char * font){           
	return ps_dsc_draw_to_page_text(pg_handle, where, size,font, text);
}
int ps_doc_page_transform(page_handle * pg_handle, transform_matrix * matrix){
	return ps_dsc_page_transform(pg_handle, matrix);
}
int ps_doc_set_crop_page(page_handle * pg_handle, dimensions * dimensions){
	return ps_dsc_set_crop_page(pg_handle,dimensions);
}

int ps_doc_structure_merge(void * s1, void * s2){
	return s1==s2;
}
int ps_doc_register_format( doc_function_implementation * reg ){
	reg->doc_open=ps_doc_open;
	reg->doc_save=ps_doc_save;
	reg->doc_close=NULL; /*snad to nebude potreba implementovat u kazdeho formatu ...*/
	reg->doc_convert=NULL;;
	reg->doc_structure_new=(void * (*)(void *structure))ps_dsc_structure_new;
	reg->doc_structure_delete=(void (*)(void *structure))ps_dsc_structure_delete;
	reg->doc_structure_merge=ps_doc_structure_merge;
	reg->page_new=ps_doc_page_new;
	reg->page_delete=ps_doc_page_delete;
	reg->page_merge=ps_doc_2pages_to_one;
	reg->draw_line=ps_doc_draw_to_page_line;
	reg->draw_text=ps_doc_draw_to_page_text;
	reg->page_transform=ps_doc_page_transform;
	reg->page_crop=ps_doc_set_crop_page;
	reg->update_bbox=NULL;
	strncpy(reg->ext_str,PS_DOC_EXT, DOC_EXT_LEN);
	return 0;
}

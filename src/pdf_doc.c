#include "pdf_doc.h"
#include "pdf_lib.h"
#include "vldoc.h"
#include "vdocerror.h"

int pdf_doc_open(page_list_head * p_doc, const char * filename){
	return pdf_open(p_doc,filename);
}
int pdf_doc_save(page_list_head * p_doc, const char * name,void * extra_args){ 
	return pdf_save(p_doc, name);
}

int pdf_doc_page_transform(page_handle * pg_handle, transform_matrix * matrix){
	return pdf_page_transform(pg_handle, matrix);
}

void pdf_doc_page_delete(void * pg_handle){
	pdf_page_delete((pdf_page_handle *) pg_handle);
	return;
}

void *  pdf_doc_page_new(const void * pg_handle, void *  doc){
	return pdf_page_new((pdf_page_handle *)pg_handle, (pdf_doc_handle *)doc);
}
void * pdf_doc_structure_new(void * structure){
	return pdf_structure_new((pdf_doc_handle *) structure);
}
void pdf_doc_structure_delete(void * structure){
	pdf_structure_delete((pdf_doc_handle *) structure);
	return;
}

int pdf_doc_structure_merge(void *s1, void * s2){
	return pdf_structure_merge((pdf_doc_handle *) s1,(pdf_doc_handle *) s2);
}
int pdf_doc_page_merge(page_handle * p1, page_handle * p2){
	return pdf_page_merge(p1,p2);
}
int pdf_doc_convert(page_list_head * p_doc, char * ext){
#ifdef HAVE_GS
	char filein[]=".tmp.XXXXXXXX";
	char fileout[]=".tmp.XXXXXXXX.pdf";
	page_list_head * pom;
	char * cmd;
	FILE * f;
	int fd;
	if (strcmp(ext,"PS")){
		return -1;
	}
	fd=mkstemp(filein);
	close(fd);
	memcpy(fileout,filein,strlen(filein));
	if (doc_save(p_doc,filein,NULL)==-1){
		return -1;
	}
	
	asprintf(&cmd,"ps2pdf %s %s", filein, fileout);
	f=popen(cmd,"r");
	free(cmd);
	while (fgetc(f)!=EOF);
	pclose(f);
	pages_list_empty(p_doc);
	doc_handle_delete(p_doc->doc);
	pom = doc_open(fileout);
	remove(filein);
	remove(fileout);
	if (pom){
		memcpy(p_doc,pom, sizeof(page_list_head));
		free(pom);
		p_doc->next->prev=(page_list *)p_doc;
		p_doc->prev->next=(page_list *)p_doc;
		((pdf_doc_handle *)p_doc->doc->doc)->p_doc=p_doc;
		return 0;
	}
#endif
	return -1;

}
int pdf_doc_register_format( doc_function_implementation * reg ){
	reg->doc_open=pdf_doc_open;
	reg->doc_save=pdf_doc_save;
	reg->doc_close=NULL;
	reg->doc_convert=pdf_doc_convert;
	reg->doc_structure_new=pdf_doc_structure_new;
	reg->doc_structure_delete=pdf_doc_structure_delete;
	reg->doc_structure_merge=pdf_doc_structure_merge;
	reg->page_new=pdf_doc_page_new;
	reg->page_delete=pdf_doc_page_delete;
	reg->page_merge=pdf_doc_page_merge;
	reg->draw_line=pdf_page_line;
	reg->draw_text=pdf_page_text;
	reg->page_transform=pdf_doc_page_transform;
	reg->page_crop=pdf_page_crop;
	reg->update_bbox=NULL;
	strncpy(reg->ext_str,PDF_DOC_EXT, DOC_EXT_LEN);
	return 0;
}

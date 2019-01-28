#include "vldoc.h"
#include "dummydoc.h"
#include "vdocerror.h"

int dummy_doc_open(page_list_head * p_doc, const char * filename){
	message(FATAL,"dummy_doc_open().\n",1);
	vdoc_errno=0;
	return 0;
}
int dummy_doc_save(page_list_head * p_doc, const char * name,void * extra_args){
	message(FATAL,"dummy_doc_save().\n",1);
	vdoc_errno=0;
	return 0;
}

int dummy_doc_close(page_list_head * doc){
	message(FATAL,"dummy_doc_close().\n",1);
	vdoc_errno=0;
	return 0;
}

int dummy_doc_structure_merge(void * s1, void * s2){
	return s1==s2;
}
void dummy_doc_page_delete(void *pg_handle){
	message(FATAL,"dummy_doc_delete_page().\n",1);
	vdoc_errno=0;
}
void * dummy_doc_structure_new(void * structure){
	
	return (void *) 1;
}
void  dummy_doc_structure_delete(void *structure){
}
int dummy_doc_convert(page_list_head * p_doc, char * ext){
	return -1;
}

#define OUT_MAX 256
int dummy_doc_bbox_update(page_list_head * p_doc){
#ifdef HAVE_GS
	char filein[]=".tmp.XXXXXXXX";
	char * cmd;
	char in[OUT_MAX];
	FILE * f;
	int i;
	page_list * page=page_next(page_begin(p_doc));
	int fd=mkstemp(filein);
	dimensions dim;
	close(fd);
	pages_list_save(p_doc,filein);
	asprintf(&cmd," ( gs -sDEVICE=bbox -dQUIET -dNOPAUSE -r100 -dBATCH %s )  2>&1 ",filein);
	f=popen(cmd,"r");
	free(cmd);
	if (f==NULL){
		message(FATAL,"bbox: popen() error.\n");
	}
	i=0;
	while (fgets(in,OUT_MAX,f)){
		if (starts(in,"%%BoundingBox:")){
			if (sscanf(in + 14,"%d %d %d %d",&dim.left.x, &dim.left.y, &dim.right.x, &dim.right.y)!=4){
				message(FATAL, "Wrong BBox format.\n");
			}
#if 0
			printf ("%d %d %d %d\n",dim.left.x, dim.left.y, dim.right.x, dim.right.y);
#endif
			if (page!=page_begin(p_doc)){
				/*copy_dimensions(&page->page->paper,&dim);*/
				copy_dimensions(&page->page->bbox,&dim);
				/*if is bbox bigger than page, upgrade it*/
				max_dimensions(&page->page->paper, &page->page->bbox);
#if 0
				printf ("%d %d %d %d\n",page->page->bbox.left.x, page->page->bbox.left.y, page->page->bbox.right.x, page->page->bbox.right.y);
#endif
				page=page_next(page);
				if (i){
					printf(" ");
				}
			}
			else{
				message(FATAL, "bbox:Wrong pages count!.\n");
			}
			printf("[%d]",i+1);
			fflush(stdout);
#if 0
			printf("%d %d %d %d\n",dim.left.x, dim.left.y, dim.right.x, dim.right.y);
			fflush(stdout);
#endif
			++i;
		}
	}
	printf("\n");
	pclose(f);
	remove(filein);
	update_global_dimensions(p_doc);

	if (page!=page_end(p_doc)){
		message(FATAL,"There were some errors during recalculating bbox.\n");
	}

	return page==page_end(p_doc)?0:-1;
#else
	message(FATAL,"Program was compiled without ghostscript support.\n");
	return -1;
#endif
}

int dummy_2doc_pages_to_one(page_handle * p1, page_handle * p2){
	message(FATAL,"dummy_doc_2pages_to_one().\n",1);
	vdoc_errno=0;
	return 0;
}
int dummy_doc_draw_to_page_line(page_handle * handle, const coordinate * begin, const coordinate * end, int width){
	message(FATAL,"dummy_doc_draw_to_page().\n",1);
	vdoc_errno=0;
	return 0;
}
int dummy_doc_draw_to_page_text(page_handle * pg_handle, const coordinate * where, const char * text,int size, const char * font){
	message(FATAL,"dummy_doc_draw_to_page().\n",1);
	vdoc_errno=0;
	return 0;
}
int dummy_doc_page_transform(page_handle * pg_handle, transform_matrix * matrix){
	message(FATAL,"dummy_doc_page_transform().\n",1);
	vdoc_errno=0;
	return 0;
}
int dummy_doc_page_crop(page_handle * pg_handle, dimensions * dimensions){
	message(FATAL,"dummy_doc_set_crop_page().\n",1);
	vdoc_errno=0;
	return 0;
}
void * dummy_doc_page_new(const void * page, void * doc){
	return (void *) 1;
}
int dummy_doc_register_format( doc_function_implementation * reg ){
	reg->doc_open=dummy_doc_open;
	reg->doc_save=dummy_doc_save;
	reg->doc_close=dummy_doc_close;
	reg->doc_convert=dummy_doc_convert;
	reg->doc_structure_new=dummy_doc_structure_new;
	reg->doc_structure_delete=dummy_doc_structure_delete;
	reg->doc_structure_merge=dummy_doc_structure_merge;
	reg->page_new=dummy_doc_page_new;
	reg->page_delete=dummy_doc_page_delete;
	reg->page_merge=dummy_2doc_pages_to_one;
	reg->draw_line=dummy_doc_draw_to_page_line;
	reg->draw_text=dummy_doc_draw_to_page_text;
	reg->page_transform=dummy_doc_page_transform;
	reg->page_crop=dummy_doc_page_crop;
	reg->update_bbox=dummy_doc_bbox_update;
	strncpy(reg->ext_str,DUMMY_DOC_EXT, DOC_EXT_LEN);
	return 0;
}

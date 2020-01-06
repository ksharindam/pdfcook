#ifndef _PDF_TYPE_H_
#define _PDF_TYPE_H_

#define LLEN 256

typedef struct pdf_object pdf_object;

typedef struct pdf_object_table_elm{
	pdf_object *obj;
    int type;    // 0=free(f), 1=nonfree(n), 2=compressed
	long offset; // type 1 only
	int major;   // object no.
	int minor; // gen id (always 0 for type 2)
    int obj_stm; // obj no. of object stream where obj is stored (for type 2)
    int index;   // index no. within the obj stream (for type 2)
	int used;
}pdf_object_table_elm;

typedef struct pdf_object_table{
	int alocated;
	int count;
	pdf_object_table_elm * table;
}pdf_object_table;


typedef struct pdf_doc_handle {
	pdf_object * trailer;
	long v_major;
	long v_minor;
	int refcounter;
	pdf_object_table table;
	page_list_head * p_doc;
}pdf_doc_handle;


typedef struct pdf_page_handle{
	int major;
	int minor;
	int compressed;
	int number;
}pdf_page_handle;

#endif

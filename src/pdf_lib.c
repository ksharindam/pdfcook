#include "common.h"
#include "pdf_lib.h"
#include "pdf_parser.h"
#include "fileio.h"
#include "pdf_filters.h"

#define XREF_ENT_LEN 18
#define PDF_TRAILER_OFFSET 64
#define NODE_MAX 10

#define get_pdf_handle(p) ((pdf_doc_handle*)(p->doc->doc))
#define get_obj_table(t) (&get_pdf_handle(t)->table)
#define get_page_content(p) (((pdf_page_handle *)p->page->page))


#define get_object_(pobj,p_pdf,major,minor_)\
	assert((major)<(p_pdf)->table.count); \
    assert((p_pdf)->table.table[(major)].minor==(minor_));\
	(pobj)=(p_pdf)->table.table[(major)].obj;\

#define TABLE_GET_ELM(t,i)((t)->table[(i)])


static char *trailer_filter[] = { "Size", "Root", "ID", NULL };
static char *catalog_filter[] = { "Pages", "Type" ,  NULL };
static char *page_filter[] = { "Type", "Parent", "Resources", "MediaBox", "Contents", NULL };
static char *xobject_filter[] = { "Type", "Subtype", "Resources", "BBox", "Length", "Filter", "FormType",  NULL };
static char *parent_filter[] = {"Type", "Count", "Kids", NULL};


int object_table_ent_insert(pdf_object_table * table, pdf_object * pobj);
int page_to_xobj(pdf_page_handle * page, pdf_doc_handle * p_pdf, transform_matrix * matrix);
static int pdf_page_to_xobj(page_handle * pg_handle);
int pdf_merge_stream(pdf_doc_handle * p_pdf,pdf_object * stream1, pdf_object * stream2, char * begin, char * end, char * sep);
pdf_doc_handle * pdf_structure_copy_w(pdf_doc_handle * p_pdf);
int stream_to_xobj(pdf_doc_handle * p_pdf, pdf_object * contents, pdf_object * pg,  transform_matrix * matrix);
int pdf_decompress_stream(pdf_doc_handle * p_pdf, int major, int minor);
int pdf_compress_stream(pdf_doc_handle * p_pdf, int major, int minor, char * filter);


pdf_object_table *object_table_realoc(pdf_object_table * old, long size){
	int i;
	if (old->table==NULL){
		old->count=size;
		old->alocated=size+1;
		old->table=(pdf_object_table_elm *)malloc(sizeof(pdf_object_table_elm)*old->alocated);
		if (old->table==NULL){
			message(FATAL,"malloc() error\n");
		}
		for(i=0;i<size+1;++i){
			old->table[i].obj=NULL;
			old->table[i].type=0;
		}
	}
	else{
		if (size>old->alocated){
			pdf_object_table_elm * pom=old->table;
			old->alocated=size+10;
			old->table=(pdf_object_table_elm *)realloc(old->table,sizeof(pdf_object_table_elm)*old->alocated);
			if (old->table==NULL){
				old->table=pom;
			}else{
				for(i=old->count+1;i<size+1;++i){
					old->table[i].obj=NULL;
					old->table[i].type=0;
				}
				old->count=size;
			}
		}else{
			if ((old->count)<size){
				for(i=size+1;i<old->count+1;++i){
					old->table[i].type=0;
				}
				old->count=size;
			}
		}
	}
	return (old && size<=old->count)?old:NULL;
}

int  object_table_copy(pdf_object_table * new,const pdf_object_table * obj){
	int i;
	if (obj!=NULL){
		memcpy(new,  obj, sizeof(pdf_object_table));
		new->table=(pdf_object_table_elm *)  malloc(sizeof(pdf_object_table_elm) * new->alocated);
		if (new->table==NULL){
			message(FATAL,"malloc() error\n");
		}else{
			memcpy(new->table,obj->table,new->alocated * sizeof(pdf_object_table_elm));
			for (i=1;i<new->count;++i){
				if (new->table[i].type == 1){
					new->table[i].obj = pdf_new_object();
					pdf_copy_object(new->table[i].obj,obj->table[i].obj);
				}
			}
		}
	}
	return 0;
}

pdf_object_table * object_table_delete(pdf_object_table * xref){
	int n;
	for (n=0;n<xref->count;++n){
		if (xref->table[n].type) {
            if (pdf_delete_object(xref->table[n].obj)==0) {
                free(xref->table[n].obj);
                xref->table[n].obj=NULL;
            }
            else
                message(WARN, "could not delete obj : %d\n", n);
		}
	}
	free(xref->table);
    xref->table=NULL;
	return 0;
}

int object_table_write(pdf_object_table * table,FILE * f){
	int i;
	fprintf(f,"xref\n%d %d\n",0,table->count);
	for (i=0;i<table->count;++i){
        char type = table->table[i].type ? 'n' : 'f';
		if (fprintf(f,"%010ld %05d %c \n",table->table[i].offset, table->table[i].minor, type)<0){
			message(FATAL, "I/O error");
		}
	}
	return 0;
}

// Replace old references with new references of same object
void update_object(pdf_object * obj, pdf_object_table * table){
	switch (obj->type){
		 case PDF_OBJ_BOOL:
		 case PDF_OBJ_INT:
		 case PDF_OBJ_REAL:
		 case PDF_OBJ_STR:
		 case PDF_OBJ_NAME:
		 case PDF_OBJ_NULL:
			 return;
		 case PDF_OBJ_ARRAY:
			{
			pdf_array *  p_array=obj->val.array.next;
			while (p_array!=(pdf_array *)&(obj->val.array)){
				update_object(p_array->obj,table);
				p_array=p_array->next;
			}
			}
			 return;
		 case PDF_OBJ_DICT:
			{
			pdf_dict * dict=obj->val.dict.next;
			while(dict!=(pdf_dict *) &(obj->val.dict)){
				update_object(dict->obj,table);
				dict=dict->next;
			}
			}
			 return;
		 case PDF_OBJ_STREAM:
			update_object(obj->val.stream.dict,table);
			return;
		 case PDF_OBJ_INDIRECT_REF:
			obj->val.reference.minor=TABLE_GET_ELM(table,obj->val.reference.major).minor;
			obj->val.reference.major=TABLE_GET_ELM(table,obj->val.reference.major).major;
			 return;
		default:
			 assert(0);
			 return;
	}
}

// flag used=1 for used/referenced objects
// flag 1 objects will not be deleted during write
void dfs_object(pdf_object *obj, pdf_object_table * table){
	switch (obj->type){
		 case PDF_OBJ_BOOL:
		 case PDF_OBJ_INT:
		 case PDF_OBJ_REAL:
		 case PDF_OBJ_STR:
		 case PDF_OBJ_NAME:
		 case PDF_OBJ_NULL:
			 return;
		 case PDF_OBJ_ARRAY:
			{
			pdf_array *  p_array=obj->val.array.next;
			while (p_array!=(pdf_array *)&(obj->val.array)){
				dfs_object(p_array->obj,table);
				p_array=p_array->next;
			}
			}
			 return;
		 case PDF_OBJ_DICT:
			{
			pdf_dict * dict=obj->val.dict.next;
			while(dict!=(pdf_dict *) &(obj->val.dict)){
				dfs_object(dict->obj,table);
				dict=dict->next;
			}
			}
			 return;
		 case PDF_OBJ_STREAM:
			dfs_object(obj->val.stream.dict,table);
			return;
		 case PDF_OBJ_INDIRECT_REF:
			 if (TABLE_GET_ELM(table,obj->val.reference.major).used){
				 return;
			 }
			 TABLE_GET_ELM(table,obj->val.reference.major).used=1;
			 if (TABLE_GET_ELM(table,obj->val.reference.major).obj==NULL){
                 // in some bad pdfs even the object is free, the object is referenced
                //message(WARN, "obj no %d is null obj\n", obj->val.reference.major);
                TABLE_GET_ELM(table,obj->val.reference.major).obj = pdf_new_object();
                TABLE_GET_ELM(table,obj->val.reference.major).obj->type = PDF_OBJ_NULL;
                return;
			}
			 dfs_object(TABLE_GET_ELM(table,obj->val.reference.major).obj,table);
			 return;
		default:
			 assert(0);
			 return;
	}
}

void update_page_ref(page_list_head * p_doc){
	page_list *  pom;
	for (pom=p_doc->next;pom!=page_end(p_doc);pom=page_next(pom)){
		get_page_content(pom)->minor=TABLE_GET_ELM(get_obj_table(p_doc),get_page_content(pom)->major).minor;
		get_page_content(pom)->major=TABLE_GET_ELM(get_obj_table(p_doc),get_page_content(pom)->major).major;
	}
	return;
}

// it gets called before dfs_object() in case of multiple pdfs
void update_obj_ref(page_list_head * p_doc){
	int i;
	pdf_object_table * table = get_obj_table(p_doc);
	update_object(get_pdf_handle(p_doc)->trailer,table);
	for (i=0;i<table->count;++i){
		if (table->table[i].type){
			update_object(table->table[i].obj,table);
		}
	}
	return;
}

int object_table_delete_unused(page_list_head * p_doc){
	pdf_object * obj = get_pdf_handle(p_doc)->trailer;
	pdf_object_table * table = get_obj_table(p_doc);
	long * offset = &table->table[0].offset;
	int i,major,count;
	for (i=0;i<table->count;++i){
		table->table[i].major=i;
		table->table[i].used=0; // flag all objects as unused
	}
	dfs_object(obj,table);      // flag used=1 for used objects
	for (i=1,major=1;i<table->count;++i){
		if (table->table[i].used){
            table->table[i].type=1;
			table->table[i].major=major;
            //printf("%d %d\n", i, major);
			++major;
		}
		else{
			if (table->table[i].type!=0){ // not used but obj loaded in memory
				table->table[i].type=0;
				pdf_delete_object(table->table[i].obj);
				free(table->table[i].obj);
				table->table[i].obj=NULL;
			}
			*offset=i;
			table->table[i].minor++;
			offset=&table->table[i].offset;
		}
	}
	*offset=0;
	update_page_ref(p_doc);
	update_obj_ref(p_doc);
	table->table[0].offset=0;
	for (i=1,count=0;i<table->count;++i){
		if (table->table[i].used){
			if (i!=table->table[i].major){
				memcpy(&table->table[table->table[i].major],&table->table[i],sizeof(pdf_object_table_elm));
			}
			count=table->table[i].major;
		}
	}
	table->count=count+1;
	return 0;
}

// from PDF 1.5 the xreftable can be a stream in an indirect object.
// the dictionary of stream is the trailer dictionary.
// essential keys : Type, Size and W . Optional keys : Index, Prev
int xreftable_from_stream(MYFILE *f, pdf_object_table *table, pdf_object *p_trailer)
{
    //FILE *fd;
    //fd = fopen("trailer", "wb");
    //fd = fopen("xref", "wb");
    pdf_object content;
    if (pdf_get_object(f, &content, NULL,NULL)!=0) {
        message(FATAL, "Unable to get xref from stream\n");
        return -1;
    }
    pdf_object *obj = content.val.reference.obj;
    pdf_copy_object(p_trailer, obj->val.stream.dict);
    //pdf_write_object(p_trailer, stdout); // write trailer dictionary to a file
    //fflush(fd);
    decompress_stream_object(obj);
    // table_size is the max object number + 1
    int table_size = pdf_get_dict_name_value(p_trailer, "Size")->val.int_number;
    table = object_table_realoc(table, table_size);
    // split stream into table, W parameter is array of length 3
    pdf_array_head w = pdf_get_dict_name_value(p_trailer, "W")->val.array;
    pdf_array *p_arr = w.next;
    int w_arr[3];
    for (int i=0; i<3; ++i) {
        w_arr[i] = p_arr->obj->val.int_number;
        p_arr = p_arr->next;
    }
    int row_len = w_arr[0] + w_arr[1] + w_arr[2];
    // Index is array of pairs of integers. Each pair has obj number and obj count
    pdf_object *index = pdf_get_dict_name_value(p_trailer, "Index");
    if (index==NULL) {
        index = pdf_add_dict_name_value(p_trailer, "Index");
        char s[24];
        snprintf(s, 23, "[ 0 %d ]", table_size);
        assert(pdf_get_object_from_str(index, s)==0);
    }
    pdf_array *item = index->val.array.next;
    for (int i=0; item != (pdf_array *)&(index->val.array); item = item->next) {
        int first = item->obj->val.int_number;
        item = item->next;
        int count = item->obj->val.int_number;
        for (int major=first; major<first+count; major++) {
            if (table->table[major].type!=0){//skip when already set by next xref table
                i+=row_len;
                continue;
            }
            char *row = obj->val.stream.stream + i;
            int field1 = w_arr[0] ? arr2int(row, w_arr[0]) : 1; // this field may not exist
            int field2 = arr2int(row+w_arr[0], w_arr[1]);
            int field3 = w_arr[2] ? arr2int(row+w_arr[0]+w_arr[1], w_arr[2]) : 0;
            //fprintf(fd, "%d %d %d %d\n", major, field1, field2, field3);
            pdf_object_table_elm *elm=&(table->table[major]);
            elm->major=major;
            elm->type=field1;
            switch (field1) {
            case 1:
                elm->offset = field2;
                elm->minor = field3;
                break;
            case 2:
                elm->obj_stm = field2;
                elm->index = field3;
            default:
                elm->minor = 0;
                elm->offset = 0;
            }
            i+=row_len;
        }
    }
    //fclose(fd);
    return 0;
}

int pdf_xreftable_get(MYFILE *f, pdf_object_table *table, long xref_poz, char *line, pdf_object *p_trailer){
	long pos=0;
	long object_id=0, object_count=0;
	size_t len=0;
	pdf_object_table_elm * elm;
	if (myfseek(f,xref_poz, SEEK_SET)==-1){
		return -1;
	}
    // bad pdf may contain a newline before 'xref'
    char c;
    do { c = mygetc(f); }
    while (isspace(c));
    myungetc(f);
	if ((myfgets(line,LLEN,f,NULL))==EOF){
        return -1;
	}
    if (!starts(line,"xref")) {
        myfseek(f,xref_poz, SEEK_SET);
        return xreftable_from_stream(f, table, p_trailer);
    }
    //FILE *fd = fopen("xref", "wb");
	while (myfgets(line,LLEN,f,NULL)!=EOF && len>=0){
        char *entry=line;
        while (isspace(*entry)) // fixes for leading spaces in xref table
            entry++;
        len=strlen(entry)-1;
        if (len==-1) continue; // skip empty lines
        while (len >= 0 && isspace((unsigned char)(entry[len]))){
            entry[len]=0;
			--len;
		}
		if(strlen(entry)==XREF_ENT_LEN){
			long object_offset_tmp;
			int object_gen_id_tmp;
			char object_used_tmp;
			if (sscanf(entry,"%ld %d %c",&object_offset_tmp,&object_gen_id_tmp,&object_used_tmp)!=3){
				break;
			}
            //fprintf(fd, "%s\n", entry);
			table=object_table_realoc(table,object_id + object_count);
			elm=&(table->table[object_id]);
            if (elm->type==0){ // skip if already set by next xreftable
                elm->offset = object_offset_tmp;
                elm->major = object_id;
                elm->minor = object_gen_id_tmp;
                elm->type = object_used_tmp=='n'? 1 : 0;
            }
			++object_id;
			--object_count;
		}
		else{
			long object_begin_tmp, object_count_tmp;
			if (sscanf(entry,"%ld %ld",&object_begin_tmp,&object_count_tmp)!=2){
				break;
			}
			object_id=object_begin_tmp;
			object_count=object_count_tmp;
		}
        pos = myftell(f);
	}
    //fclose(fd);
	if (object_count!=0){
		return -1;
	}
	while (!starts(line,"trailer")){
		if (myfgets(line,LLEN,f,NULL)!=EOF){
			message(FATAL,"trailer keyword not found\n");
			return -1;
		}
	}
    // some pdfs may have space after trailer keyword instead of newline
    // set seek pos just after trailer keyword
    myfseek(f, pos+8, SEEK_SET);
	if (p_trailer==NULL
	   || pdf_get_object(f,p_trailer,table,NULL)==-1
	   || p_trailer->type!=PDF_OBJ_DICT){
		message(FATAL,"Could not read trailer dictionary.\n");
		return -1;
	}
    return 0;
}

int getPdfHead(page_list_head * p_doc,MYFILE * f,char * line){
	long i;
	char * pom;
	pdf_doc_handle * p_pdf = (pdf_doc_handle *) p_doc->doc->doc;

	while (((i=myfgets(line,LLEN,f,NULL))!=EOF) && (!starts(line,"%PDF-")));
	if (i!=EOF)
	{
        // get pdf version
		p_pdf->v_major=strtol(line+5,&pom,10);
		if (*pom=='.'){
			pom=pom+1;
			p_pdf->v_minor=strtol(pom,&pom,10);
		}
		if (*pom!='\0'){
			message(WARN,"Missing new line after PDF head.\n");
		}
		if (p_pdf->v_major<1 || p_pdf->v_minor<4
		    || p_pdf->v_major==LONG_MIN || p_pdf->v_major==LONG_MAX
		    || p_pdf->v_minor==LONG_MIN || p_pdf->v_minor==LONG_MAX){
			p_pdf->v_major=1;
			p_pdf->v_minor=4;
		}
		return 0;
	}
	return -1;
}/*getPdfHead()*/

pdf_page_handle * pdf_page_empty(){
	pdf_page_handle * pom = (pdf_page_handle *) malloc(sizeof(pdf_page_handle));
	if (pom==NULL){
		message(FATAL,"malloc() error\n");
		return NULL;
	}
	pom->major=-1;
	return pom;
}

pdf_page_handle * pdf_page_new(const pdf_page_handle * old, pdf_doc_handle * doc){
	pdf_page_handle * pom;
	pdf_object * page, *content;
	int major;
	pom=pdf_page_empty();
	if (old){
		memcpy(pom,old,sizeof(pdf_page_handle));
	}
	else{
		assert(doc!=NULL);
		page=pdf_new_object();
		content=pdf_new_object();
		assert(pdf_get_object_from_str(content,"<</Length 1\n >> \n stream\n\n\n\nendstream")==0);
		major=object_table_ent_insert(&doc->table,content);
		free(content->val.stream.stream);
		content->val.stream.len=0;
		content->val.stream.stream = (char *) malloc(sizeof(char));
		assert(pdf_get_object_from_str(page,"<< /Type /Page \n /Parent 3 0 R \n /MediaBox [ 0 0 612 792 ] \n  /Contents 5 0 R \n  >> ")==0);
		content=pdf_add_dict_name_value(page,"Contents");
		content->type=PDF_OBJ_INDIRECT_REF;
		content->val.reference.major=major;
		content->val.reference.minor=doc->table.table[major].minor;
		major=object_table_ent_insert(&doc->table,page);
		pom->major= major;
		pom->minor=doc->table.table[major].minor;
		pom->number=0;
		pom->compressed=0;
	}
	return pom;
}

void pdf_page_delete(pdf_page_handle * page){
	if (page==NULL){
		return;
	}
	free(page);
    page=NULL;
}



// read all objects after loading xref table
int pdf_read_objects(pdf_object_table *xref, MYFILE *f) {
	/*fix for some bad xref tables*/
	if (xref->table[0].type!=0){
		xref->table[0].type=0;
		xref->table[0].offset=0;
		xref->table[0].minor=65535;
	}
    // first load nonfree objects and decompress object streams
	for (int i=1; i<xref->count; ++i) {
        //message(LOG, "reading obj %d, type %d\n", i, xref->table[i].type);
		switch (xref->table[i].type) {
            case 0:     // free obj
                break;
            case 1:     //nonfree obj
                /* some bad xref table may have offset==0, or offset > file size*/
                //message(LOG, "obj no. %d, offset %d\n", i, xref->table[i].offset);
                if (xref->table[i].offset==0){
                    xref->table[i].type=0;
                    break;
                }
                pdf_read_object(f, i, xref);
                break;
            case 2:      // compressed obj
                pdf_read_object(f, i, xref);
                xref->table[i].type=1;
            default:
                break;
		}
	}
	return 0;
}

int pdf_write_objects(pdf_object_table * xref, FILE*f){
	int i;
	for (i=1;i<xref->count;++i){
		switch (xref->table[i].type){
			case 0:
				continue;
			case 1:
				xref->table[i].offset=ftell(f);
				if (fprintf(f,"%d %d obj\n",xref->table[i].major,xref->table[i].minor)<0){
					message(FATAL,"I/O error");
				}
				if (pdf_write_object(xref->table[i].obj,f)<0){
					message(FATAL,"I/O error");
				}
				if (fprintf(f,"\nendobj\n")<0){
					message(FATAL,"I/O error");
				}
				break;
			default:
				assert(0);
		}
	}
	return 0;
}

int object_table_ent_insert(pdf_object_table * table, pdf_object * pobj){
	int i=0, prev_i=0;
	if (table->count==0){
		object_table_realoc(table,1);
		table->table[0].offset=0;
		table->table[0].minor=65635;
		table->table[0].major=0;
	}
	while(table->table[i].offset){
		prev_i=i;
		i=table->table[i].offset;
		if (table->count<i){
			return -1;
		}
	}
	table->table[prev_i].offset=0;
	if (i==0){
		object_table_realoc(table,table->count+1);
		i=table->count-1;
		table->table[i].minor=0;
	}
	table->table[i].major=i;
	table->table[i].minor=0;
	table->table[i].type=1;
	table->table[i].obj=pobj;
	return i;
}

int pdf_object_table_ent_delete(pdf_object_table * table, int major, int minor){
	int i=0;
	if (table->count<major || table->table[major].minor!=minor
	    || table->table[major].type==0)
	{
		message(FATAL, "freeing unalocated object from xref table.\n");
		return -1;
	}
	table->table[major].type=0;
	while(table->table[i].offset<major && table->table[i].offset){
		i=table->table[i].offset;
		if (table->count<i){
			return -1;
		}
	}
	table->table[major].offset=table->table[i].offset;
	table->table[i].offset=major;
	return 0;
}

int getPdfRotate(pdf_object *  p_obj, orientation * orient){
	pdf_object * pom_object;
	pom_object=pdf_get_dict_name_value(p_obj,"Rotate");
	if (pom_object==NULL || pom_object->type!=PDF_OBJ_INT){
		/*orient=DOC_O_PORTRAIT;*/
		return 0;
	}
	switch(pom_object->val.int_number){
		case 270:
		case 90:
			*orient=DOC_O_LANDSCAPE;
			break;
		default:
			*orient=DOC_O_PORTRAIT;
			break;
	}
	return 0;
}

int setPdfRotate(pdf_object *  p_obj, orientation orient){
	pdf_object * pom_object;
	pom_object=pdf_add_dict_name_value(p_obj,"Rotate");
	if (pom_object==NULL){
		return -1;
	}
	pom_object->type=PDF_OBJ_INT;
	switch(orient){
		case DOC_O_UNKNOWN:
		case DOC_O_LANDSCAPE:
			pom_object->val.int_number=90;
			break;
		case DOC_O_SEASCAPE:
			pom_object->val.int_number=270;
			break;
		case DOC_O_PORTRAIT:
			pom_object->val.int_number=0;
			break;
		case DOC_O_UPSIDE_DOWN:
			pom_object->val.int_number=180;
			break;
	}
	return 0;
}

static int pdf_get_boundaries(dimensions * dim, pdf_object * obj){
	int i;
	pdf_array * arr;
	if (dim==NULL || obj==NULL || obj->type!=PDF_OBJ_ARRAY){
		return -1;
	}
	for (arr=obj->val.array.next,i=1;arr!=(pdf_array *)&obj->val.array;arr=arr->next,++i){
		if (!isInt(arr->obj) && !isReal(arr->obj)){
			message(FATAL,"type isn't number\n");
		}
		switch(i){
			case 1:
				dim->left.x = round((isReal(arr->obj))?(arr->obj->val.real_number):(arr->obj->val.int_number));
				break;
			case 2:
				dim->left.y = round((isReal(arr->obj))?(arr->obj->val.real_number):(arr->obj->val.int_number));
				break;
			case 3:
				dim->right.x = round((isReal(arr->obj))?(arr->obj->val.real_number):(arr->obj->val.int_number));
				break;
			case 4:
				dim->right.y = round((isReal(arr->obj))?(arr->obj->val.real_number):(arr->obj->val.int_number));
				break;
			default:
				message(FATAL,"wrong boundaries\n");
		}
	}
	if (i<4){
		message(FATAL,"wrong boundaries\n");
	}
	return 0;
}

static int pdf_set_boundaries(dimensions * dim, pdf_object * obj){
	char * str;
	if (dim==NULL || obj==NULL){
		return -1;
	}
	pdf_delete_object(obj);
	asprintf(&str,"[ %d %d %d %d ]",dim->left.x,dim->left.y,
					dim->right.x,dim->right.y);
	assert (pdf_get_object_from_str(obj,str)==0);
	free(str);
	return 0;
}

static int _getPdfPages(page_list_head *p_doc, MYFILE *f, int major, int minor){
	pdf_doc_handle * p_pdf = (pdf_doc_handle *) p_doc->doc->doc;
	pdf_object * pobj, * pomobj, * kids;
	pdf_object * resources, * next_page, *tmp, *resources_tmp;
	pdf_array * arr;
	dimensions bbox = {{0,0},{0,0}};
	dimensions paper = {{0,0},{0,0}};

	get_object_(pobj,p_pdf,major,minor);
	pomobj=pdf_get_dict_name_value(pobj,"Type");
	if (pomobj==NULL || pomobj->type!=PDF_OBJ_NAME){
		message(FATAL,"Pages/Page dictionary dosn't contain Type entry\n");
	}
	/*Pages node*/
	if (strcmp("Pages",pomobj->val.name)==0){
		copy_dimensions(&paper,&p_doc->doc->paper);
		copy_dimensions(&bbox,&p_doc->doc->bbox);
		pdf_get_boundaries(&p_doc->doc->paper,pdf_get_dict_name_value(pobj,"MediaBox"));
		if (pdf_get_boundaries(&p_doc->doc->bbox,pdf_get_dict_name_value(pobj,"TrimBox")) == -1){
			if (pdf_get_boundaries(&p_doc->doc->bbox,pdf_get_dict_name_value(pobj,"BleedBox")) == -1){
				if (pdf_get_boundaries(&p_doc->doc->bbox,pdf_get_dict_name_value(pobj,"CropBox"))==0)
                    copy_dimensions(&p_doc->doc->paper,&p_doc->doc->bbox);
			}
		}

		kids=pdf_get_dict_name_value(pobj,"Kids");
		if (isRef(kids)){
			get_object_(kids,p_pdf,kids->val.reference.major,kids->val.reference.minor);
		}
		if (kids==NULL || kids->type!=PDF_OBJ_ARRAY){
			message(FATAL,"Pages dictionary dosn't contain Kids entry\n");
		}
		resources = pdf_get_dict_name_value(pobj, "Resources");
		for (arr=kids->val.array.next;arr!=(pdf_array *)&kids->val.array;arr=arr->next){
			if (arr->obj->type!=PDF_OBJ_INDIRECT_REF){
				message(FATAL,"Kids object doesn't contain indirect ref objects\n");
			}
			/*propagate inheritable resources ...*/
			if (resources){
				get_object_(next_page,p_pdf,arr->obj->val.reference.major,arr->obj->val.reference.minor);
				if ((tmp = pdf_get_dict_name_value(next_page,"Resources"))!=NULL){
					resources_tmp = pdf_new_object();
					pdf_copy_object(resources_tmp, resources);

					pdf_merge_two_dict(tmp,resources_tmp);
					pdf_delete_object(resources_tmp);
					free(resources_tmp);

				}
				else {
					tmp = pdf_add_dict_name_value(next_page, "Resources");
					pdf_copy_object(tmp,resources);
				}

			}
			_getPdfPages(p_doc,f,arr->obj->val.reference.major,arr->obj->val.reference.minor);
		}
		copy_dimensions(&p_doc->doc->paper,&paper);
		copy_dimensions(&p_doc->doc->bbox,&bbox);
		return 0;
	}
	/*Page leaf*/
	if (strcmp("Page",pomobj->val.name)==0){
		page_list * new_page;
		pdf_page_handle * content;
		new_page=page_new(NULL,0);
		new_page->page->type=p_doc->doc->type;

		if (pdf_get_boundaries(&new_page->page->paper,pdf_get_dict_name_value(pobj,"MediaBox")) == -1) {
			copy_dimensions(&new_page->page->paper,&p_doc->doc->paper);
			pdf_set_boundaries(&new_page->page->paper,pdf_add_dict_name_value(pobj,"MediaBox"));
		}

		if (pdf_get_boundaries(&new_page->page->bbox,pdf_get_dict_name_value(pobj,"TrimBox")) == -1){
			if (pdf_get_boundaries(&new_page->page->bbox,pdf_get_dict_name_value(pobj,"BleedBox")) == -1){
				if (pdf_get_boundaries(&new_page->page->bbox,pdf_get_dict_name_value(pobj,"CropBox"))==-1){
                    copy_dimensions(&new_page->page->bbox,&new_page->page->paper);
				}// in a pdfviewer, the visible page size is the CropBox
                else copy_dimensions(&new_page->page->paper, &new_page->page->bbox);
			}
		}

		pdf_filter_dict(pobj,page_filter);

		content=pdf_page_empty();
		content->major=major;
		content->minor=minor;
		content->compressed=1;
		content->number=pages_count(p_doc);
		new_page->page->page=content;
		pages_list_add_page(p_doc,new_page,pg_add_end);
		return 0;
	}
	message(FATAL,"Object isn't Page or Pages\n");
	return -1;
}


pdf_object * get_dict_name_value_type(pdf_object * obj,char * name,int type){
	pdf_object * pom;
	pom=pdf_get_dict_name_value(obj,name);
	if (pom==NULL || pom->type!=type){
		assert(0);

		message(FATAL, "entry wasn't found or isn't required type\n");
		return NULL;
	}
	return pom;
}

int getPdfPages(page_list_head * p_doc,MYFILE * f){
	pdf_doc_handle * p_pdf = (pdf_doc_handle *) p_doc->doc->doc;
	int c_major, c_minor;
	pdf_object * pobj;
	int ret_val;

	pobj=pdf_get_dict_name_value(p_pdf->trailer,"Root");//get Catalog
	if (pobj==NULL){
		message(FATAL,"Trailer dictionary doesn't contain Root entry.\n");
	}
	switch (pobj->type){
		case PDF_OBJ_INDIRECT_REF:
			c_major=pobj->val.reference.major;
			c_minor=pobj->val.reference.minor;
			get_object_(pobj,p_pdf,c_major,c_minor);
			pdf_filter_dict(pobj,catalog_filter);
			pobj=pdf_get_dict_name_value(pobj,"Pages");
			if (pobj==NULL){
				message(FATAL,"Catalog dictionary dosn't contain Pages entry.\n");
			}
			if (pobj->type!=PDF_OBJ_INDIRECT_REF){
				message(FATAL,"Pages entry isn't indirect reference.\n");
			}
			c_major=pobj->val.reference.major;
			c_minor=pobj->val.reference.minor;
			ret_val = _getPdfPages(p_doc,f,c_major,c_minor);
			update_global_dimensions(p_doc);
			return ret_val;
		default:
			message(FATAL,"Root entry isn't indirect object.\n");
	}
	return 0;
}

int getPdfTrailer(page_list_head *  p_doc, MYFILE * f, char * line,long offset){
	pdf_doc_handle * p_pdf = (pdf_doc_handle *) p_doc->doc->doc;
	unsigned char buf[PDF_TRAILER_OFFSET + 1];
	pdf_object * pom_object;
	int i, n, c;
	unsigned char * p;
	if (offset==-1){ /*first trailer*/
		if (myfseek(f, -1 * PDF_TRAILER_OFFSET,SEEK_END)==-1){
			fprintf(stderr,"Seek error\n");
			return -1;
		}
		for(n=0;n<PDF_TRAILER_OFFSET && (c=mygetc(f))!=EOF;++n){
			buf[n]=c;
		}
		buf[n]='\0';
		/* find startxref*/
        for (i = n-9; i >= 0; --i) {
		    if (!strncmp((char *)(buf+i), "startxref", 9))
		        break;
        }
	    if (i < 0) {
		    message(FATAL,"\"startxref\" not found\n");
            return -1;
        }
        for (p = buf+i+9; isspace(*p); ++p)
            offset = atol((char*)p);
	}

	pdf_object *p_trailer = pdf_new_object();
	if (pdf_xreftable_get(f,&(p_pdf->table),offset,line,p_trailer)!=0){
		message(FATAL,"xreftable read error\n");
        pdf_delete_object(p_trailer);
		return -1;
	}
	pom_object=pdf_get_dict_name_value(p_trailer,"Prev");
	if (pom_object!=NULL){
		if (pom_object->type!=PDF_OBJ_INT){
			message(FATAL,"Object in dict of trailer Prev is not int.\n");
			return -1;
		}
		if (getPdfTrailer(p_doc,f,line,pom_object->val.int_number)==-1){
			return -1;
		}

		pdf_merge_two_dict(p_trailer,p_pdf->trailer);
		free(p_pdf->trailer);
	}

	if (pdf_get_dict_name_value(p_trailer,"Encrypt")!=NULL){
		message(FATAL,"This pdf is encrypted, sorry, I cannot work with it.\n");
	}

	pdf_filter_dict(p_trailer,trailer_filter);
	p_pdf->trailer=p_trailer;
	return 0;
}


/*fci se preda pole objektu stranek, odkaz na xref,  fce vrati cislo objektu, rootu stranek */
int make_pages_tree(int * nodes, int pages_count, pdf_object_table * table){
	pdf_array * pom_array;
	int nodes_count, major=0, i, j;
	int count;
	pdf_object * node, * page, * kids, * pobj, * pom;

	nodes_count=(pages_count/NODE_MAX)+((pages_count%NODE_MAX)?1:0);
	for(i=0;i<nodes_count;++i){
		node = pdf_new_object();
		assert(pdf_get_object_from_str(node,"<< /Type /Pages /Count 0 /Kids [  ]  /Parent 0 0 R >>")!=-1);
		major=object_table_ent_insert(table,node);
		kids = get_dict_name_value_type(node, "Kids", PDF_OBJ_ARRAY);
		count=0;
		for(j=0;j<NODE_MAX;++j){
			if (pages_count<=(i * NODE_MAX + j)){
				break;
			}
			page = TABLE_GET_ELM(table,nodes[i * NODE_MAX + j]).obj;
			if (((pobj=pdf_get_dict_name_value(page, "Count"))!=NULL) && isInt(pobj)){
				count+=pobj->val.int_number;
			}
			else{
				count+=1;
			}
			pobj=get_dict_name_value_type(page,"Parent",PDF_OBJ_INDIRECT_REF);
			pobj->val.reference.major=major;
			pobj->val.reference.minor=TABLE_GET_ELM(table,major).minor;

			pobj=pdf_new_object();

			pobj->type=PDF_OBJ_INDIRECT_REF;
			pobj->val.reference.major=nodes[i * NODE_MAX + j];
			pobj->val.reference.minor=TABLE_GET_ELM(table,nodes[i * NODE_MAX + j]).minor;
			pom_array=(pdf_array *)malloc(sizeof(pdf_array));
			if (pom_array==NULL){
				return -1;
			}
			pom_array->obj=pobj;
			__list_add((pdf_array*)&(kids->val.array),pom_array);

		}
		pom = get_dict_name_value_type(node, "Count", PDF_OBJ_INT);
		pom->val.int_number = count;
		nodes[i]=major;
	}
	if (nodes_count>1){
		/*fce bude vracet major cislo hlavy stranek*/
		return make_pages_tree(nodes,nodes_count,table);
	}else{
		if(nodes_count==1){
			/*smazat polozku parent z uzlu*/
			page = TABLE_GET_ELM(table,nodes[0]).obj;
			pdf_filter_dict(page,parent_filter);
			return major;
		}else{
			return -1;
		}
	}
}


int putPdfPages(page_list_head * p_doc){
	page_list * page;
	pdf_object * pobj;
	int count;
	int * nodes;

	if (pages_count(p_doc)<1){
		message(FATAL, "Cannot create PDF with zero pages.\n");
	}
	nodes = (int *)malloc(sizeof(int)* pages_count(p_doc));
	assert(nodes!=NULL);

	for (page=p_doc->next, count=0;page!=(page_list *)p_doc;page=page->next,++count){
		nodes[count]=get_page_content(page)->major;
		get_object_(pobj,get_pdf_handle(p_doc),get_page_content(page)->major,get_page_content(page)->minor);
 		pdf_set_boundaries(&page->page->paper,pdf_get_dict_name_value(pobj,"MediaBox"));

		if (!isdimzero(page->page->bbox)){
			pdf_set_boundaries(&page->page->bbox,pdf_add_dict_name_value(pobj,"TrimBox"));
		}
	}
	make_pages_tree(nodes,count,get_obj_table(p_doc));

	/*pridani objektu pages do seznamu stranek*/
	pobj=get_dict_name_value_type(get_pdf_handle(p_doc)->trailer,"Root",PDF_OBJ_INDIRECT_REF);
	pobj=(TABLE_GET_ELM(get_obj_table(p_doc),pobj->val.reference.major)).obj;
	pobj=get_dict_name_value_type(pobj,"Pages",PDF_OBJ_INDIRECT_REF);
	pobj->val.reference.major=nodes[0];
	pobj->val.reference.minor=TABLE_GET_ELM(get_obj_table(p_doc),nodes[0]).minor;
	free(nodes);
	return 0;
}

int pdf_save(page_list_head * p_doc, const char * filename){
	FILE * f = stdout;
	long table_poz;
	pdf_object * pobj;
	pdf_doc_handle * p_pdf = NULL;
	int  * pages = NULL;
	char binary[] = {(char)0xDE,(char)0xAD,' ',(char)0xBE,(char)0xEF,'\n',0};
	if (strcmp(filename,"-")){
		f = fopen(filename,"wb");
		if (f==NULL){
			message(FATAL, "Cannot open for writing file \"%s\"\n",filename);
		}
	}


	if (get_pdf_handle(p_doc)->refcounter>1){
		page_list * page;
		int i;
		p_pdf = get_pdf_handle(p_doc);
		p_doc->doc->doc = pdf_structure_copy_w(get_pdf_handle(p_doc));
		get_pdf_handle(p_doc)->p_doc = p_doc;
		pages = (int *) malloc(sizeof(int) * 2 * pages_count(p_doc));
		assert(pages!=NULL);
		page = page_next(page_begin(p_doc));
		for (page = page_next(page_begin(p_doc)), i=0;page!=page_end(p_doc);page=page_next(page), ++i){
			pages[i]=get_page_content(page)->major;
			pages[i + pages_count(p_doc)]=get_page_content(page)->minor;

		}
	}
	fprintf(f,"%%PDF-%ld.%ld\n",get_pdf_handle(p_doc)->v_major,get_pdf_handle(p_doc)->v_minor);
	fprintf(f,binary);


	putPdfPages(p_doc);
	object_table_delete_unused(p_doc);
	pdf_write_objects(get_obj_table(p_doc),f);
	table_poz=ftell(f);
	object_table_write(get_obj_table(p_doc),f);
	pobj=get_dict_name_value_type(get_pdf_handle(p_doc)->trailer,"Size",PDF_OBJ_INT);
	pobj->val.int_number=get_obj_table(p_doc)->count;
	fprintf(f,"trailer\n");
	pdf_write_object(get_pdf_handle(p_doc)->trailer,f);
	fprintf(f,"\nstartxref\n%ld\n%%%%EOF\n",table_poz);
	fclose(f);
	if (p_pdf != NULL){
		page_list * page;
		int i;
		pdf_structure_delete(get_pdf_handle(p_doc));
		p_pdf->refcounter+=1;
		p_doc->doc->doc = p_pdf;
		for (page = page_next(page_begin(p_doc)), i=0;page!=page_end(p_doc);page=page_next(page), ++i){
			get_page_content(page)->major=pages[i];
			get_page_content(page)->minor=pages[i + pages_count(p_doc)];
		}
		free(pages);
        pages=NULL;
	}
	return 0;
}

int pdf_open(page_list_head * p_doc,const char * fname){
	MYFILE * f;
	char iobuffer[LLEN];

	pdf_doc_handle *  p_pdf=pdf_structure_new(NULL);
	if (p_pdf==NULL){
		return -1;
	}

	p_doc->doc->doc=p_pdf;
	p_pdf->p_doc=p_doc;

	if ((f=myfopen(fname,"rb"))==NULL){
		pdf_structure_delete(p_pdf);
		return -1;
	}
	if (getPdfHead(p_doc,f,iobuffer)==-1
	    || getPdfTrailer(p_doc,f,iobuffer,-1)==-1){
		pdf_structure_delete(p_pdf);
		return -1;
	}

	if (pdf_read_objects(&p_pdf->table,f)==-1){
		pdf_structure_delete(p_pdf);
		return -1;
	}
	getPdfPages(p_doc,f);
	myfclose(f);

	return 0;
}/*END read_open()*/

// insert structure 2 into structure 1
int pdf_structure_merge(pdf_doc_handle * s1, pdf_doc_handle * s2){
	int offset;
	int i;
	if (s1==s2){
		return 0;
	}
	offset=s1->table.count;
	object_table_realoc(&s1->table,s1->table.count+s2->table.count-1);
	for (i=1;i<s2->table.count;++i){
		s2->table.table[i].major=offset+i-1;
		s2->table.table[i].minor=0;
	}
	update_page_ref(s2->p_doc);
	update_obj_ref(s2->p_doc);

	for (i=1;i<s2->table.count;++i){
		memcpy(&s1->table.table[s2->table.table[i].major],&s2->table.table[i],sizeof(pdf_object_table_elm));
		s2->table.table[i].obj=NULL;// no need to free, as same object is pointed by s1
		s2->table.table[i].type=0;
	}
	return 0;
}

pdf_doc_handle * pdf_structure_new(pdf_doc_handle * old){
	pdf_doc_handle *  p_pdf;

	if (old){
		old->refcounter++;
		return old;
	}

	p_pdf=(pdf_doc_handle *) malloc(sizeof(pdf_doc_handle));

	if (p_pdf==NULL){
		vdoc_errno=VDOC_ERR_LIBC;
		return NULL;
	}

	memset(p_pdf,0,sizeof(pdf_doc_handle));
	p_pdf->refcounter=1;
	p_pdf->trailer=NULL;
	return p_pdf;
}

pdf_doc_handle * pdf_structure_copy_w(pdf_doc_handle * p_pdf){
	pdf_doc_handle * new;
	assert(p_pdf->refcounter>0);
	if (p_pdf->refcounter==1){
		return p_pdf;
	}
	p_pdf->refcounter-=1;

	new=(pdf_doc_handle *) malloc(sizeof(pdf_doc_handle));

	if (new==NULL){
		vdoc_errno=VDOC_ERR_LIBC;
		return NULL;
	}

	memcpy(new, p_pdf, sizeof(pdf_doc_handle));
	new->refcounter=1;
	new->trailer = pdf_new_object();
	pdf_copy_object(new->trailer,p_pdf->trailer);
	object_table_copy(&(new->table), &(p_pdf->table));

	return new;

}

void pdf_structure_delete(pdf_doc_handle * p_pdf){
	if (p_pdf==NULL){
		return;
	}
	if (p_pdf->refcounter>1){
		p_pdf->refcounter-=1;
		return;
	}

	if (p_pdf->trailer){
		pdf_delete_object(p_pdf->trailer);
	}
	free(p_pdf->trailer);
    p_pdf->trailer=NULL;
	object_table_delete(&p_pdf->table);
	free(p_pdf);
    p_pdf=NULL;
	return;
}

static int pdf_page_to_xobj(page_handle * pg_handle){
	int major;
	pdf_object * new_page, * new_page_contents, * new_page_xobject, * contents, * pg, * pom, * xobj_val;
	pdf_page_handle * page = (pdf_page_handle *)pg_handle->page;
	pdf_doc_handle * p_pdf = get_pdf_handle(pg_handle);
	char * str;
	char * xobjname;
	char * stream_content = NULL;
	static int revision = 1;
	if (((pdf_page_handle *)pg_handle->page)->compressed==0){
		return 0;
	}

	/*get_page_object*/
	get_object_(pg,p_pdf,page->major,page->minor);

/*	pdf_set_boundaries(&pg_handle->paper,pdf_add_dict_name_value(pg,"MediaBox"));*/

	/*create new_page object*/
	new_page=pdf_new_object();
	assert(pdf_get_object_from_str(new_page,"<</Type /Page \n  /Contents 0 0 R  /Parent 0 0 R /Resources << /ProcSet [ /PDF ]  /XObject <<  >> \n >> \n >>")==0);
	pdf_set_boundaries(&pg_handle->paper,pdf_add_dict_name_value(new_page,"MediaBox"));
	pdf_set_boundaries(&pg_handle->bbox,pdf_add_dict_name_value(new_page,"TrimBox"));

	new_page_contents = pdf_get_dict_name_value(new_page,"Contents");
	assert(new_page_contents);
	new_page_xobject = pdf_get_dict_name_value(new_page, "Resources");
	assert(new_page_xobject);
	new_page_xobject = pdf_get_dict_name_value(new_page_xobject, "XObject");
	assert(new_page_xobject);

	/*add new page to xref table*/
	major=object_table_ent_insert(&p_pdf->table,new_page);
	((pdf_page_handle *)pg_handle->page)->major=major;
	((pdf_page_handle *)pg_handle->page)->minor=p_pdf->table.table[major].minor;
	((pdf_page_handle *)pg_handle->page)->compressed=0;


	pom=pdf_get_dict_name_value(pg,"Contents");
	if (pom==NULL){
		asprintf(&stream_content,"q Do Q");
	} else {
		switch (pom->type){
			case PDF_OBJ_INDIRECT_REF:
				do{
					get_object_(contents,p_pdf,pom->val.reference.major,pom->val.reference.minor);
					pom = contents;
				} while (contents->type == PDF_OBJ_INDIRECT_REF);
				if (isStream(contents)){
					major = stream_to_xobj(p_pdf,contents,pg,NULL);
					asprintf(&xobjname,"xo%d",revision++);
					xobj_val = pdf_add_dict_name_value(new_page_xobject,xobjname);

					xobj_val->type = PDF_OBJ_INDIRECT_REF;
					xobj_val->val.reference.major = major;
					xobj_val->val.reference.minor = p_pdf->table.table[major].minor;

					asprintf(&stream_content,"q /%s Do Q",xobjname);
					free(xobjname);
					break;
				}
				if (!isArray(contents)) {
					message(FATAL, "I excepting array type, but it isn't.\n");
				}
			case PDF_OBJ_ARRAY:
				{
					pdf_array * p_array = pom->val.array.next;
					pdf_object * new_stream;
					pdf_object * tmp_stream;
					if (p_array==(pdf_array*)&pom->val.array){
						asprintf(&stream_content," ");
						break;
					}
					else {
						major = pdf_decompress_stream(p_pdf,p_array->obj->val.reference.major,p_array->obj->val.reference.minor);
					}

					get_object_(new_stream,p_pdf,major,p_pdf->table.table[major].minor);
					for (p_array=p_array->next;p_array!=(pdf_array*)&pom->val.array;p_array=p_array->next){

						major = pdf_decompress_stream(p_pdf,p_array->obj->val.reference.major,p_array->obj->val.reference.minor);
						if (major == -1){
							message(WARN,"I haven't filter for decompress stream,\n I'll convert it to Xobject,"
								    "but it will not be correct in some cases.\n");
							goto to_xobj;
						}
						get_object_(tmp_stream,p_pdf,major,p_pdf->table.table[major].minor);
						major = pdf_merge_stream(p_pdf,new_stream,tmp_stream,NULL,NULL," ");
						get_object_(new_stream,p_pdf,major,p_pdf->table.table[major].minor);

					}
					major = stream_to_xobj(p_pdf,new_stream,pg,NULL);
	#ifdef HAVE_ZLIB
					major = pdf_compress_stream(p_pdf,major,p_pdf->table.table[major].minor, "FlateDecode");
					assert(major!=-1);
	#endif
					asprintf(&xobjname,"xo%d",revision++);
					xobj_val = pdf_add_dict_name_value(new_page_xobject,xobjname);

					xobj_val->type = PDF_OBJ_INDIRECT_REF;
					xobj_val->val.reference.major = major;
					xobj_val->val.reference.minor = p_pdf->table.table[major].minor;

					asprintf(&stream_content,"q /%s Do Q",xobjname);
					free(xobjname);

				}
				break;
			to_xobj:
				{
					pdf_array * p_array;
					char * tmp_stream_content;
					for (p_array=pom->val.array.next;p_array!=(pdf_array*)&pom->val.array;p_array=p_array->next){
						major = stream_to_xobj(p_pdf,p_array->obj,pg,NULL);
						asprintf(&xobjname,"xo%d",revision++);
						pdf_add_dict_name_value(new_page_xobject,xobjname);
						xobj_val = pdf_add_dict_name_value(new_page_xobject,xobjname);
						xobj_val->type = PDF_OBJ_INDIRECT_REF;

						xobj_val->val.reference.major = major;
						xobj_val->val.reference.minor = p_pdf->table.table[major].minor;
						if (stream_content == NULL){
							asprintf(&stream_content," /%s Do ",xobjname);
						}
						else{
							tmp_stream_content = stream_content;
							asprintf(&stream_content,"%s q /%s Do Q",tmp_stream_content,xobjname);
							free(tmp_stream_content);
						}
					}
				}
				break;
			default:
				assert(0);
		}
	}



	/*create new_page contents*/
	contents=pdf_new_object();
	asprintf(&str,"<</Length %lu\n>>\nstream\n%s\nendstream",(long unsigned int)strlen(stream_content),stream_content);
	free(stream_content);
	assert(pdf_get_object_from_str(contents, str)==0);
	free(str);
	major=object_table_ent_insert(&p_pdf->table,contents);
	new_page_contents->type = PDF_OBJ_INDIRECT_REF;
	new_page_contents->val.reference.major = major;
	new_page_contents->val.reference.minor = p_pdf->table.table[major].minor;

	return 0;
}
int pdf_merge_stream(pdf_doc_handle * p_pdf,pdf_object * stream1, pdf_object * stream2, char * begin, char * end, char * sep){
	char * str, * ptn;
	pdf_object * stream;
	stream = pdf_new_object();
	asprintf(&str,"<</Length %d\n>>\nstream\n%s\nendstream",1, " ");
	assert(pdf_get_object_from_str(stream, str)==0);
	free(str);
	free(stream->val.stream.stream);
	stream->val.stream.len=(begin?strlen(begin):0) + (stream1?stream1->val.stream.len:0) + (sep?strlen(sep):0) + (stream2?stream2->val.stream.len:0) + (end?strlen(end):0);
	stream->val.stream.stream = (char *) malloc(sizeof(char) * stream->val.stream.len);
	ptn = stream->val.stream.stream;

	if (begin){
		memcpy(ptn,begin, strlen(begin));
		ptn += strlen(begin);
	}

	if (stream1){
		memcpy(ptn,stream1->val.stream.stream,stream1->val.stream.len);
		ptn += stream1->val.stream.len;
	}

	if (sep){
		memcpy(ptn,sep, strlen(sep));
		ptn += strlen(sep);
	}

	if (stream2){
		memcpy(ptn,stream2->val.stream.stream,stream2->val.stream.len);
		ptn += stream2->val.stream.len;
	}

	if (end){
		memcpy(ptn,end, strlen(end));
		ptn += strlen(end);
	}

	return object_table_ent_insert(&p_pdf->table,stream);
}

int pdf_page_transform(page_handle * pg_handle, transform_matrix * matrix){
	int major;
	pdf_object * new_page, * page1, * pom, * stream1;
	char * ct;
	pdf_doc_handle * p_pdf = (pdf_doc_handle *)pg_handle->doc->doc;
	if (matrix == NULL
	    || ((*matrix)[0][0]==1 && (*matrix)[0][1]==0 && (*matrix)[0][2]==0
	     && (*matrix)[1][0]==0 && (*matrix)[1][1]==1 && (*matrix)[1][2]==0
	     && (*matrix)[2][0]==0 && (*matrix)[2][1]==0 && (*matrix)[2][2]==1) ){
		return 0;
	}
	assert(p_pdf!=NULL);
	pdf_page_to_xobj(pg_handle);
	get_object_(page1,p_pdf,(((pdf_page_handle *)pg_handle->page)->major),(((pdf_page_handle *)pg_handle->page)->minor));
	new_page=pdf_new_object();
	pdf_copy_object(new_page,page1);

	pom=pdf_get_dict_name_value(page1,"Contents");
	get_object_(stream1,p_pdf, pom->val.reference.major, pom->val.reference.minor);
	if (stream1->val.stream.len==0){
		return 0;
	}

	asprintf(&ct,"q %.2f %.2f %.2f %.2f %.2f %.2f cm\n",   (*matrix)[0][0], (*matrix)[0][1],
								 (*matrix)[1][0], (*matrix)[1][1],
								 (*matrix)[2][0], (*matrix)[2][1]);
	major = pdf_merge_stream(p_pdf,stream1,NULL,ct," Q",NULL);
	free(ct);

	pom=pdf_add_dict_name_value(new_page,"Contents");
	pom->type=PDF_OBJ_INDIRECT_REF;
	pom->val.reference.major=major;
	pom->val.reference.minor=p_pdf->table.table[major].minor;

	major=object_table_ent_insert(&p_pdf->table,new_page);
	((pdf_page_handle *)pg_handle->page)->major=major;
	((pdf_page_handle *)pg_handle->page)->minor=p_pdf->table.table[major].minor;
	return 0;
}

int pdf_page_line(page_handle * pg_handle, const coordinate * begin, const coordinate * end, int width){
	char * ch;
	int major;
	pdf_doc_handle * p_pdf = (pdf_doc_handle *)pg_handle->doc->doc;
	pdf_object * page, * new_page, * pom, * stream;
	asprintf(&ch," q %d w %d %d m %d %d l S Q", width, begin->x, begin->y, end->x, end->y);
	assert(p_pdf!=NULL);
	pdf_page_to_xobj(pg_handle);
	get_object_(page,p_pdf,(((pdf_page_handle *)pg_handle->page)->major),(((pdf_page_handle *)pg_handle->page)->minor));
	new_page=pdf_new_object();
	pdf_copy_object(new_page,page);

	pom=pdf_get_dict_name_value(page,"Contents");
	get_object_(stream,p_pdf, pom->val.reference.major, pom->val.reference.minor);
	major = pdf_merge_stream(p_pdf,stream,NULL,NULL,ch,NULL);
	free(ch);

	pom=pdf_add_dict_name_value(new_page,"Contents");
	pom->type=PDF_OBJ_INDIRECT_REF;
	pom->val.reference.major=major;
	pom->val.reference.minor=p_pdf->table.table[major].minor;

	major=object_table_ent_insert(&p_pdf->table,new_page);
	((pdf_page_handle *)pg_handle->page)->major=major;
	((pdf_page_handle *)pg_handle->page)->minor=p_pdf->table.table[major].minor;
	return 0;
}


int pdf_page_text(page_handle * pg_handle, const coordinate * where, const char * text,int size, const char * font){
/*TODO: dopsat*/
	char * ch, * str;
	pdf_object * font_obj, *page, *new_page, *stream, * pom;
	pdf_doc_handle * p_pdf = (pdf_doc_handle *)pg_handle->doc->doc;
	int major;


	assert(p_pdf!=NULL);
	pdf_page_to_xobj(pg_handle);
	get_object_(page,p_pdf,(((pdf_page_handle *)pg_handle->page)->major),(((pdf_page_handle *)pg_handle->page)->minor));
	new_page=pdf_new_object();
	pdf_copy_object(new_page,page);

	pom=pdf_get_dict_name_value(page,"Contents");
	get_object_(stream,p_pdf, pom->val.reference.major, pom->val.reference.minor);


	if (font == NULL){
		font = "Helvetica";
	}

	pom=pdf_add_dict_name_value(new_page,"Resources");
	if (pom->type!=PDF_OBJ_DICT){
		pom->type=PDF_OBJ_DICT;
		pom->val.dict.next=(pdf_dict*)&(pom->val.dict);
		pom->val.dict.prev=(pdf_dict*)&(pom->val.dict);
	}
	pom=pdf_add_dict_name_value(pom, "Font");
	if (pom->type!=PDF_OBJ_DICT){
		pom->type=PDF_OBJ_DICT;
		pom->val.dict.next=(pdf_dict*)&(pom->val.dict);
		pom->val.dict.prev=(pdf_dict*)&(pom->val.dict);
	}
	asprintf(&ch,"F%s",font);
	pom=pdf_add_dict_name_value(pom,ch);
	free(ch);
	if (pom->type!=PDF_OBJ_DICT){
		asprintf(&str,"<< /Type /Font /Subtype /Type1 /BaseFont /%s /Name /F%s /Encoding /MacRomanEncoding >>",font, font);
		font_obj = pdf_new_object();
		assert(pdf_get_object_from_str(font_obj,str)==0);
		free(str);
		major=object_table_ent_insert(&p_pdf->table,font_obj);
		pom->type=PDF_OBJ_INDIRECT_REF;
		pom->val.reference.major=major;
		pom->val.reference.minor=p_pdf->table.table[major].minor;
	}
	asprintf(&ch, " q BT /F%s %d Tf \n %d %d Td\n (%s) Tj ET Q", font ,size, where->x, where->y, text);
	major = pdf_merge_stream(p_pdf,NULL,stream,NULL,ch,NULL);
	free(ch);

	pom=pdf_add_dict_name_value(new_page,"Contents");
	pom->type=PDF_OBJ_INDIRECT_REF;
	pom->val.reference.major=major;
	pom->val.reference.minor=p_pdf->table.table[major].minor;

	major=object_table_ent_insert(&p_pdf->table,new_page);
	((pdf_page_handle *)pg_handle->page)->major=major;
	((pdf_page_handle *)pg_handle->page)->minor=p_pdf->table.table[major].minor;
	return 0;
}

int pdf_page_crop(page_handle * pg_handle,dimensions * dimensions){
	char * ch;
	int major;
	pdf_doc_handle * p_pdf = (pdf_doc_handle *)pg_handle->doc->doc;
	pdf_object * page, * new_page, * pom, * stream;
	assert(p_pdf!=NULL);
	pdf_page_to_xobj(pg_handle);
	get_object_(page,p_pdf,(((pdf_page_handle *)pg_handle->page)->major),(((pdf_page_handle *)pg_handle->page)->minor));
	new_page=pdf_new_object();
	pdf_copy_object(new_page,page);

	pom=pdf_get_dict_name_value(page,"Contents");
	get_object_(stream,p_pdf, pom->val.reference.major, pom->val.reference.minor);
	if (stream->val.stream.len==0){
		return 0;
	}
	asprintf(&ch,"q %d %d %d %d re W n ", dimensions->left.x, dimensions->left.y, dimensions->right.x - dimensions->left.x,  dimensions->right.y - dimensions->left.y);
	major = pdf_merge_stream(p_pdf,NULL,stream,ch," Q",NULL);
	free(ch);

	pom=pdf_add_dict_name_value(new_page,"Contents");
	pom->type=PDF_OBJ_INDIRECT_REF;
	pom->val.reference.major=major;
	pom->val.reference.minor=p_pdf->table.table[major].minor;

	major=object_table_ent_insert(&p_pdf->table,new_page);
	((pdf_page_handle *)pg_handle->page)->major=major;
	((pdf_page_handle *)pg_handle->page)->minor=p_pdf->table.table[major].minor;
	return 0;
}


int stream_to_xobj(pdf_doc_handle * p_pdf, pdf_object * contents, pdf_object * pg,  transform_matrix * matrix){
	pdf_object * xobj, * pom, * pom2;
#ifdef TMATRIX
	char * str;
#endif
	while (isRef(contents)){
		get_object_(contents,p_pdf,contents->val.reference.major,contents->val.reference.minor);
	}
	assert(isStream(contents));
	xobj = pdf_new_object();

	pdf_copy_object(xobj,contents);

	pom=pdf_add_dict_name_value(xobj->val.stream.dict,"Type");
	pdf_delete_object(pom);
	assert(pdf_get_object_from_str(pom,"/XObject")!=-1);

	pom=pdf_add_dict_name_value(xobj->val.stream.dict,"Subtype");
	pdf_delete_object(pom);
	assert(pdf_get_object_from_str(pom,"/Form")!=-1);
	pom=pdf_add_dict_name_value(xobj->val.stream.dict,"FormType");
	pdf_delete_object(pom);
	pom->type=PDF_OBJ_INT;
	pom->val.int_number=1;

	pom=pdf_add_dict_name_value(xobj->val.stream.dict,"BBox");
	pdf_delete_object(pom);
	pom2=get_dict_name_value_type(pg, "MediaBox", PDF_OBJ_ARRAY);
	pdf_copy_object(pom,pom2);

#ifdef TMATRIX
	pom=pdf_add_dict_name_value(xobj->val.stream.dict,"Matrix");
	pdf_delete_object(pom);
	asprintf(&str,"[ %.2f %.2f %.2f %.2f %.2f %.2f ]",   (*matrix)[0][0], (*matrix)[0][1],
								 (*matrix)[1][0], (*matrix)[1][1],
								 (*matrix)[2][0], (*matrix)[2][1]);
	assert(pdf_get_object_from_str(pom,str)==0);
	free(str);
#endif
	pom2=pdf_get_dict_name_value(pg,"Resources");
	if (pom2!=NULL){
		pom=pdf_add_dict_name_value(xobj->val.stream.dict,"Resources");
		pdf_delete_object(pom);

		switch(pom2->type){
		case PDF_OBJ_INDIRECT_REF:
			get_object_(pom2,p_pdf, pom2->val.reference.major, pom2->val.reference.minor);
		case PDF_OBJ_DICT:
			pdf_copy_object(pom,pom2);
			break;
		default:
			assert(0);

		}
	}
	pdf_filter_dict(xobj,xobject_filter);
	return object_table_ent_insert(&p_pdf->table, xobj);

}


int pdf_page_merge(page_handle * p1, page_handle *p2){
	int major;
	pdf_object * new_page, * page1,* page2, * xobj1, * xobj2, * pom, *stream1, *stream2;
	pdf_doc_handle * p_pdf= (pdf_doc_handle *)p1->doc->doc;
	pdf_structure_merge((pdf_doc_handle *) p1->doc->doc,(pdf_doc_handle *)p2->doc->doc);
	assert(p_pdf!=NULL);
	pdf_page_to_xobj(p1);
	pdf_page_to_xobj(p2);

	get_object_(page1,p_pdf,(((pdf_page_handle *)p1->page)->major),(((pdf_page_handle *)p1->page)->minor));
	get_object_(page2,p_pdf,(((pdf_page_handle *)p2->page)->major),(((pdf_page_handle *)p2->page)->minor));

	new_page=pdf_new_object();
	pdf_copy_object(new_page,page1);
	xobj1=pdf_get_dict_name_value(pdf_get_dict_name_value(new_page,"Resources"), "XObject");
	xobj2=pdf_get_dict_name_value(pdf_get_dict_name_value(page2,"Resources"), "XObject");
	if (xobj2!=NULL){
		if (xobj1==NULL){
			xobj1=pdf_add_dict_name_value(new_page,"Resources");
			if (xobj1->type==PDF_OBJ_UNKNOWN){
				assert(pdf_get_object_from_str(xobj1,"<< >>")==0);
			}
			xobj1=pdf_add_dict_name_value(xobj1,"XObject");
			assert(pdf_get_object_from_str(xobj1,"<< >>")==0);
		}
		pom=pdf_new_object();
		pdf_copy_object(pom,xobj2);
		pdf_merge_two_dict(xobj1, pom);
		free(pom);
	}

	pom=pdf_get_dict_name_value(page1,"Contents");
	get_object_(stream1,p_pdf, pom->val.reference.major, pom->val.reference.minor);
	pom=pdf_get_dict_name_value(page2,"Contents");
	get_object_(stream2,p_pdf, pom->val.reference.major, pom->val.reference.minor);

	major = pdf_merge_stream(p_pdf,stream1,stream2,NULL,NULL," ");
	pom=pdf_add_dict_name_value(new_page,"Contents");
	pom->type=PDF_OBJ_INDIRECT_REF;
	pom->val.reference.major=major;
	pom->val.reference.minor=p_pdf->table.table[major].minor;
	pdf_set_boundaries(&p1->paper,pdf_add_dict_name_value(new_page,"MediaBox"));
	if (!isdimzero(p1->doc->bbox)){
		pdf_set_boundaries(&p1->bbox,pdf_add_dict_name_value(new_page,"TrimBox"));
	}
	major=object_table_ent_insert(&p_pdf->table,new_page);
	((pdf_page_handle *)p1->page)->major=major;
	((pdf_page_handle *)p1->page)->minor=p_pdf->table.table[major].minor;
	return 0;
}


int pdf_compress_stream(pdf_doc_handle * p_pdf, int major, int minor, char * filter){
	pdf_object * stream;
	pdf_object * new_stream;
	pdf_object * p_obj;
	char * ch;
	pdf_array * pom_array;
	get_object_(stream,p_pdf,major,minor);
	assert(isStream(stream));
#ifdef COPY_STREAM
	new_stream = pdf_new_object();
	pdf_copy_object(new_stream,stream);
	major=object_table_ent_insert(&(p_pdf->table),new_stream);
#else
	new_stream = stream;
#endif

	if (apply_compress_filter(filter, &(new_stream->val.stream.stream), &(new_stream->val.stream.len),new_stream->val.stream.dict) != 0){
		return -1;
	}

	p_obj = pdf_get_dict_name_value(new_stream->val.stream.dict,"Filter");
	if (p_obj == NULL) {
		p_obj = pdf_add_dict_name_value(new_stream->val.stream.dict,"Filter");
		asprintf(&ch,"/%s",filter);
		assert(pdf_get_object_from_str(p_obj,ch)==0);
		free(ch);
	}
	else{
	  	switch (p_obj->type){
		case PDF_OBJ_ARRAY:
			pom_array=(pdf_array *)malloc(sizeof(pdf_array));
			if (pom_array==NULL){
				return -1;
			}
			__list_add((pdf_array*)&p_obj->val.array,pom_array);
			pom_array->obj = pdf_new_object();
			asprintf(&ch,"/%s",filter);
			assert(pdf_get_object_from_str(pom_array->obj,ch)==0);
			free(ch);
			break;
		case PDF_OBJ_NAME:
			asprintf(&ch," [ /%s /%s ] ",p_obj->val.name,filter);
			pdf_delete_object(p_obj);
			assert(pdf_get_object_from_str(p_obj,ch)==0);
			free(ch);
			break;
		default:
			assert(0);
		}
	}
	return major;

}


int pdf_decompress_stream(pdf_doc_handle * p_pdf, int major, int minor){
	pdf_object * stream;
	pdf_object * new_stream;
	pdf_object * p_obj;
	pdf_array *  p_array;
	pdf_object * tmp;

	get_object_(stream,p_pdf,major,minor);
	assert(isStream(stream));
	p_obj = pdf_get_dict_name_value(stream->val.stream.dict,"Filter");
	if (p_obj == NULL) {
		return major;
	}
    /* pdf_write_object(stream,stdout);*/
	switch (p_obj->type){
	case PDF_OBJ_ARRAY:
		{
		new_stream = pdf_new_object();
		pdf_copy_object(new_stream,stream);
		pdf_del_dict_name_value(new_stream->val.stream.dict, "Filter");
		major=object_table_ent_insert(&(p_pdf->table),new_stream);

		for (p_array = p_obj->val.array.next;p_array!=(pdf_array *)&(p_obj->val.array);p_array=p_array->next){
			tmp = p_array->obj;
			while (tmp->type == PDF_OBJ_INDIRECT_REF){
				get_object_(tmp,p_pdf,tmp->val.reference.major,tmp->val.reference.minor);
			}

			if (apply_decompress_filter(tmp->val.name, &(new_stream->val.stream.stream), &(new_stream->val.stream.len),new_stream->val.stream.dict) != 0){
				return -1;
			}
		}
		return major;
		}
	case PDF_OBJ_NAME:
		new_stream = pdf_new_object();
		pdf_copy_object(new_stream,stream);
		major=object_table_ent_insert(&(p_pdf->table),new_stream);
		p_obj = pdf_get_dict_name_value(new_stream->val.stream.dict, "Filter");
		if (apply_decompress_filter(p_obj->val.name, &(new_stream->val.stream.stream), &(new_stream->val.stream.len),new_stream->val.stream.dict) == 0){
			pdf_del_dict_name_value(new_stream->val.stream.dict, "Filter");
			return major;
		}
		else{
			return  -1;
		}
	default:
		assert(0);
		return -1;
	}
	return -1;
}

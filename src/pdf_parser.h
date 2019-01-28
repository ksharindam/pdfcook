/** \file pdf_parser.h 
*   \brief Definice zakladnich datovych struktur a funkci pro praci s PDF dokumentem
**/

#ifndef _PDF_PARSER_H_
#define _PDF_PARSER_H_
#include "fileio.h"

#define PDF_NAME_MAX_LEN 255
#define PDF_ID_MAX_LEN 255


#define __list_add(list,elm)((elm)->next=(list), (elm)->prev=(list)->prev, (elm)->prev->next=elm, (elm)->next->prev=(elm))
#define __list_elm_remove(elm)(elm->next->prev=elm->prev, elm->prev->next=elm->next)

#define isDict(o)(((o)!=NULL) && (o)->type==PDF_OBJ_DICT)
#define isInt(o)(((o)!=NULL) && (o)->type==PDF_OBJ_INT)
#define isRef(o)(((o)!=NULL) && (o)->type==PDF_OBJ_INDIRECT_REF)
#define isIndirect(o)(((o)!=NULL) && (o)->type==PDF_OBJ_INDIRECT)
#define isReal(o)(((o)!=NULL) && (o)->type==PDF_OBJ_REAL)
#define isStr(o)(((o)!=NULL) && (o)->type==PDF_OBJ_STR)
#define isStream(o)(((o)!=NULL) && (o)->type==PDF_OBJ_STREAM)
#define isNull(o)(((o)!=NULL) && (o)->type==PDF_OBJ_NULL)
#define isName(o)(((o)!=NULL) && (o)->type==PDF_OBJ_NAME)
#define isArray(o)(((o)!=NULL) && (o)->type==PDF_OBJ_ARRAY)


/**konstanty definujici bile znaky*/
enum  { char_null=0, char_tab=9, char_lf=10, char_ff=12, char_cr=13, char_sp=32};
/**konstanty identifikujici typ tokenu*/
typedef enum pdf_tok_type { PDF_T_INT, PDF_T_REAL, PDF_T_STR, PDF_T_BOOL, PDF_T_NAME, PDF_T_NULL,
			    PDF_T_EOLN, PDF_T_EOF, PDF_T_BDICT, PDF_T_EDICT, PDF_T_BARRAY, 
			    PDF_T_EARRAY, PDF_T_ID, PDF_T_UNKNOWN} pdf_tok_type; 
/**konstanty definujici typ stringu*/
typedef enum pdf_string_type { PDF_STR_CHR, PDF_STR_HEX }pdf_string_type;
/**struktura pro reprezentaci retezce*/
typedef struct pdf_string{
	pdf_string_type type;/**<typ stringu*/
	char * str;/**<odkaz na buffer stringu*/
}pdf_string;
/**realne cislo*/
typedef double pdf_real_number;
/**celociselna hodnota*/
typedef int pdf_int_number;
/**jmeno*/
typedef char *  pdf_name;
/**ID*/
typedef char * pdf_id;
/**struktura, ktera se predava tokenizatoru*/
typedef struct pdf_tok{
	pdf_tok_type type;/*<typ tokenu*/
	pdf_real_number real_number;/*<realn hodnota tokenu*/
	pdf_int_number int_number;/*<celociselna hodnota tokenu*/
	char name[PDF_NAME_MAX_LEN];/*<jmeno tokenu*/
	char id[PDF_ID_MAX_LEN];/*<id tokenu*/
	pdf_string str;/**<string tokenu*/
	int new_line;/**<pred tokenem se nachazel novy radek*/
	int sign;/**<znamenko ciselneho typu*/
}pdf_tok;

/**objekt typu reference*/
typedef struct pdf_reference{
	int major;/**<major cislo*/
	int minor;/**<minor cislo*/
	pdf_object * obj;/*<ukazatel na objekt*/
}pdf_reference;

/**objekt typu stream*/
typedef struct pdf_stream{
	int begin;/**<pocatek streamu v souboru*/
	long len;/**<delka streamu*/
	int compressed;/**<1 je-li stream komprimovany*/
	pdf_object * dict;/**<ukazatel na slovnik streamu*/
	char * stream;/**<ukazatel na pole obsahujici data streamu*/
}pdf_stream;

/**prvek objektu array*/
typedef struct pdf_array{
	struct pdf_array * next;/**<dalsi prvek*/
	struct pdf_array * prev;/**<predchozi prvek*/
	pdf_object * obj;/**<ukazatel na objekt*/
}pdf_array;
/**hlava objektu array*/
typedef struct pdf_array_head{
	pdf_array * next;/**<dalsi prvek*/
	pdf_array * prev;/**<predchozi prvek*/
}pdf_array_head;

/**prvek objektu dict*/
typedef struct pdf_dict{
	struct pdf_dict * next;/**<dalsi prvek*/
	struct pdf_dict * prev;/**<predchozi prvek*/
	char  * name; /**<ukazatel na jmeno slovniku*/
	pdf_object * obj;/**<ukazatel na objekt slovniky*/
}pdf_dict;
/**hlava objektu dict*/
typedef struct pdf_dict_head{
	pdf_dict * next;/**<dalsi prvek*/
	pdf_dict * prev;/**<predchozi prvek*/
}pdf_dict_head;
/**konstanty identifikujici typ objektu*/
typedef enum pdf_object_type { PDF_OBJ_BOOL, PDF_OBJ_INT, PDF_OBJ_REAL, PDF_OBJ_STR, PDF_OBJ_NAME, PDF_OBJ_ARRAY, 
			PDF_OBJ_DICT, PDF_OBJ_STREAM, PDF_OBJ_NULL, PDF_OBJ_INDIRECT, PDF_OBJ_INDIRECT_REF, PDF_OBJ_UNKNOWN}
			pdf_object_type;

/**struktura popisujici obsah objektu*/
union pdf_object_value{
	pdf_real_number real_number;
	pdf_int_number int_number;
	pdf_name name;
	pdf_id id;
	pdf_string str;
	pdf_reference reference;
	pdf_dict_head dict;
	pdf_array_head array;
	pdf_stream stream;
	int boolean;
};

/**struktura popisujici objekt*/
struct  pdf_object{
	pdf_object_type type;
	union pdf_object_value val;
};

int pdf_get_object(MYFILE * f, pdf_object * p_obj, pdf_object_table * xref, pdf_tok * last_tok);
int pdf_get_object_from_str(pdf_object * obj, char * str);
int pdf_write_object (pdf_object * p_obj, FILE * f);
int pdf_get_tok(MYFILE * f, pdf_tok * p_tok);
long pdf_get_obj_poz(pdf_object_table * xref, pdf_object * p_obj);
pdf_object * pdf_new_object(void);
int pdf_delete_object (pdf_object * p_obj);
int pdf_copy_object (pdf_object * new_obj, pdf_object * old_obj);
int pdf_del_dict_name_value(pdf_object * p_obj, char * name);
void pdf_filter_dict(pdf_object * p_obj, char * filter[]);
pdf_object * pdf_get_dict_name_value(pdf_object * p_obj, char *  name);
pdf_object * pdf_add_dict_name_value(pdf_object * p_obj, char *  name);
int pdf_merge_two_dict(pdf_object * d1, pdf_object *d2);
int pdf_free_tok(pdf_tok * p_tok);
int pdf_count_size_object (pdf_object * p_obj);

#endif

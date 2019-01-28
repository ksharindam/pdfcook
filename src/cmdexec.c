/**\file cmdexec.c
 * \brief Prikazovy interpret pro knihovnu.
 */


#define _GNU_SOURCE
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "cmdexec.h"
#include "common.h"
#include "pdf_lib.h"
#include "ps_lib.h"
#include "fileio.h"
#include "magic.h"

#define skip_wh_spaces(_where_){while (*(_where_) && isspace(*(_where_)))\
		++_where_;}
#define get_ids_len(a) (sizeof((a))/sizeof(struct id_str))
#define STR_MAX 255
#define PARAM_MAX 11
/*definice typu*/
/*tokens*/
enum {  CMD_TOK_UNKNOWN,
	CMD_TOK_EOF,
	CMD_TOK_ID,
	CMD_TOK_INT,
	CMD_TOK_REAL,
	CMD_TOK_STR,
	CMD_TOK_DOTDOT,
	CMD_TOK_EQ,
	CMD_TOK_COMMA,
	CMD_TOK_LPAR,
	CMD_TOK_RPAR,
	CMD_TOK_LCPAR,
	CMD_TOK_RCPAR,
	CMD_TOK_MINUS,
	CMD_TOK_MEASURE,
	CMD_TOK_DOLLAR
};

struct id_str{
    char * str;
    int id;
};

struct id_str ids_orient[]={
	{"vertical",DOC_O_LANDSCAPE},
	{"v",DOC_O_LANDSCAPE},
	{"landscape",DOC_O_LANDSCAPE},
	{"horizontal",DOC_O_PORTRAIT},
	{"h",DOC_O_PORTRAIT},
	{"portrait",DOC_O_PORTRAIT}
};

struct id_str ids_center[]={
	{"off",0},
	{"xy",1},
	{"x",3},
	{"y",2}
};
struct id_str ids_center_nup[]={
	{"off",0},
	{"on",1},
	{"final",2}
};

static int str_to_id(char * str, struct id_str ids[],size_t len){
	int i;
	for(i=0;i<len &&strcmp(str,ids[i].str);++i);
	if (i==len){
		return -1;
	}
	else{
		return ids[i].id;
	}
}

typedef struct cmd_tok_struct{
	int token;
	char str[STR_MAX];
	long number;
	double real_number;
	int row;
	int column;
}cmd_tok_struct;

typedef struct cmd_param {
	struct cmd_param * next;
	struct cmd_param * prev;
	int type;
	char name[STR_MAX];
	long number;
	double real_number;
	char str[STR_MAX];
}cmd_param;

typedef struct cmd_param_head{
	struct cmd_param * next;
	struct cmd_param * prev;
}cmd_param_head;

typedef struct cmd_page_list_head{
	struct cmd_page_list * next;
	struct cmd_page_list * prev;
}cmd_page_list_head;

typedef struct cmd_ent_struct{
	struct cmd_ent_struct * next;
	struct cmd_ent_struct * prev;
	struct cmd_param_head params;
	struct cmd_page_list_head range;
	char cmd_id[STR_MAX];
	int row;
	int column;
	int params_count;
}cmd_ent_struct;


typedef struct cmd_page_list{
	struct cmd_page_list * next;
	struct cmd_page_list * prev;
	long range[2];
	int negativ_range;
	cmd_ent_struct_head commands;
}cmd_page_list;

typedef struct param{
	char * name;
	int type;
	int value;
	long int_number;
	double real_number;
	char * str;
}param;

typedef struct def_list {
	struct def_list *next;
	struct def_list *prev;
	char * name;
	param * params;
	int params_count;
	struct cmd_arg_ent * arg_ent;
	int arg_ent_count;
} def_list;

typedef struct def_list_head {
	struct def_list * next;
	struct def_list * prev;
} def_list_head;

/**struktura pro definici prikazu*/
typedef struct cmd_entry{
	char * str; /**<identifikator prikazu*/
	char *  help;/**<napoveda k prikazu*/
	int (*proc)(page_list_head *,param params[],cmd_page_list_head *); /**<obsluzna fce prikazu*/
	param * params;/**<argumenty prikazu*/
	int params_count;/**<pocet argumentu*/
	int pages;/**<vyber stranek*/
}cmd_entry;

struct unit_val_ent {
	char * name;
	double value;
};

struct  cmd_arg_ent {
	char * part;
	param * pparam;	
};


static int cmd_get_token(MYFILE * f,cmd_tok_struct * structure);
static int cmd_get_args(MYFILE * f, cmd_ent_struct * cmd, cmd_tok_struct * p_tok);
static int cmd_make_tree(MYFILE * f, cmd_ent_struct_head * cmd_tree, cmd_tok_struct * p_tok);
static void cmd_free_tree(cmd_ent_struct_head * cmd_tree);



static int cmd_read(page_list_head * p_doc, param params[], cmd_page_list_head * pages);
static int cmd_write(page_list_head * p_doc, param params[], cmd_page_list_head * pages);
static int cmd_modulo(page_list_head * p_doc, param params[], cmd_page_list_head * pages);
static int cmd_book(page_list_head * p_doc, param params[], cmd_page_list_head * pages);
static int cmd_select(page_list_head * p_doc, param params[], cmd_page_list_head * pages);
static int cmd_apply(page_list_head * p_doc, param params[], cmd_page_list_head * pages);
static int cmd_new(page_list_head * p_doc, param params[], cmd_page_list_head * pages);
static int cmd_del(page_list_head * p_doc, param params[], cmd_page_list_head * pages);
static int cmd_scale(page_list_head * p_doc, param params[], cmd_page_list_head * pages);
static int cmd_scaleto(page_list_head * p_doc, param params[], cmd_page_list_head * pages);
static int cmd_scaleto2(page_list_head * p_doc, param params[], cmd_page_list_head * pages);
static int cmd_flip(page_list_head * p_doc, param params[], cmd_page_list_head * pages);
static int cmd_number(page_list_head * p_doc, param params[], cmd_page_list_head * pages);
static int cmd_crop(page_list_head * p_doc, param params[], cmd_page_list_head * pages);
static int cmd_crop2(page_list_head * p_doc, param params[], cmd_page_list_head * pages);
static int cmd_paper(page_list_head * p_doc, param params[], cmd_page_list_head * pages);
static int cmd_paper2(page_list_head * p_doc, param params[], cmd_page_list_head * pages);
static int cmd_orient(page_list_head * p_doc, param params[], cmd_page_list_head * pages);
static int cmd_nup(page_list_head * p_doc, param params[], cmd_page_list_head * pages);
static int cmd_merge(page_list_head * p_doc, param params[], cmd_page_list_head * pages);
static int cmd_bbox(page_list_head * p_doc, param params[], cmd_page_list_head * pages);
static int cmd_text(page_list_head * p_doc, param params[], cmd_page_list_head * pages);
static int cmd_line(page_list_head * p_doc, param params[], cmd_page_list_head * pages);
static int cmd_rotate(page_list_head * p_doc, param params[], cmd_page_list_head * pages);
static int cmd_move(page_list_head * p_doc, param params[], cmd_page_list_head * pages);
static int cmd_cmarks(page_list_head * p_doc, param params[], cmd_page_list_head * pages);
static int cmd_norm(page_list_head * p_doc, param params[], cmd_page_list_head * pages);
static int cmd_duplex(page_list_head * p_doc, param params[], cmd_page_list_head * pages);
static int cmd_matrix(page_list_head * p_doc, param params[], cmd_page_list_head * pages);
static int cmd_spaper(page_list_head * p_doc, param params[], cmd_page_list_head * pages);
static int cmd_pinfo(page_list_head * p_doc, param params[], cmd_page_list_head * pages);

static param  cmd_read_params[] = {{"name",CMD_TOK_STR,CMD_TOK_UNKNOWN,0,0,NULL}};
static param  cmd_write_params[] = {{"name",CMD_TOK_STR,CMD_TOK_UNKNOWN,0,0,NULL}};
static param  cmd_modulo_params[] = {{"pages",CMD_TOK_INT,CMD_TOK_UNKNOWN,0,0,NULL},
				     {"half",CMD_TOK_INT,CMD_TOK_INT,-1,0,NULL},
				     {"round",CMD_TOK_INT,CMD_TOK_INT,-1,0,NULL}
				};
static param  cmd_duplex_params[] = {{"long-edge",CMD_TOK_INT,CMD_TOK_INT,1,0,NULL}};
static param  cmd_scale_params[] = {{"scale",CMD_TOK_REAL,CMD_TOK_UNKNOWN,0,0,NULL}};
static param  cmd_scaleto_params[] = {{"paper",CMD_TOK_ID,CMD_TOK_UNKNOWN,0,0,NULL},
				 {"top",CMD_TOK_MEASURE,CMD_TOK_MEASURE,0,36,NULL},
				 {"right",CMD_TOK_MEASURE,CMD_TOK_MEASURE,0,36,NULL},
				 {"bottom",CMD_TOK_MEASURE,CMD_TOK_MEASURE,0,36,NULL},
				 {"left",CMD_TOK_MEASURE,CMD_TOK_MEASURE,0,36,NULL},
				};
static param  cmd_scaleto2_params[] = {{"x",CMD_TOK_MEASURE,CMD_TOK_UNKNOWN,-1,0,NULL},
				{"y",CMD_TOK_MEASURE,CMD_TOK_UNKNOWN,-1,0,NULL},
				 {"top",CMD_TOK_MEASURE,CMD_TOK_MEASURE,0,36,NULL},
				 {"right",CMD_TOK_MEASURE,CMD_TOK_MEASURE,0,36,NULL},
				 {"bottom",CMD_TOK_MEASURE,CMD_TOK_MEASURE,0,36,NULL},
				 {"left",CMD_TOK_MEASURE,CMD_TOK_MEASURE,0,36,NULL},
				};

static param  cmd_flip_params[] = {{"mode",CMD_TOK_ID,CMD_TOK_UNKNOWN,0,0,NULL}};
static param cmd_number_params[] = {{"x",CMD_TOK_MEASURE,CMD_TOK_MEASURE,0,-1,NULL},
				    {"y",CMD_TOK_MEASURE,CMD_TOK_MEASURE,0,-1,NULL},
				    {"start",CMD_TOK_INT,CMD_TOK_INT,1,0,NULL},
				    {"text",CMD_TOK_STR,CMD_TOK_STR,0,0,NULL},
				    {"font",CMD_TOK_STR,CMD_TOK_STR,0,0,NULL},
				    {"size",CMD_TOK_INT,CMD_TOK_INT,10,0,NULL}};
static param cmd_crop_params[] = {{"paper",CMD_TOK_ID,CMD_TOK_UNKNOWN,0,0,NULL}};
static param cmd_crop2_params[] = {{"lx",CMD_TOK_MEASURE,CMD_TOK_UNKNOWN,0,0,NULL},
				  {"ly",CMD_TOK_MEASURE,CMD_TOK_UNKNOWN,0,0,NULL},
				  {"hx",CMD_TOK_MEASURE,CMD_TOK_UNKNOWN,0,0,NULL},
				  {"hy",CMD_TOK_MEASURE,CMD_TOK_UNKNOWN,0,0,NULL}};
static param cmd_paper_params2[] = {{"x",CMD_TOK_MEASURE,CMD_TOK_UNKNOWN,0,0,NULL},
				   {"y",CMD_TOK_MEASURE,CMD_TOK_UNKNOWN,0,0,NULL}};
static param cmd_paper_params[]= {{"paper",CMD_TOK_ID,CMD_TOK_UNKNOWN,0,0,NULL}};
static param cmd_orient_params[] = {{"orient",CMD_TOK_ID,CMD_TOK_UNKNOWN,0,0,NULL}};
static param cmd_nup_params[] = {{"x",CMD_TOK_INT,CMD_TOK_UNKNOWN,0,0,NULL},
				 {"y",CMD_TOK_INT,CMD_TOK_INT,0,0,NULL},
				 {"dx",CMD_TOK_MEASURE,CMD_TOK_MEASURE,0,0,NULL},
				 {"dy",CMD_TOK_MEASURE,CMD_TOK_MEASURE,0,0,NULL},
				 {"orient",CMD_TOK_ID,CMD_TOK_ID,0,0,"unknown"},
				 {"rotate",CMD_TOK_INT,CMD_TOK_INT,0,0,NULL},
				 {"by_bbox",CMD_TOK_INT,CMD_TOK_INT,0,0,NULL},
				 {"paper",CMD_TOK_ID,CMD_TOK_ID,0,0,NULL},
				 {"frame",CMD_TOK_INT,CMD_TOK_INT,0,0,NULL},
				 {"center",CMD_TOK_ID,CMD_TOK_ID,0,0,NULL},
				 {"scale",CMD_TOK_INT,CMD_TOK_INT,1,0,NULL}};
static param cmd_text_params[] = {{"x",CMD_TOK_MEASURE,CMD_TOK_UNKNOWN,0,0,NULL},
				 {"y",CMD_TOK_MEASURE,CMD_TOK_UNKNOWN,0,0,NULL},
				 {"text",CMD_TOK_STR,CMD_TOK_UNKNOWN,0,0,NULL},
				 {"font",CMD_TOK_STR,CMD_TOK_STR,0,0,NULL},
				 {"size",CMD_TOK_INT,CMD_TOK_INT,10,0,NULL}};
static param cmd_line_params[]   = {{"lx",CMD_TOK_MEASURE,CMD_TOK_UNKNOWN,0,0,NULL},
				    {"ly",CMD_TOK_MEASURE,CMD_TOK_UNKNOWN,0,0,NULL},
				    {"hx",CMD_TOK_MEASURE,CMD_TOK_UNKNOWN,0,0,NULL},
				    {"hy",CMD_TOK_MEASURE,CMD_TOK_UNKNOWN,0,0,NULL},
				    {"width",CMD_TOK_MEASURE,CMD_TOK_MEASURE,0,2,NULL}
				   };
static param cmd_rotate_params[] = {{"angle",CMD_TOK_INT,CMD_TOK_UNKNOWN,0,0,NULL}};
static param cmd_move_params[]   = {{"x",CMD_TOK_MEASURE,CMD_TOK_UNKNOWN,0,0,NULL},
				    {"y",CMD_TOK_MEASURE,CMD_TOK_UNKNOWN,0,0,NULL}};

static param cmd_cmarks_params[]   = {{"by_bbox",CMD_TOK_INT,CMD_TOK_INT,1,0,NULL}};

static param cmd_norm_params[]   = {{"center",CMD_TOK_ID,CMD_TOK_ID,0,0,NULL},
				    {"scale",CMD_TOK_INT,CMD_TOK_INT,1,0,NULL},
				    {"l_bbox",CMD_TOK_INT,CMD_TOK_INT,1,0,NULL},
				    {"g_bbox",CMD_TOK_INT,CMD_TOK_INT,1,0,NULL}};
static param cmd_matrix_params[]   = {{"a",CMD_TOK_REAL,CMD_TOK_UNKNOWN,0,0,NULL},
				      {"b",CMD_TOK_REAL,CMD_TOK_UNKNOWN,0,0,NULL},
				      {"c",CMD_TOK_REAL,CMD_TOK_UNKNOWN,0,0,NULL},
				      {"d",CMD_TOK_REAL,CMD_TOK_UNKNOWN,0,0,NULL},
				      {"e",CMD_TOK_REAL,CMD_TOK_UNKNOWN,0,0,NULL},
				      {"f",CMD_TOK_REAL,CMD_TOK_UNKNOWN,0,0,NULL}};

static param  cmd_spaper_params[] = {{"name",CMD_TOK_ID,CMD_TOK_UNKNOWN,0,0,NULL},
				     {"x",CMD_TOK_MEASURE, CMD_TOK_UNKNOWN,0,0,NULL},
				     {"y",CMD_TOK_MEASURE, CMD_TOK_UNKNOWN,0,0,NULL}};

#define fill_params(p) p,sizeof(p)/sizeof(param)
/**definice prikazu*/
static cmd_entry cmd_commands[]={
	{"apply","",cmd_apply,NULL,0,1},
	{"bbox","recalculate bbox on each page by GS",cmd_bbox,NULL,0,0},
	{"book", "",cmd_book,NULL,0,0},
	{"cmarks","add printing marks",cmd_cmarks,fill_params(cmd_cmarks_params),0},
	{"crop","",cmd_crop,fill_params(cmd_crop_params),0},
	{"crop2","",cmd_crop2,fill_params(cmd_crop2_params),0},
	{"del","delete list",cmd_del,NULL,0,0},
	{"duplex","",cmd_duplex,fill_params(cmd_duplex_params),0},
	{"flip","horizontal | vertical",cmd_flip,fill_params(cmd_flip_params),0},
	{"line","draw line to page",cmd_line,fill_params(cmd_line_params),0},
	{"matrix","transform by matrix",cmd_matrix,fill_params(cmd_matrix_params),0},
	{"merge","merge list to one page",cmd_merge,NULL,0},
	{"modulo", "",cmd_modulo,fill_params(cmd_modulo_params),1},
	{"move","",cmd_move,fill_params(cmd_move_params),0},
	{"new","add new page at the end of list",cmd_new,NULL,0,1},
	{"norm","normalize  pages",cmd_norm,fill_params(cmd_norm_params),0},
	{"number","add numbers to pages",cmd_number,fill_params(cmd_number_params),0},
	{"nup","",cmd_nup,fill_params(cmd_nup_params),0},
	{"orient","set text orientation the page landscape|portrait",cmd_orient,fill_params(cmd_orient_params),0},
	{"paper","",cmd_paper,fill_params(cmd_paper_params),0},
	{"paper2","",cmd_paper2,fill_params(cmd_paper_params2),0},
	{"read","append file at the end of list",cmd_read,fill_params(cmd_read_params),0},
	{"rotate","rotate page",cmd_rotate,fill_params(cmd_rotate_params),0},
	{"scale","scale pages in the list",cmd_scale,fill_params(cmd_scale_params),0},
	{"scaleto","",cmd_scaleto,fill_params(cmd_scaleto_params),0},
	{"scaleto2","",cmd_scaleto2,fill_params(cmd_scaleto2_params),0},
	{"select","",cmd_select,NULL,0,1},
	{"text","write text to page",cmd_text,fill_params(cmd_text_params),0},
	{"write","save list to file",cmd_write,fill_params(cmd_write_params),0},
	{"spaper","define new paper format",cmd_spaper,fill_params(cmd_spaper_params),0},
	{"info","print information about each pages",cmd_pinfo,NULL,0,0},
	{NULL,NULL,0}
};

static struct unit_val_ent table[] = {
	{"cm",28.346456692913385211},
	{"mm",2.8346456692913385211},
	{"in",1*72.0},
	{"pt",1},
	{NULL,0}
};

static struct def_list_head dlist = {(def_list *)&dlist,(def_list *)&dlist};

static void cmd_add_def(char * name,  param * params, int params_count, struct cmd_arg_ent * arg_ent, int arg_ent_count) {
	def_list * tmp = (def_list*) malloc(sizeof(def_list));
	if (tmp == NULL) {
		vdoc_errno=VDOC_ERR_LIBC;	
		message(FATAL,"malloc() error");
		assert(0);
		return;
	}
	assert(tmp!=NULL);
	tmp->name = name;
	tmp->params = params;
	tmp->params_count = params_count;
	tmp->arg_ent = arg_ent;
	tmp->arg_ent_count = arg_ent_count;
	listinsert(tmp, dlist.next);
	
}


static def_list * cmd_get_def(char *name) {
	def_list * tmp = dlist.next;
	for (tmp = dlist.next; tmp!=((def_list *)&dlist); tmp=tmp->next) {
		if (strcmp(name, tmp->name) == 0) {
			return tmp;
		}
	} 
	return NULL;
} 

static cmd_param* cmd_expand_param(cmd_ent_struct * cmd, int param_pos, char * name) {
	cmd_param * argument;
	int i;
	for(argument =cmd->params.next, i=1;i< param_pos && argument!=(cmd_param *)(&cmd->params);
	    argument=argument->next, ++i) {
		if (argument->name && name && (strcmp(argument->name, name) == 0)) {
			return argument;
		}
	}
	if (argument!=(cmd_param *)(&cmd->params)) {
		return argument;
	} else {
		return NULL;
	}
}

static void cmd_print_arg(cmd_param * argument, char * buffer, size_t buffer_len) {
	
	switch (argument->type) {
		case CMD_TOK_INT:
			snprintf(buffer, buffer_len, "%ld", argument->number);
			break;
		case CMD_TOK_REAL:
			snprintf(buffer, buffer_len, "%f", argument->real_number);
			break;
		case CMD_TOK_ID:
			snprintf(buffer, buffer_len, "%s", argument->str);
			break;
		case CMD_TOK_STR:
			snprintf(buffer, buffer_len, "\"%s\"", argument->str);
			break;
		case CMD_TOK_MEASURE:
			snprintf(buffer, buffer_len, "%f", argument->real_number);
			break;
		default:
			assert(0);
			break;

	}
		
}

static void cmd_print_arg_exp (param * argument, char * buffer, size_t buffer_len) {
	
	switch (argument->type) {
		case CMD_TOK_INT:
			snprintf(buffer, buffer_len, "%ld", argument->int_number);
			break;
		case CMD_TOK_REAL:
			snprintf(buffer, buffer_len, "%f", argument->real_number);
			break;
		case CMD_TOK_ID:
			snprintf(buffer, buffer_len, "%s", argument->str);
			break;
		case CMD_TOK_STR:
			snprintf(buffer, buffer_len, "\"%s\"", argument->str);
			break;
		case CMD_TOK_MEASURE:
			snprintf(buffer, buffer_len, "%f", argument->real_number);
			break;
		default:
			assert(0);
			break;

	}
		
}



#define BUF_LEN 256
static int cmd_expand_macro( cmd_ent_struct_head * cmd_tree, def_list * def, cmd_ent_struct * cmd ) {
	int arg_count = def->arg_ent_count;
	struct cmd_arg_ent * arg_ent =  def->arg_ent;
	cmd_param * argument;
	int param_pos;
	char buf[BUF_LEN];
	char * c_buf = NULL;
	char * tmp_buf;
	MYFILE * f;
	cmd_tok_struct tok;
	int retv;

	int i;

//del me
	printf("expanding makro ...\n");
	for (i=0;i<arg_count;++i) {
		if (arg_ent[i].pparam) {
			param_pos = arg_ent[i].pparam - def->params + 1;
			argument = cmd_expand_param(cmd, param_pos, arg_ent[i].pparam->name);
			if (argument == NULL) {
				if (arg_ent[i].pparam->type == CMD_TOK_UNKNOWN) {
					message(WARN,">Command \"def\" has feuew params at line %d column %d.\n",cmd->row,cmd->column);
					return -1;
				}
				cmd_print_arg_exp(arg_ent[i].pparam,buf, BUF_LEN);
			} else {
				cmd_print_arg(argument,buf, BUF_LEN);
			}
			if (c_buf) {
				tmp_buf = c_buf;
				asprintf(&c_buf, "%s%s",tmp_buf, buf);
				free(tmp_buf);
			} else {
				asprintf(&c_buf, "%s", buf);
				
			}
		}

		if (arg_ent[i].part) {
			if (c_buf) {
				tmp_buf = c_buf;
				asprintf(&c_buf, "%s%s",tmp_buf, arg_ent[i].part);
				free(tmp_buf);
			} else {
				asprintf(&c_buf, "%s", arg_ent[i].part);
				
			}
		}
	}	
//del me
	printf("%s\n", c_buf);
	f = stropen(c_buf);
	cmd_get_token(f, &tok);
	retv = cmd_make_tree(f, cmd_tree, &tok);
	myfclose(f);
	free(c_buf);
	return retv;
}

static double get_unit_val(char * unit){
	int i;
	for (i=0;table[i].name;++i){
		if (strcmp(unit,table[i].name)==0){
			return table[i].value;
		}
	}
	return 0;
}

static cmd_page_list * cmd_add_range(cmd_ent_struct * cmd, long begin, long end, int negativ_range){
	cmd_page_list * new_range = (cmd_page_list*) malloc(sizeof(cmd_page_list));

	if (new_range == NULL) {
		vdoc_errno=VDOC_ERR_LIBC;	
		message(FATAL,"malloc() error");
		assert(0);
		return NULL;
	}

	if (cmd == NULL){
		assert(0);
		return NULL;
	}
	new_range->range[0]=begin;
	new_range->range[1]=end;
	new_range->negativ_range=negativ_range;
	new_range->commands.next=new_range->commands.prev=(cmd_ent_struct *)&(new_range->commands);
	new_range->next=(cmd_page_list *)&(cmd->range);
	new_range->prev=cmd->range.prev;
	new_range->next->prev=new_range;
	new_range->prev->next=new_range;
	return new_range;
}
static int cmd_sort_range(cmd_page_list_head * sorted,cmd_page_list_head * unsorted, int pages_count){
	cmd_page_list * it1, *it2,*pom;
	int tmp;
	sorted->next=sorted->prev=(cmd_page_list *)sorted;
	for(it1=unsorted->next;it1!=(cmd_page_list*)unsorted;it1=it1->next){
		pom=(cmd_page_list *)malloc(sizeof(cmd_page_list));

		if (pom == NULL) {
			vdoc_errno=VDOC_ERR_LIBC;	
			message(FATAL,"malloc() error");
			assert(0);
			return -1;
		}
		
		memcpy(pom,it1,sizeof(cmd_page_list));

		if (pom->range[0]==-1){
			pom->range[0]=pages_count;
		}

		if (pom->range[1]==-1){
			pom->range[1]=pages_count;
		}

		if (pom->range[1]<pom->range[0]){
			tmp = pom->range[1];
			pom->range[1] = pom->range[0];
			pom->range[0] = tmp;
		}

		if (pom->negativ_range){
			pom->range[0]=pages_count-pom->range[0] + 1;
			pom->range[1]=pages_count-pom->range[1] + 1;
			tmp=pom->range[0];
			pom->range[0]=pom->range[1];
			pom->range[1]=tmp;
		}
		/*insert sort*/
		for(it2=sorted->next;it2!=(cmd_page_list *)sorted && it2->range[0]>pom->range[0];it2=it2->next);
		pom->next=it2;
		pom->prev=it2->prev;
		pom->next->prev=pom;
		pom->prev->next=pom;
	}
	return 0;
}

static int cmd_get_pages_args_cmd_new(MYFILE * f, cmd_ent_struct * cmd, cmd_tok_struct * p_tok){
	cmd_page_list * page_range;
	if (p_tok->token!=CMD_TOK_LCPAR){
		return 0;
	}
	cmd_get_token(f,p_tok);
	switch (p_tok->token){
		case CMD_TOK_ID:
			page_range=cmd_add_range(cmd,1,1,0);
			cmd_make_tree(f,&(page_range->commands),p_tok);
			if (p_tok->token==CMD_TOK_RCPAR){
				cmd_get_token(f,p_tok);
				return 0;
			}
			else{
				return -1;
			}
			break;
		default:
			return -1;
	}
	return 0;
}
static int cmd_get_pages_args(MYFILE * f, cmd_ent_struct * cmd, cmd_tok_struct * p_tok){
	cmd_page_list * page_range;
	int negativ_range=0;
	long r_begin, r_end;
	/*this part is not mandatory*/
	if (p_tok->token!=CMD_TOK_LCPAR){
		return 0;
	}
	cmd_get_token(f,p_tok);
	while (1){
		switch (p_tok->token){
			case CMD_TOK_RCPAR:
				cmd_get_token(f,p_tok);
				return 0;
			case CMD_TOK_MINUS:
				negativ_range=1;
				cmd_get_token(f,p_tok);
				if (p_tok->token!=CMD_TOK_INT){
					return -1;
				}
			case CMD_TOK_INT:
			case CMD_TOK_DOLLAR:
				r_begin=r_end=p_tok->number;
				cmd_get_token(f,p_tok);
				if (p_tok->token==CMD_TOK_DOTDOT){
					cmd_get_token(f,p_tok);
					if (p_tok->token==CMD_TOK_INT || p_tok->token==CMD_TOK_DOLLAR){
						r_end=p_tok->number;
					}
					else{
						return -1;
					}
					cmd_get_token(f,p_tok);
				}
				page_range=cmd_add_range(cmd,r_begin,r_end,negativ_range);
				negativ_range=0;
				/*prikazy pro dannou skupinu stranek*/
				if (p_tok->token==CMD_TOK_ID){
					/*if (*/cmd_make_tree(f,&(page_range->commands),p_tok);/*==-1){
					return -1;
					}*/
				}
				break;
			default:
				return -1;
			
		}
	}
	return -1;
}
static cmd_param * cmd_add_param(cmd_ent_struct * cmd, char * p_name){
	cmd_param * new_param = (cmd_param *)  malloc(sizeof(cmd_param));

	if (new_param == NULL) {
		vdoc_errno=VDOC_ERR_LIBC;	
		message(FATAL,"malloc() error");
		assert(0);
		return NULL;
	}

	if (cmd == NULL){
		assert(0);
		return NULL;
	}
	if (p_name==NULL){
		p_name="";
	}
	strncpy(new_param->name,p_name,STR_MAX);
	new_param->next=(cmd_param *)(&cmd->params);
	new_param->prev=cmd->params.prev;
	new_param->next->prev=new_param;
	new_param->prev->next=new_param;
	++cmd->params_count;
	return new_param;
}

static void cmd_set_param_val(cmd_param * param, cmd_tok_struct * p_tok){
	param->type=p_tok->token;
	switch(p_tok->token){
		case CMD_TOK_STR:
		case CMD_TOK_ID:
			strncpy(param->str,p_tok->str,STR_MAX);
			return;
		case CMD_TOK_INT:
			param->number=p_tok->number;
			return;
		case CMD_TOK_MEASURE:
		case CMD_TOK_REAL:
			param->real_number=p_tok->real_number;
			return;
		default:
			param->type=CMD_TOK_UNKNOWN;
			return;
	}
}
/*nacteni parametru prikazu*/
static int cmd_get_args(MYFILE * f, cmd_ent_struct * cmd, cmd_tok_struct * p_tok){
	int comma=1;
	int eq=0;
	cmd_tok_struct pom_tok;
	cmd_param * param;
	if (p_tok->token!=CMD_TOK_LPAR){
		return 0;
	}
	param=cmd_add_param(cmd,"");
	cmd_get_token(f,p_tok);
	while (1){
		switch (p_tok->token){
			case CMD_TOK_RPAR:
				if (comma==0 && eq==0){
					cmd_get_token(f,p_tok);
					return 0;
				}
				else{
					return -1;
				}
			case CMD_TOK_COMMA:
				if (comma==0 && eq==0){
					param=cmd_add_param(cmd,"");
					comma=1;
					break;
				}
				else{
					return -1;
				}
			case CMD_TOK_MINUS:
					if(comma==0){
						return -1;
					}
					comma=0; eq=0;
					cmd_get_token(f,p_tok);
					switch (p_tok->token){
						case CMD_TOK_INT:
							p_tok->number*=-1;
							break;
						case CMD_TOK_REAL:
							p_tok->real_number*=-1;
							break;
						default:
							return -1;
					}
					cmd_set_param_val(param,p_tok);
				break;
			case CMD_TOK_ID:
				if (comma==0){
					double unit_val;
					if ((unit_val=get_unit_val(p_tok->str))==0){
						return -1;
					}
					switch (param->type){
						case CMD_TOK_INT:
							param->real_number=param->number;
						case CMD_TOK_REAL:
							param->real_number*=unit_val;
							param->type=CMD_TOK_MEASURE;
							break;
						default:
							return -1;
					}
					break;
				}
				if (eq==1){
					if (comma==0){
						return -1;
					}
					comma=0; eq=0;
					cmd_set_param_val(param,p_tok);
					break;
				}
				cmd_get_token(f,&pom_tok);
				switch(pom_tok.token){
					case CMD_TOK_EQ:
						if (eq==1){
							return -1;
						}
						eq=1;
						strncpy(param->name,p_tok->str,STR_MAX);
						break;
					case CMD_TOK_COMMA:
						/*???*/
						cmd_set_param_val(param,p_tok);
						if (comma==1 && eq==0){
							param=cmd_add_param(cmd,"");
							comma=1;
						}
						else{
							return -1;
						}
						break;
					case CMD_TOK_RPAR:
						/*???*/
						cmd_set_param_val(param,p_tok);
						comma=0; eq=0;
					default:
						memcpy(p_tok,&pom_tok,sizeof(cmd_tok_struct));
						continue;
				}
				break;
			case CMD_TOK_INT:
			case CMD_TOK_REAL:
			case CMD_TOK_STR:
				if (comma==0){
					return -1;
				}
				comma=0; eq=0;
				cmd_set_param_val(param,p_tok);
				break;
			case CMD_TOK_EOF:
				p_tok->token=CMD_TOK_UNKNOWN;
				return -1;
				break;
			default:
				return -1;
		}
		cmd_get_token(f,p_tok);
	}
	return -1;
}


static cmd_ent_struct * cmd_new_command(cmd_ent_struct_head * cmd_tree, char * cmd_id, int row, int column){
	cmd_ent_struct * new_cmd = (cmd_ent_struct *) malloc(sizeof(cmd_ent_struct));
	if (new_cmd == NULL ||  cmd_id == NULL ){
		return NULL;
	}
	strncpy(new_cmd->cmd_id,cmd_id,STR_MAX);
	if (cmd_tree != NULL) {
		new_cmd->next=(cmd_ent_struct *)cmd_tree;
		new_cmd->prev=cmd_tree->prev;
	} else {
		new_cmd->next = new_cmd;
		new_cmd->prev = new_cmd;
	}
	new_cmd->next->prev=new_cmd;
	new_cmd->prev->next=new_cmd;
	new_cmd->params.next=new_cmd->params.prev=(cmd_param *)(&(new_cmd->params));
	new_cmd->range.next=new_cmd->range.prev=(cmd_page_list *)(&(new_cmd->range));
	new_cmd->row = row;
	new_cmd->column = column;
	new_cmd->params_count = 0;
	return new_cmd;
}

static int cmd_make_tree(MYFILE * f, cmd_ent_struct_head * cmd_tree, cmd_tok_struct * p_tok){
	cmd_ent_struct * cmd;
	def_list * tmp;
	cmd_ent_struct_head tmp_tree;
	cmd_tree->next=cmd_tree->prev=(cmd_ent_struct *)cmd_tree;
	tmp_tree.next=tmp_tree.prev=(cmd_ent_struct *)&tmp_tree;
	while((p_tok->token)==CMD_TOK_ID){
		tmp = cmd_get_def(p_tok->str);	
		if (strcmp("def", p_tok->str) && tmp == NULL) {
			cmd=cmd_new_command(cmd_tree,p_tok->str, p_tok->row, p_tok->column);
		} else {
			cmd=cmd_new_command(&tmp_tree,p_tok->str, p_tok->row, p_tok->column);
		}
		if ((cmd_get_token(f,p_tok)==-1) || (cmd_get_args(f,cmd,p_tok)==-1)){
			if (tmp) {
				cmd_expand_macro(cmd_tree, tmp, cmd);
			}
			break;
		}

		if (strcmp("def", cmd->cmd_id)==0) {
			char * def_name = NULL;
			char * def_body = NULL;
			char * arg_name;
			char * last_cmd;
			param * params = NULL;
			cmd_param * argument;
			struct cmd_arg_ent * arg_ent;
			int i;
			int j;
			int k;
			int params_count = cmd->params_count - 2;
			int arg_count = 1;
			int tmp;
			assert(cmd->params_count>=0);
			
			if (cmd->params_count) {
				params = malloc(sizeof(param) * params_count);

				if (params == NULL) {
					vdoc_errno=VDOC_ERR_LIBC;	
					message(FATAL,"malloc() error");
					assert(0);
					return -1;
				}
			}

			for(i=0,argument=cmd->params.next;
			    argument!=(cmd_param *)(&cmd->params) &&  argument->name[0]==0;
			    argument=argument->next,++i) {
				switch (i) {
					case 0:
						if (argument->type==CMD_TOK_STR || argument->type==CMD_TOK_ID) {
							asprintf(&def_name, "%s", argument->str);
						} else {
							message(WARN,"Param \"name\" in command \"def\" must be ID or str  at line %d column %d.\n",cmd->row,cmd->column);
							return -1;
						}
						break;
					case 1:
						if (argument->type==CMD_TOK_STR || argument->type==CMD_TOK_ID) {
							def_body = argument->str;
						} else {
							message(WARN,"Param \"body\" in command \"def\" must be ID or str  at line %d column %d.\n",cmd->row,cmd->column);
							return -1;
						}
						break;
					case 2:
					default:
						params[i-2].type =  CMD_TOK_UNKNOWN;
						switch (argument->type) {
							case CMD_TOK_STR:
							case CMD_TOK_ID:
								asprintf(&(params[i-2].name), "%s", argument->str);
								break;
							default:
								message(WARN,"Command \"def\" has wrong param type at line %d column %d.\n",cmd->row,cmd->column);
								return -1;

						}
						break;
				}
			}

			for(;argument!=(cmd_param *)(&cmd->params) &&  argument->name[0]!=0;
			    argument=argument->next, ++i)
			{
				params[i-2].type =  argument->type;
				asprintf(&(params[i-2].name), "%s", argument->name);
				switch (argument->type) {
					case CMD_TOK_INT:
						params[i-2].int_number = argument->number;
						break;
					case CMD_TOK_REAL:
						params[i-2].real_number = argument->real_number;
						break;
					case CMD_TOK_STR:
					case CMD_TOK_ID:
						asprintf(&(params[i-2].str), "%s", argument->str);
						break;
					default:
						assert(0);
						return -1;

				}

			}
			if (argument!=(cmd_param *)(&cmd->params)) {
				message(WARN,"Command has unnamed args after named args at line %d column %d\n",cmd->row, cmd->column);
				return -1;
			}

			if (i<2) {
				message(WARN,"Command \"def\" has feuew params at line %d column %d.\n",cmd->row,cmd->column);
				return -1;
			}
			for (i=0;def_body[i] != 0;++i) {
				if (def_body[i]=='$') {
					++arg_count;
				}
				if (def_body[i] == '\\' && def_body[i+1] != 0){
					++i;
				}
			}
			arg_ent = (struct cmd_arg_ent *)  malloc(sizeof(struct cmd_arg_ent) * arg_count);

			if (arg_ent == NULL) {
				vdoc_errno=VDOC_ERR_LIBC;	
				message(FATAL,"malloc() error");
				assert(0);
				return -1;
			}
			last_cmd = def_body;
			j = 0;
			arg_ent[j].part = NULL;
			arg_ent[j].pparam = NULL;
			for (i=0;def_body[i] != 0;++i) {
				if (def_body[i]=='$') {
					tmp = def_body[i];
					def_body[i] = 0;
					asprintf(&arg_ent[j].part,"%s", last_cmd);
					def_body[i] = tmp;
					++j;
					++i;
					arg_name = def_body + i;
					while (isalnum(def_body[i])) {
						++i;
					}
					tmp = def_body[i];
					def_body[i] = 0;
					arg_ent[j].pparam = NULL;
					
					for (k=0;k<params_count;++k) {
						if (params[k].name &&  strcmp(arg_name, params[k].name) == 0) {
							arg_ent[j].pparam = params + k;
							break;
						}
					}					

					if (arg_ent[j].pparam == NULL) {
						message(WARN,"In command \"def\" param \"%s\"  isn't declared at line %d column %d.\n", arg_name, cmd->row,cmd->column);
						return -1;
					}

					def_body[i] = tmp;
					last_cmd = def_body + i;	
					--i;
				}
				if (def_body[i] == '\\' && def_body[i+1] != 0){
					++i;
				}
			}

			if (strlen(last_cmd)) {
				asprintf(&arg_ent[j].part,"%s", last_cmd);
				//arg_ent[j].pparam = NULL;
			}
#if 0			
			printf("user defined command \"%s\"\n", def_name);
			printf("body %s\n", def_body);
			printf("params count: %d\n", params_count);
			for (i=0;i<arg_count;++i) {
				if (arg_ent[i].part)
					printf(">>%s\n",arg_ent[i].part); 
				if (arg_ent[i].pparam) {
					printf(" <$$> ");
				}
			}
			
			printf("def isn't implemented yet ...\n");
#endif
			cmd_add_def(def_name, params, params_count, arg_ent, arg_count);
			continue;
		} else if (tmp) {
			cmd_expand_macro(cmd_tree, tmp, cmd);
		}

		if (strcmp("new",cmd->cmd_id)==0){
			if (cmd_get_pages_args_cmd_new(f,cmd,p_tok)==-1){
				break;
			}
		}
		else{
			if (cmd_get_pages_args(f,cmd,p_tok)==-1){
				break;
			}
		}
	}
	cmd_free_tree(&tmp_tree);
	if (p_tok->token==CMD_TOK_EOF){
		return 0;
	}
	else{
		vdoc_errno=VDOC_ERR_IS_NOT_ID_TOKEN;
		if (p_tok->token != CMD_TOK_RCPAR && p_tok->token != CMD_TOK_INT) {
			message(WARN,"Syntax error at line %d column %d.\n",p_tok->row,p_tok->column);
		}
		return -1;
	}
}

static int cmd_exec_command_(page_list_head * p_doc,cmd_ent_struct * cmd,int index, int test){
	int i;
	int params_count=cmd_commands[index].params_count;
	cmd_param * argument;
	param params[PARAM_MAX];
	assert(PARAM_MAX>=cmd_commands[index].params_count);

	if (cmd_commands[index].pages==0 && cmd->range.next!=(cmd_page_list*)&cmd->range){
		/*command contain section pages, but it should not have it*/
		message(FATAL,"Command %s should not contain pages section, at line %d column %d.\n",cmd_commands[index].str, cmd->row, cmd->column);
		return -1;
	}
	memcpy(params,cmd_commands[index].params,sizeof(param)*params_count);
	/*unnamed arguments*/
	for(i=0,argument=cmd->params.next;
	    argument!=(cmd_param *)(&cmd->params) && i<params_count && argument->name[0]==0;
	    argument=argument->next,++i)
	{
		switch(params[i].type){
			case CMD_TOK_INT:
				if(argument->type==CMD_TOK_INT){
					params[i].value=CMD_TOK_INT;
					params[i].int_number=argument->number;
				}
				else{
					/*bad argument type*/
					message(FATAL,"Param %d of command %s is incompatible type, at line %d column %d.\n",i+1,cmd_commands[index].str, cmd->row, cmd->column);
					return -1;
				}
				break;
			case CMD_TOK_REAL:
				switch(argument->type){
					case CMD_TOK_INT:
						params[i].value=CMD_TOK_REAL;
						params[i].real_number=argument->number;
						break;
					case CMD_TOK_REAL:
						params[i].value=CMD_TOK_REAL;
						params[i].real_number=argument->real_number;
						break;
					default:
						/*bad argument type*/
						message(FATAL,"Param %d of command %s is incompatible type,at line %d column %d.\n",i+1,cmd_commands[index].str, cmd->row, cmd->column);
						return -1;
				}
				break;
			case CMD_TOK_MEASURE:
				switch(argument->type){
				case CMD_TOK_INT:
						params[i].value=CMD_TOK_REAL;
						params[i].real_number=argument->number;
					break;
				case CMD_TOK_REAL:
				case CMD_TOK_MEASURE:
						params[i].value=CMD_TOK_REAL;
						params[i].real_number=argument->real_number;
					break;
				default:
						/*bad argument type*/
						message(FATAL,"Param %d of command %s is incompatible type, at line %d column %d.\n",i+1,cmd_commands[index].str, cmd->row, cmd->column);
						return -1;
				}
				break;
			case CMD_TOK_ID:
				if(argument->type==CMD_TOK_ID){
					params[i].value=CMD_TOK_ID;
					params[i].str=argument->str;
				}
				else{
					/*bad argument type*/
					message(FATAL,"Param %d of command %s is incompatible type, %d:%d.\n",i+1,cmd_commands[index].str, cmd->row, cmd->column);
					return -1;
				}
				break;
			case CMD_TOK_STR:
				if(argument->type==CMD_TOK_STR){
					params[i].value=CMD_TOK_STR;
					params[i].str=argument->str;
				}
				else{
					/*bad argument type*/
					message(FATAL,"Param %d of command %s is incompatible type, %d:%d.\n",i+1,cmd_commands[index].str, cmd->row, cmd->column);
					return -1;
				}
				break;
			default:
				assert(0);
		}
	}
		
	/*pojmenovane argumenty*/
	for(;argument!=(cmd_param *)(&cmd->params) &&  argument->name[0]!=0;
	    argument=argument->next)
	{
		for(i=0;i<params_count && strcmp(params[i].name,argument->name)!=0;++i);
		if (i==params_count){
			/*neplatne jmeno argumentu*/
			message(FATAL,"Param \"%s\" is not from command \"%s\", at line %d column %d.\n",argument->name,cmd_commands[index].str, cmd->row, cmd->column);
			return -1;
		}
		switch(params[i].type){
			case CMD_TOK_INT:
				if(argument->type==CMD_TOK_INT){
					params[i].value=CMD_TOK_INT;
					params[i].int_number=argument->number;
				}
				else{
					/*bad argument type*/
					return -1;
				}
				break;
			case CMD_TOK_REAL:
				switch(argument->type){
					case CMD_TOK_INT:
						params[i].value=CMD_TOK_REAL;
						params[i].real_number=argument->number;
						break;
					case CMD_TOK_REAL:
						params[i].value=CMD_TOK_REAL;
						params[i].real_number=argument->real_number;
						break;
					default:
					/*bad argument type*/
						return -1;
				}
				break;
			case CMD_TOK_MEASURE:
				switch(argument->type){
				case CMD_TOK_INT:
						params[i].value=CMD_TOK_REAL;
						params[i].real_number=argument->number;
					break;
				case CMD_TOK_REAL:
				case CMD_TOK_MEASURE:
						params[i].value=CMD_TOK_REAL;
						params[i].real_number=argument->real_number;
					break;
				default:
					/*bad argument type*/
						return -1;
				}
				break;
			case CMD_TOK_ID:
				if(argument->type==CMD_TOK_ID){
					params[i].value=CMD_TOK_ID;
					params[i].str=argument->str;
				}
				else{
					/*bad argument type*/
					return -1;
				}
				break;
			case CMD_TOK_STR:
				if(argument->type==CMD_TOK_STR){
					params[i].value=CMD_TOK_STR;
					params[i].str=argument->str;
				}
				else{
					/*bad argument type*/
					return -1;
				}
				break;
			default:
				assert(0);
		}
	}

	if (argument!=(cmd_param *)(&cmd->params)){
		/*bad arguments*/
		return -1;
	}
	for(i=0;i<params_count;++i){
		if (params[i].value==CMD_TOK_UNKNOWN){
			/*value should be set*/
			return -1;
		}
	}
	if (test) {
		return 0;
	} else {
		return cmd_commands[index].proc(p_doc,params,&cmd->range);
	}
}

static int cmd_exec_command(page_list_head * p_doc, cmd_ent_struct * cmd, int test){
	int index;
	int fail = 0;
	for(index=0;cmd_commands[index].str;++index){
		if(strcmp(cmd_commands[index].str,cmd->cmd_id)==0){
			if(cmd_exec_command_(p_doc,cmd,index, test)==0){
				return 0;
			}
			fail = 1;
		}
	}
	if (fail){
		message(FATAL,"There were some errors during executing command \"%s\". Check command args, at line %d column %d.\n",cmd->cmd_id, cmd->row, cmd->column);
	}
	else{
		message(FATAL,"Unknown command \"%s\" at line %d column %d.\n",cmd->cmd_id , cmd->row, cmd->column);
	}
	return -1;
}

static int cmd_exec_tree(page_list_head * p_doc,cmd_ent_struct_head * cmd_tree, int test){
	cmd_ent_struct  * pom=cmd_tree->next;
	cmd_ent_struct * end=pom->prev;
	while (pom!=end){
		if (cmd_exec_command(p_doc,pom,test)==-1){
			return -1;
		}
		pom=pom->next;
	}
	return 0;	
}
static void cmd_free_range(cmd_page_list_head *range){
	cmd_page_list * pom = range->next, * pom2;
	while (pom!=(cmd_page_list*) range){
#ifdef _CMD_DEBUG_
		if (pom->negativ_range){
			printf(" - ");
		}
		printf("%ld..%ld\n",pom->range[0], pom->range[1]);
#endif
		if (pom->commands.next->prev==(cmd_ent_struct *)&(pom->commands)){
			cmd_free_tree(&(pom->commands));
		}
		pom2=pom;
		pom=pom->next;
		free(pom2);
	}
}
static void cmd_free_args(cmd_param_head * params){
	cmd_param * pom = params->next, * pom2;
	while (pom!=(cmd_param *) params){
#ifdef _CMD_DEBUG_
		printf("%s:",pom->name);
		switch(pom->type){
			case CMD_TOK_INT:
				printf("i%ld\n",pom->number);
				break;

			case CMD_TOK_MEASURE:
			case CMD_TOK_REAL:
				printf("f%f\n",pom->real_number);
				break;
			
			case CMD_TOK_ID:
			case CMD_TOK_STR:
				printf("%s\n",pom->str);
				break;
			default:
				printf("Unknown param\n");
						
		}
#endif
		pom2=pom;
		pom=pom->next;
		free(pom2);
	}
	
}
static void cmd_free_tree(cmd_ent_struct_head * cmd_tree){
	cmd_ent_struct  * pom=cmd_tree->next, * pom2;
	while (pom!=(cmd_ent_struct*)cmd_tree){
#ifdef _CMD_DEBUG_
		printf("%s\n",pom->cmd_id);
#endif
		cmd_free_args(&(pom->params));
		cmd_free_range(&(pom->range));
		pom2=pom;
		pom=pom->next;
		free(pom2);
	}
	
	return;
}

int cmd_preexec(cmd_ent_struct_head * cmd_tree, MYFILE * f){
	cmd_tok_struct token;
	cmd_get_token(f,&token);
	if (cmd_make_tree(f,cmd_tree,&token)==-1 || cmd_exec_tree(NULL,cmd_tree, 1)==-1){
		cmd_free_tree(cmd_tree);
		message(FATAL,"There were some errors during parsing commands\n");
		return -1;
	}
	return 0;
}

int cmd_exec(page_list_head * p_doc, cmd_ent_struct_head * cmd_tree ,MYFILE * f){
	if (cmd_exec_tree(p_doc,cmd_tree, 0)==-1){
		cmd_free_tree(cmd_tree);
		message(FATAL,"There were some errors during executing commands\n");
		return -1;
	}
	cmd_free_tree(cmd_tree);
	return 0;
	
}

#define update_poz\
		switch(c){\
			case '\n':\
				++(*row);\
				*lastc=*column;\
				(*column)=0;\
				break;\
			default:\
				(*lastc)=(*column);\
				++*(column);\
		}

#define un_update_poz\
		if (*column){\
			--(*column);\
		}else{\
			(*column)=(*lastc);\
			--(*row);\
		}

#define set_poz(structure)\
	{\
	structure->row=*row;\
	structure->column=*column;\
	}

//TODO: for debugung purpose, remove it!!
#if 0
int _mygetc(MYFILE * f, int i, int j)
{
	int c;
	c = mygetc(f);
	printf("%d %d  \"%c\"*\n",i, j, c);
	return c;
}

#undef mygetc

#define mygetc(a) \
		 _mygetc(a, __LINE__, column)

void  _myungetc(MYFILE * f, int i, int j)
{
	printf("%d %d -*\n",i, j);
	myungetc(f);

}

#undef myungetc

#define myungetc(a) \
		 _myungetc(a, __LINE__, column)
#endif


/**tokenizer for cmdexec parser*/
static int cmd_get_token(MYFILE * f,cmd_tok_struct * structure){
	int c,i;
	int * column = &f->column; 
	int * row = &f->row;
	int * lastc = &f->lastc;
	int * next_token=&f->scratch;
	if (*next_token!=CMD_TOK_UNKNOWN){
		structure->token=*next_token;
		set_poz(structure);
		*next_token=CMD_TOK_UNKNOWN;
		return 0;
	}
	while ((c=mygetc(f))!=EOF){
		update_poz;
		if(!isspace(c)){
			break;
		}
	}

	set_poz(structure);

	switch(c){
		case '{':
			structure->token=CMD_TOK_LCPAR;
			return 0;
		case  '}':
			structure->token=CMD_TOK_RCPAR;
			return 0;
		case '(': 
			structure->token=CMD_TOK_LPAR;
			return 0;
		case ')':
			structure->token=CMD_TOK_RPAR;
			return 0;
		case ',':
			structure->token=CMD_TOK_COMMA;
			return 0;
		case '.':
			if (mygetc(f)=='.'){
				update_poz;
				structure->token=CMD_TOK_DOTDOT;
				return 0;
			}
			structure->token=CMD_TOK_UNKNOWN;
			return -1;
		case '-':
			structure->token=CMD_TOK_MINUS;
			return 0;
		case '=':
			structure->token=CMD_TOK_EQ;
			return 0;
		case '"':
			for(i=0,c=mygetc(f);c!=EOF && c!='"' && i<STR_MAX-1 ;c=mygetc(f),++i){
				structure->str[i]=c;	
			}
			structure->str[i]=0;
			if (c=='"'){
				structure->token=CMD_TOK_STR;
				return 0;
			}
			else {
				structure->token=CMD_TOK_UNKNOWN;
				return -1;
			}
		case '0':
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
			structure->number=c-'0';
			structure->token=CMD_TOK_INT;
			while((c=mygetc(f))!=EOF && isdigit(c)){
				update_poz;
				structure->number=structure->number * 10 + (c - '0');
			}
			if (c!='.'){
				if (c!=EOF){
					//un_update_poz;
					myungetc(f);
				}
				return 0;
			}
			c=mygetc(f);
			update_poz;
			if (isdigit(c)){
				long frac=10;
				structure->token=CMD_TOK_REAL;
				structure->real_number=structure->number + ((double)(c - '0'))/frac;
				while((c=mygetc(f))!=EOF && isdigit(c)){
					update_poz;
					frac*=10;
					structure->real_number+= ((double)(c - '0'))/frac;
				}
				if (c!=EOF){
					//un_update_poz;
					myungetc(f);
				}
				return 0;
			}else{
				if (c=='.'){
					*next_token=CMD_TOK_DOTDOT;
					return 0;
				}
				structure->token=CMD_TOK_UNKNOWN;
				return -1;
			}
		case '$':
			structure->token=CMD_TOK_DOLLAR;
			structure->number=-1;
			return 0;
		case EOF:
			structure->token=CMD_TOK_EOF;
			return -1;
		default:
			if (isalpha(c)){
				structure->str[0]=c;
				i=1;
				while(i<STR_MAX-1 && (c=mygetc(f))!=EOF && (isalnum(c) || c=='.' || c=='_' || c=='-' || c=='/')){
					update_poz;
					structure->str[i]=c;	
					++i;
				}
				if (c!=EOF){
			//		un_update_poz;
					myungetc(f);
				}
				structure->str[i]=0;
				structure->token=CMD_TOK_ID;
				return 0;
			}
			structure->token=CMD_TOK_UNKNOWN;
			return -1;
	}
}

static int cmd_read(page_list_head * p_doc, param params[], cmd_page_list_head * pages){
	int retval;
	page_list_head * p_doc_new;
	p_doc_new=pages_list_open(params[0].str);
	if (p_doc_new==NULL){
		return -1;
	}
	retval=pages_list_cat(p_doc,p_doc_new);
	if (retval!=0){
		pages_list_delete(p_doc_new);
	}
	return retval;
}

static int cmd_write(page_list_head * p_doc, param params[], cmd_page_list_head * pages){
	return pages_list_save(p_doc,params[0].str);
}

static int cmd_select(page_list_head * p_doc, param params[], cmd_page_list_head * pages){
	param p[3];
	p[0].type=CMD_TOK_INT;
	p[0].int_number=pages_count(p_doc);
	p[1].type=CMD_TOK_INT;
	p[1].int_number=0;
	p[2].type=CMD_TOK_INT;
	p[2].int_number=pages_count(p_doc);
	return cmd_modulo(p_doc,p,pages);
}
static int cmd_book(page_list_head * p_doc, param params[], cmd_page_list_head * pages){
	int i;
	page_list * p1,*p2,*p3,*p4;
	page_list_head * new;
	new=pages_list_new(p_doc,0);
	while (pages_count(p_doc)%4){
		page_list * pg_handle;
		pg_handle=page_new_ext(NULL,p_doc->doc->type,p_doc->doc);
		if (pg_handle==NULL){
			return -1;
		}
		pages_list_add_page(p_doc,pg_handle,pg_add_end);
	}
	p1=page_prev(page_end(p_doc));
	p2=page_next(page_begin(p_doc));
	p3=page_next(p2);
	p4=page_prev(p1);
	for (i=0;i<pages_count(p_doc)/4;++i){
		pages_list_add_page(new, page_new(p1,0),pg_add_end);
		pages_list_add_page(new, page_new(p2,0),pg_add_end);
		pages_list_add_page(new, page_new(p3,0),pg_add_end);
		pages_list_add_page(new, page_new(p4,0),pg_add_end);

		p1=page_prev(page_prev(p1));
		p2=page_next(page_next(p2));
		p3=page_next(page_next(p3));
		p4=page_prev(page_prev(p4));

	}
	pages_list_empty(p_doc);
	pages_list_cat(p_doc, new);
	return 0;
}

static int cmd_modulo(page_list_head * p_doc, param params[], cmd_page_list_head * pages){
	page_list_head * range, * new;
	page_list * new_page, * page_end;
	cmd_page_list * ranges;
	int invers;
	int pages_count;
	int i=1,modulo=params[0].int_number;
	int half = params[1].int_number;
	int round = params[2].int_number;
	int positive = 0;
	int negative = 0;

	if (half==-1){
		half=0;
		for (ranges=pages->next;ranges!=(cmd_page_list*)pages;ranges=ranges->next){
			if (ranges->negativ_range){
				negative = 1;
			}
			else {
				positive = 1;
			}
			if (negative && positive){
				half = 1;
				break;
			}
		}
	}

	if (round==-1){
		round=half?2*modulo:modulo;
	}
	else {
		round = max(round,modulo);
	}
	while (pages_count(p_doc)%round){
		page_list * pg_handle;
		pg_handle=page_new_ext(NULL,p_doc->doc->type,p_doc->doc);
		if (pg_handle==NULL){
			return -1;
		}
		pages_list_add_page(p_doc,pg_handle,pg_add_end);
	}
	
	new=pages_list_new(p_doc,0);
	pages_count=pages_count(p_doc);
	if (half){
		pages_count = pages_count/2;
	}
	for (i=0;i<pages_count;i+=modulo){
		for (ranges=pages->next;ranges!=(cmd_page_list*)pages;ranges=ranges->next){
			range=pages_list_new(p_doc,0);

			if (ranges->range[0]==-1){
				ranges->range[0]=modulo;
			}

			if (ranges->range[1]==-1){
				ranges->range[1]=modulo;
			}

			invers = (ranges->range[0]>ranges->range[1]);
			if (!ranges->negativ_range){
				new_page=page_num_to_ptn(p_doc, ranges->range[0]+i);
				page_end=page_num_to_ptn(p_doc, ranges->range[1]+i);
			}
			else{
				new_page=page_num_to_ptn(p_doc, -1*ranges->range[1]-i);
				page_end=page_num_to_ptn(p_doc, -1*ranges->range[0]-i);
			}
			
			if (new_page==NULL || page_end==NULL){
				message(FATAL,"Wrong ranges %ld, %ld\n",ranges->range[0]+i,  -1*ranges->range[1]-i);
				return -1;
			}

			/*duplicate page list*/
			for(;new_page!=page_end;new_page=invers?page_prev(new_page):page_next(new_page)){
				pages_list_add_page(range, page_new(new_page,0),pg_add_end);
			}
			pages_list_add_page(range, page_new(new_page,0),pg_add_end);
			

			if (cmd_exec_tree(range, &(ranges->commands), 0)==-1){
				return -1;
			}
			pages_list_cat(new,range);
		}
	}
	pages_list_empty(p_doc);
	pages_list_cat(p_doc, new);

	return 0;
}

/*V teto fci pristupuji lowlevel do struktury, asi by se to nemelo delat, ale je to rychlejsi.*/
static int cmd_apply(page_list_head * p_doc, param params[], cmd_page_list_head * pages){
	page_list_head selected;
	page_list * begin, * end;
	cmd_page_list * range;
	cmd_page_list_head sorted_pages;
	int count;
	
	if (cmd_sort_range(&sorted_pages,pages,pages_count(p_doc))==-1){
		/*error during sorting*/
		return -1;
	}
	
	selected.id=M_ID_DOC_PAGE_LIST_HEAD;
	selected.doc=p_doc->doc;
	for(range=sorted_pages.next;range!=(cmd_page_list*)&(sorted_pages);range=range->next){
		if (range->range[0]>range->range[1]){
			return -1;
		}

		begin=page_num_to_ptn(p_doc, range->range[0]);
		end=page_num_to_ptn(p_doc, range->range[1]+1);
		
		if (begin==NULL || end==NULL){
			return -1;
		}
		/*TODO: odstranit zbytecne kopirovani seznamu*/
		count=pages_count(p_doc);
		count=count-range->range[0]-range->range[1]+1;
		pages_count(p_doc)=pages_count(p_doc)-count;
		selected.next=begin;
		selected.prev=end->prev;
		end->prev=begin->prev;
		end->prev->next=end;
		selected.next->prev=(page_list*)&selected;
		selected.prev->next=(page_list*)&selected;
	
		if (cmd_exec_tree(&selected, &(range->commands), 0)==-1){
			return -1;
		}
		/*TODO: odstranit zbytecne kopirovani seznamu*/
		if (selected.next!=(page_list *) &selected){
			end->prev->next=selected.next;
			selected.next->prev=end->prev;
			end->prev=selected.prev;
			end->prev->next=(page_list*)end;
		}
		pages_count(p_doc)=pages_count(p_doc)+count;
	}
	cmd_free_range(&sorted_pages);
	return 0;
}

static int cmd_new(page_list_head * p_doc, param params[], cmd_page_list_head * pages){
	page_list_head *  new_list;
	cmd_page_list * cmds=NULL;
	if (pages->next!=(cmd_page_list*)&(pages)){
		cmds=pages->next;
	}
	new_list=pages_list_new(p_doc,0);
	pages_list_add_page(new_list,page_new_ext(NULL,p_doc->doc->type,p_doc->doc),pg_add_end);	
	if (cmds->commands.next!=NULL){ 
		if (cmd_exec_tree(new_list, &(cmds->commands),0)==-1){
			return -1;
		}
	}
	pages_list_cat(p_doc,new_list);
	return 0;
}
static int cmd_del(page_list_head * p_doc, param params[], cmd_page_list_head * pages){
	pages_list_empty(p_doc);	
	return 0;
}

static int cmd_scale(page_list_head * p_doc, param params[], cmd_page_list_head * pages){
	pages_scale(p_doc,params[0].real_number);
	return 0;
}

static int cmd_scaleto(page_list_head * p_doc, param params[], cmd_page_list_head * pages){
	dimensions dim;
	doc_get_pformat_name_to_dimensions(params[0].str,&dim);
	if (isdimzero(dim)){
		message(FATAL,"%s is unknown paper size\n",params[0].str);
	}
	pages_scaleto(p_doc,&dim,params[1].real_number,params[2].real_number,params[3].real_number,params[4].real_number);
	return 0;
}
static int cmd_scaleto2(page_list_head * p_doc, param params[], cmd_page_list_head * pages){
	dimensions dim;
	dim.left.x=0;
	dim.left.y=0;
	dim.right.x=params[0].real_number;
	dim.right.y=params[1].real_number;

	pages_scaleto(p_doc,&dim,params[2].real_number,params[3].real_number,params[4].real_number,params[5].real_number);
	return 0;
}
static int cmd_flip(page_list_head * p_doc, param params[], cmd_page_list_head * pages){
	int mode1, mode2;
	mode1=str_to_id(params[0].str,ids_orient,get_ids_len(ids_orient));
	mode2=DOC_O_UNKNOWN;

	if (mode1==-1){
		return -1;
	}
	return pages_flip(p_doc,mode1,mode2);
}

static int cmd_number(page_list_head * p_doc, param params[], cmd_page_list_head * pages){
	return pages_number(p_doc,params[0].real_number,params[1].real_number,params[2].int_number,
			params[3].str,params[4].str,params[5].int_number);
}

static int cmd_crop(page_list_head * p_doc, param params[], cmd_page_list_head * pages){
	dimensions dim;
	doc_get_pformat_name_to_dimensions(params[0].str,&dim);
	if (isdimzero(dim)){
		message(FATAL,"%s is unknown paper size\n",params[0].str);
	}
	pages_crop(p_doc,&dim);
	return 0;
}

static int cmd_crop2(page_list_head * p_doc, param params[], cmd_page_list_head * pages){
	dimensions dim;
	dim.left.x=params[0].real_number;
	dim.left.y=params[1].real_number;
	dim.right.x=params[2].real_number;
	dim.right.y=params[3].real_number;
	pages_crop(p_doc,&dim);
	return 0;
}

static int cmd_paper(page_list_head * p_doc, param params[], cmd_page_list_head * pages){
	dimensions dim;
	doc_get_pformat_name_to_dimensions(params[0].str,&dim);
	if (isdimzero(dim)){
		message(FATAL,"%s is unknown paper size\n",params[0].str);
	}
	set_paper_size(p_doc,&dim);
	return 0;
}

static int cmd_paper2(page_list_head * p_doc, param params[], cmd_page_list_head * pages){
	dimensions dim;
	dim.left.x=0;
	dim.left.y=0;
	dim.right.x=params[0].real_number;
	dim.right.y=params[1].real_number;
	set_paper_size(p_doc,&dim);
	return 0;
}
static int cmd_orient(page_list_head * p_doc, param params[], cmd_page_list_head * pages){
	orientation orient  = DOC_O_UNKNOWN; 
	if (strcmp(params[0].str,"landscape")==0){
		orient = DOC_O_LANDSCAPE;
		goto next_or;
	}
	if (strcmp(params[0].str,"portrait")==0){
		orient = DOC_O_PORTRAIT;
		goto next_or;
	}
	if (strcmp(params[0].str,"seascape")==0){
		orient = DOC_O_SEASCAPE;
		goto next_or;
	}
	if (strcmp(params[0].str,"upside_down")==0){
		orient = DOC_O_UPSIDE_DOWN;
	}
	next_or:
	set_paper_orient(p_doc,orient);	
	return 0;
}
static int cmd_nup(page_list_head * p_doc, param params[], cmd_page_list_head * pages){
	int x,y, orient,rotate,order_bbox,frame, center=1,scale=1;
	double dx,dy;
	dimensions bbox;


	x=params[0].int_number;
	y=params[1].int_number;
	dx=params[2].real_number;
	dy=params[3].real_number;
	scale = params[10].int_number;
	/*TODO: doresit nastaveni orientace stranky!!!*/
	if (strcmp(params[4].str,"unknown") == 0){
		orient=p_doc->doc->orient;
		orient=(orient==DOC_O_UNKNOWN)?DOC_O_PORTRAIT:DOC_O_LANDSCAPE;
	}
	else{
		orient=str_to_id(params[4].str,ids_orient,get_ids_len(ids_orient));
		if (orient == -1){
			return -1;
		}
	}
	
	rotate=params[5].int_number;
	order_bbox=params[6].int_number;
	if (params[7].str==NULL){
		copy_dimensions(&bbox,&p_doc->doc->paper);
	}
	else{
		doc_get_pformat_name_to_dimensions(params[7].str,&bbox);
		if (isdimzero(bbox)){
			message(FATAL,"%s is unknown paper size\n",params[7].str);
		}
	}
	frame=params[8].int_number;
	/*TODO: doresit rozmery!!!*/
	if (params[9].str!=NULL){
		center=str_to_id(params[9].str,ids_center_nup,get_ids_len(ids_center_nup));
		if (center==-1){
			return -1;
		}
	}
	
	return pages_nup(p_doc,x,y,&bbox,dx,dy,orient,rotate,order_bbox,frame, center, scale);
}


static int cmd_cmarks(page_list_head * p_doc, param params[], cmd_page_list_head * pages){
	return pages_cmarks(p_doc, params[0].int_number);
}
static int cmd_norm(page_list_head * p_doc, param params[], cmd_page_list_head * pages){
	int center;
	center=1;
	if (params[0].str){
		center = str_to_id(params[0].str,ids_center,get_ids_len(ids_center));
		if (center == -1){
			return -1;
		}
	}
	return pages_norm(p_doc, center, params[1].int_number, params[2].int_number, params[3].int_number);
}
static int cmd_bbox(page_list_head * p_doc, param params[], cmd_page_list_head * pages){
	return doc_update_bbox(p_doc);
}
static int cmd_merge(page_list_head * p_doc, param params[], cmd_page_list_head * pages){
	return pages_to_one(p_doc);
}

static int cmd_text(page_list_head * p_doc, param params[], cmd_page_list_head * pages){
	return pages_text(p_doc,params[0].real_number,params[1].real_number,
			params[2].str,params[3].str,params[4].int_number);
}

static int cmd_line(page_list_head * p_doc, param params[], cmd_page_list_head * pages){
	return pages_line(p_doc,params[0].real_number,params[1].real_number,
			params[2].real_number,params[3].real_number,params[4].real_number);
}

static int cmd_rotate(page_list_head * p_doc, param params[], cmd_page_list_head * pages){
	pages_rotate(p_doc,params[0].int_number);
	return 0;
}
static int cmd_move(page_list_head * p_doc, param params[], cmd_page_list_head * pages){
	pages_move_xy(p_doc,params[0].real_number,params[1].real_number);
	return 0;
}
static int cmd_duplex(page_list_head * p_doc, param params[], cmd_page_list_head * pages){
	char cmds[] = " modulo(2,0){ 1 2 rotate(180) }"; 
	MYFILE * f;
	cmd_ent_struct_head cmd_ent;
/*TODO: add api to vdoc for working with duplex*/
	if (params[0].int_number){
		f = stropen(cmds);
		assert(cmd_preexec(&cmd_ent, f));
		assert(cmd_exec(p_doc, &cmd_ent, f)==0);
		myfclose(f);
	}
	return 0;

}

static int cmd_matrix(page_list_head * p_doc, param params[], cmd_page_list_head * pages){
	transform_matrix matrix;
	matrix[0][0] = params[0].real_number;
	matrix[0][1] = params[1].real_number;
	matrix[0][2] = 0;
	matrix[1][0] = params[2].real_number;
	matrix[1][1] = params[3].real_number;
	matrix[1][2] = 0;
	matrix[2][0] = params[4].real_number;
	matrix[2][1] = params[5].real_number;
	matrix[2][2] = 1;
	return pages_transform(p_doc, &matrix);
}

static int cmd_spaper(page_list_head * p_doc, param params[], cmd_page_list_head * pages) {
	doc_set_pformat_dimensions(params[0].str, params[1].real_number, params[2].real_number);	
	return 0;
}


void cmd_print_info(FILE *f){
	int i=0;
	int j;
	for (i=0;cmd_commands[i].help!=NULL;++i){
		fprintf(f,"%s",cmd_commands[i].str);
		if (cmd_commands[i].params_count){
			for (fprintf(f,"("),j=0;j<cmd_commands[i].params_count || (fprintf(f,")"),0);++j,((j<cmd_commands[i].params_count)?fprintf(f,","):0)){
				fprintf(f,"%s=",cmd_commands[i].params[j].name);
				switch(cmd_commands[i].params[j].type){
					case CMD_TOK_INT:
						if (cmd_commands[i].params[j].value==CMD_TOK_INT){
							fprintf(f,"%ld",cmd_commands[i].params[j].int_number);
						}
						else{
							fprintf(f,"<int>");
						}
						break;
					case CMD_TOK_REAL:
						if (cmd_commands[i].params[j].value==CMD_TOK_REAL){
							fprintf(f,"%.3f",cmd_commands[i].params[j].real_number);
						}
						else{
							fprintf(f,"<real>");
						}
						break;
					case CMD_TOK_ID:
						if (cmd_commands[i].params[j].value==CMD_TOK_ID){
							if (cmd_commands[i].params[j].str){
								fprintf(f,"%s",cmd_commands[i].params[j].str);
							}
							else{
								fprintf(f,"default");
							}
						}
						else{
							fprintf(f,"<id>");
						}
						break;
					case CMD_TOK_STR:
						if (cmd_commands[i].params[j].value==CMD_TOK_STR){
							if (cmd_commands[i].params[j].str){
								fprintf(f,"\"%s\"",cmd_commands[i].params[j].str);
							}
							else{
								fprintf(f,"\"%s\"","default");
							}
						}
						else{
							fprintf(f,"<str>");
						}
						break;
					case CMD_TOK_MEASURE:
						if (cmd_commands[i].params[j].value==CMD_TOK_MEASURE){
							fprintf(f,"%.2f pt",cmd_commands[i].params[j].real_number);
						}
						else{
							fprintf(f,"<measure>");
						}
						break;
					default:
						assert(0);
						break;
				}
			}
		}
		if (cmd_commands[i].pages){
			fprintf(f,"{page_ranges}");
		}

		fprintf(f, "\n                  %s\n", (cmd_commands[i].help) && strlen(cmd_commands[i].help)  ? (cmd_commands[i].help) : ("missing"));
	}
}

static int cmd_pinfo(page_list_head * p_doc, param params[], cmd_page_list_head * pages) {
	pages_info(p_doc, stdout);
	return 0;
}


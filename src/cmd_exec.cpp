/* This file is a part of pdfcook program, which is GNU GPLv2 licensed */
#include "cmd_exec.h"
#include "debug.h"
#include "fileio.h"
#include "pdf_doc.h"
#include "doc_edit.h"

#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define STR_MAX 255
#define PARAM_MAX 12


typedef struct cmd_param {
    cmd_param * next;
    cmd_param * prev;
    int type;
    char name[STR_MAX];
    long integer;
    double real;
    char str[STR_MAX];
} cmd_param;

typedef struct {
    struct cmd_param * next;
    struct cmd_param * prev;
} cmd_param_head;


class Command
{
public:
    char name[STR_MAX];
    int row;
    int column;
    int params_count;
    cmd_param_head params;
    PageRanges page_ranges;

    Command(char *cmd_name, int row, int column);
};


typedef struct {
    const char *name;
    int type;
    int val_type;
    long integer;
    double real;
    const char *str;
} Param;


// predefined command name entry
typedef struct {
    const char *name;
    const char *help;
    bool (*cmd_func)(PdfDocument &doc, Param params[], PageRanges &page_ranges);
    Param *params;
    int params_count;
} cmd_entry;

/*tokens*/
typedef enum {
    CMD_TOK_UNKNOWN, CMD_TOK_EOF, CMD_TOK_ID, CMD_TOK_STR,
    CMD_TOK_INT, CMD_TOK_REAL, CMD_TOK_DOLLAR, CMD_TOK_DOTDOT,
    CMD_TOK_COMMA, CMD_TOK_EQ,
    CMD_TOK_LPAR, CMD_TOK_RPAR,
    CMD_TOK_LCPAR, CMD_TOK_RCPAR,
    CMD_TOK_MINUS, CMD_TOK_MEASURE,
    CMD_TOK_ODD, CMD_TOK_EVEN
} CmdTokenType;

typedef struct {
    int token;
    char str[STR_MAX];
    long integer;
    double real;
    int row;
    int column;
} CmdToken;


static int cmd_get_token(MYFILE * f,CmdToken * structure);



static bool cmd_read(PdfDocument &doc, Param params[], PageRanges &pages);
static bool cmd_write(PdfDocument &doc, Param params[], PageRanges &pages);
static bool cmd_new(PdfDocument &doc, Param params[], PageRanges &pages);
static bool cmd_del(PdfDocument &doc, Param params[], PageRanges &pages);
static bool cmd_select(PdfDocument &doc, Param params[], PageRanges &pages);
static bool cmd_modulo(PdfDocument &doc, Param params[], PageRanges &pages);
static bool cmd_nup(PdfDocument &doc, Param params[], PageRanges &pages);
static bool cmd_book(PdfDocument &doc, Param params[], PageRanges &pages);
static bool cmd_crop(PdfDocument &doc, Param params[], PageRanges &pages);
static bool cmd_crop2(PdfDocument &doc, Param params[], PageRanges &pages);
static bool cmd_flip(PdfDocument &doc, Param params[], PageRanges &pages);
static bool cmd_line(PdfDocument &doc, Param params[], PageRanges &pages);
static bool cmd_matrix(PdfDocument &doc, Param params[], PageRanges &pages);
static bool cmd_move(PdfDocument &doc, Param params[], PageRanges &pages);
static bool cmd_number(PdfDocument &doc, Param params[], PageRanges &pages);
static bool cmd_paper(PdfDocument &doc, Param params[], PageRanges &pages);
static bool cmd_paper2(PdfDocument &doc, Param params[], PageRanges &pages);
static bool cmd_scale(PdfDocument &doc, Param params[], PageRanges &pages);
static bool cmd_scaleto(PdfDocument &doc, Param params[], PageRanges &pages);
static bool cmd_scaleto2(PdfDocument &doc, Param params[], PageRanges &pages);
static bool cmd_spaper(PdfDocument &doc, Param params[], PageRanges &pages);
static bool cmd_rotate(PdfDocument &doc, Param params[], PageRanges &pages);
static bool cmd_text(PdfDocument &doc, Param params[], PageRanges &pages);
static bool cmd_pinfo(PdfDocument &doc, Param params[], PageRanges &pages);

/* {name, type, default_val_type, default_int_val, default_real_val, default_str_val}
default_value_type = CMD_TOK_UNKNOwN means no default value provided
if default value type is real, pass argument to default_real_val field only.
then default_int_val will be 0 and default_char_val will be NULL
*/
static Param  cmd_read_params[] = {{"name", CMD_TOK_STR, CMD_TOK_UNKNOWN, 0,0,NULL}};
static Param  cmd_write_params[] = {{"name", CMD_TOK_STR, CMD_TOK_UNKNOWN, 0,0,NULL}};
static Param  cmd_modulo_params[] = {
                {"step", CMD_TOK_INT, CMD_TOK_UNKNOWN, 0,0,NULL},
                {"round", CMD_TOK_INT, CMD_TOK_INT, 1,0,NULL}
};

static Param  cmd_scale_params[] = {{"scale",CMD_TOK_REAL,CMD_TOK_UNKNOWN,0,0,NULL}};

static Param  cmd_scaleto_params[] = {
                {"paper", CMD_TOK_ID, CMD_TOK_UNKNOWN, 0,0,NULL},
                {"top", CMD_TOK_MEASURE, CMD_TOK_MEASURE, 0,0,NULL},
                {"right", CMD_TOK_MEASURE, CMD_TOK_MEASURE, 0,0,NULL},
                {"bottom", CMD_TOK_MEASURE, CMD_TOK_MEASURE, 0,0,NULL},
                {"left", CMD_TOK_MEASURE, CMD_TOK_MEASURE, 0,0,NULL},
                {"orient", CMD_TOK_ID, CMD_TOK_ID, 0,0,"auto"}
};

static Param  cmd_scaleto2_params[] = {
                {"w", CMD_TOK_MEASURE,CMD_TOK_UNKNOWN, 0,0,NULL},
                {"h", CMD_TOK_MEASURE,CMD_TOK_UNKNOWN, 0,0,NULL},
                {"top", CMD_TOK_MEASURE,CMD_TOK_MEASURE, 0,0,NULL},
                {"right", CMD_TOK_MEASURE,CMD_TOK_MEASURE, 0,0,NULL},
                {"bottom", CMD_TOK_MEASURE,CMD_TOK_MEASURE, 0,0,NULL},
                {"left", CMD_TOK_MEASURE,CMD_TOK_MEASURE, 0,0,NULL}
};
static Param  cmd_flip_params[] = {{"mode", CMD_TOK_ID, CMD_TOK_ID, 0,0,"h"}};

static Param cmd_number_params[] = {
                {"x", CMD_TOK_MEASURE, CMD_TOK_MEASURE, 0,-1,NULL},
                {"y", CMD_TOK_MEASURE, CMD_TOK_MEASURE, 0,-1,NULL},
                {"start", CMD_TOK_INT, CMD_TOK_INT, 1,0,NULL},
                {"text", CMD_TOK_STR, CMD_TOK_STR, 0,0,"%d"},
                {"size", CMD_TOK_INT, CMD_TOK_INT, 10,0,NULL},
                {"font", CMD_TOK_STR, CMD_TOK_STR, 0,0,"Helvetica"}
};

static Param cmd_crop_params[] = {
                {"paper", CMD_TOK_ID, CMD_TOK_UNKNOWN, 0,0,NULL},
                {"orient", CMD_TOK_ID, CMD_TOK_ID, 0,0,"auto"}
};

static Param cmd_crop2_params[] = {
                {"lx", CMD_TOK_MEASURE, CMD_TOK_UNKNOWN, 0,0,NULL},
                {"ly", CMD_TOK_MEASURE, CMD_TOK_UNKNOWN, 0,0,NULL},
                {"hx", CMD_TOK_MEASURE, CMD_TOK_UNKNOWN, 0,0,NULL},
                {"hy", CMD_TOK_MEASURE, CMD_TOK_UNKNOWN, 0,0,NULL}
};
static Param cmd_paper_params[]= {
                {"paper", CMD_TOK_ID, CMD_TOK_UNKNOWN, 0,0,NULL},
                {"orient", CMD_TOK_ID, CMD_TOK_ID, 0,0,"auto"}
};

static Param cmd_paper2_params[] = {
                {"w", CMD_TOK_MEASURE, CMD_TOK_UNKNOWN, 0,0,NULL},
                {"h", CMD_TOK_MEASURE, CMD_TOK_UNKNOWN, 0,0,NULL}
};
static Param cmd_nup_params[] = {
                {"n", CMD_TOK_INT, CMD_TOK_INT, 2,0,NULL},
                {"cols", CMD_TOK_INT, CMD_TOK_INT, 2,0,NULL},
                {"dx", CMD_TOK_MEASURE, CMD_TOK_MEASURE, 0,0,NULL},
                {"dy", CMD_TOK_MEASURE, CMD_TOK_MEASURE, 0,0,NULL},
                {"paper", CMD_TOK_ID, CMD_TOK_ID, 0,0,"a4"},
                {"orient", CMD_TOK_ID, CMD_TOK_ID, 0,0,"auto"}
};

static Param cmd_line_params[] = {
                {"lx", CMD_TOK_MEASURE, CMD_TOK_UNKNOWN, 0,0,NULL},
                {"ly", CMD_TOK_MEASURE, CMD_TOK_UNKNOWN, 0,0,NULL},
                {"hx", CMD_TOK_MEASURE, CMD_TOK_UNKNOWN, 0,0,NULL},
                {"hy", CMD_TOK_MEASURE, CMD_TOK_UNKNOWN, 0,0,NULL},
                {"width", CMD_TOK_MEASURE, CMD_TOK_MEASURE, 0,1,NULL}
};

static Param cmd_text_params[] = {
                {"x", CMD_TOK_MEASURE, CMD_TOK_UNKNOWN, 0,0,NULL},
                {"y", CMD_TOK_MEASURE, CMD_TOK_UNKNOWN, 0,0,NULL},
                {"text", CMD_TOK_STR, CMD_TOK_UNKNOWN, 0,0,NULL},
                {"size", CMD_TOK_INT, CMD_TOK_INT, 10,0,NULL},
                {"font", CMD_TOK_STR, CMD_TOK_STR, 0,0,"Helvetica"}
};
static Param cmd_rotate_params[] = {{"angle",CMD_TOK_INT, CMD_TOK_INT, 270,0,NULL}};

static Param cmd_move_params[] = {
                {"x", CMD_TOK_MEASURE, CMD_TOK_MEASURE, 0,0,NULL},
                {"y", CMD_TOK_MEASURE, CMD_TOK_MEASURE, 0,0,NULL}
};
static Param cmd_matrix_params[]   = {
                {"a", CMD_TOK_REAL, CMD_TOK_REAL, 0,1,NULL},
                {"b", CMD_TOK_REAL, CMD_TOK_REAL, 0,0,NULL},
                {"c", CMD_TOK_REAL, CMD_TOK_REAL, 0,0,NULL},
                {"d", CMD_TOK_REAL, CMD_TOK_REAL, 0,1,NULL},
                {"e", CMD_TOK_REAL, CMD_TOK_REAL, 0,0,NULL},
                {"f", CMD_TOK_REAL, CMD_TOK_REAL, 0,0,NULL}
};
static Param  cmd_spaper_params[] = {
                {"name", CMD_TOK_ID, CMD_TOK_UNKNOWN, 0,0,NULL},
                {"x", CMD_TOK_MEASURE, CMD_TOK_UNKNOWN, 0,0,NULL},
                {"y", CMD_TOK_MEASURE, CMD_TOK_UNKNOWN, 0,0,NULL}};

// fills parameters and parameters count
#define fill_params(p) p,sizeof(p)/sizeof(Param)
/* { name, help, function, parameters, parameters count} */
static cmd_entry cmd_commands[] = {
    {"info",    "Print information about each page", cmd_pinfo, NULL,0},
    {"new",     "Insert a new blank page", cmd_new, NULL,0},
    {"del",     "Delete pages, must add page ranges", cmd_del, NULL,0},
    {"select",  "Select pages to save", cmd_select, NULL,0},
    {"modulo",  "Advanced Select pages (see manual page)",cmd_modulo,fill_params(cmd_modulo_params)},
	{"nup",     "Print n pages per page", cmd_nup, fill_params(cmd_nup_params)},
	{"book",    "Arrage pages for printing booklets", cmd_book, NULL,0},
	{"crop",    "make outside area blank white", cmd_crop, fill_params(cmd_crop_params)},
	{"crop2",   "Crop with exact size", cmd_crop2, fill_params(cmd_crop2_params)},
	{"flip",    "horizontal | vertical (v|h)", cmd_flip, fill_params(cmd_flip_params)},
	{"line",    "Draw line on page", cmd_line, fill_params(cmd_line_params)},
	{"matrix",  "Transform by matrix", cmd_matrix, fill_params(cmd_matrix_params)},
	{"move",    "Move or translate page by x and y points",cmd_move,fill_params(cmd_move_params)},
    {"number",  "Add page numbers", cmd_number, fill_params(cmd_number_params)},
	{"paper",   "Set paper size (doesn't scale contents)", cmd_paper, fill_params(cmd_paper_params)},
	{"paper2",  "Set exact paper size", cmd_paper2, fill_params(cmd_paper2_params)},
	{"rotate",  "Rotate pages", cmd_rotate, fill_params(cmd_rotate_params)},
    {"scale",   "Scale pages by factor", cmd_scale, fill_params(cmd_scale_params)},
    {"scaleto", "Scale pages by standard paper sizes", cmd_scaleto, fill_params(cmd_scaleto_params)},
	{"scaleto2","Scale to exact width and height", cmd_scaleto2, fill_params(cmd_scaleto2_params)},
	{"spaper",  "Define new paper format", cmd_spaper, fill_params(cmd_spaper_params)},
	{"text",    "Write text on page", cmd_text, fill_params(cmd_text_params)},
    {"read",    "Append file at the end (join pdfs)",cmd_read,fill_params(cmd_read_params)},
    {"write",   "Save to file", cmd_write, fill_params(cmd_write_params)},
    {NULL, NULL, 0}
};


typedef struct {
    const char *name;
    double value;
} unit_val_ent;

// unit to points
static unit_val_ent table[] = {
    {"cm", 28.346456692913385211},
    {"mm", 2.8346456692913385211},
    {"in", 1*72.0},
    {"pt", 1},
    {NULL, 0}
};

static double get_unit_val(char * unit){
    int i;
    for (i=0; table[i].name; ++i){
        if (strcmp(unit,table[i].name)==0){
            return table[i].value;
        }
    }
    return 0;
}

typedef struct {
    const char *str;
    int id;
} id_str;

id_str ids_orient[] = {
	{"portrait", ORIENT_PORTRAIT},
	{"landscape", ORIENT_LANDSCAPE},
	{"vertical", ORIENT_LANDSCAPE},
	{"horizontal", ORIENT_PORTRAIT},
	{"v", ORIENT_LANDSCAPE},
	{"h", ORIENT_PORTRAIT}
};

#define get_ids_len(a) (sizeof((a))/sizeof(id_str))

static int str_to_id(const char *str, id_str ids[], size_t len)
{
    if (str==NULL) return -1;
	uint i;
	for (i=0; i<len && strcmp(str, ids[i].str); ++i);
	if (i==len){
		return -1;
	}
    return ids[i].id;
}

Orientation orientation_from_str(const char *str)
{
    int orientation = str_to_id(str, ids_orient, get_ids_len(ids_orient));

    switch (orientation) {
        case ORIENT_PORTRAIT:
            return ORIENT_PORTRAIT;
        case ORIENT_LANDSCAPE:
            return ORIENT_LANDSCAPE;
        default :
            return ORIENT_AUTO;
    }
}


static void cmd_free_args(cmd_param_head * params){
    cmd_param *item, *item2;
    for (item = params->next; item!=(cmd_param *) params; ){
        item2 = item;
        item = item->next;
        free(item2);
    }
}

static void cmd_list_free(CmdList &cmd_list)
{
    for (Command *cmd : cmd_list) {
        cmd_free_args(&(cmd->params));
        cmd->page_ranges.clear();
        delete cmd;
    }
    cmd_list.clear();
}

// parse the page ranges after a command. the page ranges are enclosed by { }
static int cmd_get_pages_args(Command *cmd, MYFILE * f, CmdToken * p_tok)
{
    bool negativ_range;
    long r_begin, r_end, pos;
    // if does not start with '{' it is not page ranges
    if (p_tok->token!=CMD_TOK_LCPAR){
         return 0;
    }
    cmd->page_ranges.clear();// as we have previously added all pages

    while (1) {
        negativ_range = false;
        cmd_get_token(f, p_tok);

        switch (p_tok->token)
        {
            case CMD_TOK_RCPAR:// reached end
                cmd_get_token(f, p_tok);
                return 0;
            case CMD_TOK_MINUS:
                negativ_range = true;
                cmd_get_token(f, p_tok);
                if (p_tok->token!=CMD_TOK_INT){
                    message(WARN, "page range must contain integer after minus sign");
                    return -1;
                }
            case CMD_TOK_INT:
            case CMD_TOK_DOLLAR:
                r_begin = r_end = p_tok->integer;
                pos = myftell(f);
                cmd_get_token(f, p_tok);
                if (p_tok->token==CMD_TOK_DOTDOT){
                    cmd_get_token(f,p_tok);
                    if (p_tok->token==CMD_TOK_INT || p_tok->token==CMD_TOK_DOLLAR){
                        r_end = p_tok->integer;
                    }
                    else return -1;
                }
                else
                    myfseek(f, pos, SEEK_SET);
                cmd->page_ranges.append(PageRange(r_begin, r_end, negativ_range));
                break;
            case CMD_TOK_ODD:
                cmd->page_ranges.append(PageRange( PAGE_SET_ODD ));
                break;
            case CMD_TOK_EVEN:
                cmd->page_ranges.append(PageRange( PAGE_SET_EVEN ));
                break;
            default:
                return -1;
        }
    }
    return -1;
}

// create new cmd_param and append it to Command param list, and return the cmd_param
static cmd_param * cmd_add_param(Command *cmd)
{
    assert(cmd != NULL);
    cmd_param * new_param = (cmd_param *)  malloc(sizeof(cmd_param));

    if (new_param == NULL) {
        message(FATAL, "malloc() error");
    }
    new_param->name[0] = 0;
    new_param->next = (cmd_param *)(&cmd->params);
    new_param->prev = cmd->params.prev;
    new_param->next->prev = new_param;
    new_param->prev->next = new_param;
    cmd->params_count++;
    return new_param;
}

static void cmd_set_param_val(cmd_param * param, CmdToken * p_tok)
{
    param->type = p_tok->token;
    switch(p_tok->token){
        case CMD_TOK_STR:
        case CMD_TOK_ID:
            strncpy(param->str,p_tok->str,STR_MAX);
            return;
        case CMD_TOK_INT:
            param->integer = p_tok->integer;
            return;
        case CMD_TOK_MEASURE:
        case CMD_TOK_REAL:
            param->real = p_tok->real;
            return;
        default:
            param->type = CMD_TOK_UNKNOWN;
            return;
    }
}

static int cmd_get_args(Command *cmd, MYFILE *f, CmdToken *p_tok)
{
    int comma=1;
    int eq=0;
    CmdToken tmp_tok;
    cmd_param * param;
    if (cmd_get_token(f, p_tok)==-1)
        return -1;
    if (p_tok->token!=CMD_TOK_LPAR){
        return 0;
    }
    param = cmd_add_param(cmd);
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
                    param=cmd_add_param(cmd);
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
                            p_tok->integer *= -1;
                            break;
                        case CMD_TOK_REAL:
                            p_tok->real*=-1;
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
                            param->real = param->integer;
                        case CMD_TOK_REAL:
                            param->real *= unit_val;
                            param->type = CMD_TOK_MEASURE;
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
                cmd_get_token(f, &tmp_tok);
                switch(tmp_tok. token){
                    case CMD_TOK_EQ:
                        if (eq==1){
                            return -1;
                        }
                        eq=1;
                        strncpy(param->name, p_tok->str, STR_MAX);
                        break;
                    case CMD_TOK_COMMA:
                        /*???*/
                        cmd_set_param_val(param, p_tok);
                        if (comma==1 && eq==0){
                            param = cmd_add_param(cmd);
                            comma = 1;
                        }
                        else {
                            return -1;
                        }
                        break;
                    case CMD_TOK_RPAR:
                        /*???*/
                        cmd_set_param_val(param, p_tok);
                        comma=0; eq=0;
                    default:
                        memcpy(p_tok, &tmp_tok, sizeof(CmdToken));
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
                cmd_set_param_val(param, p_tok);
                break;
            case CMD_TOK_EOF:
                p_tok->token = CMD_TOK_UNKNOWN;
                return -1;
                break;
            default:
                return -1;
        }
        cmd_get_token(f,p_tok);
    }
    return -1;
}


Command:: Command(char *cmd_name, int row, int column)
{
    if (cmd_name != NULL) {
        strncpy(&this->name[0], cmd_name, STR_MAX);
    }
    this->row = row;
    this->column = column;
    this->params_count = 0;
    params.next = params.prev = (cmd_param *)(&(params));
}

static bool cmd_list_parse_file(CmdList &cmd_list, MYFILE *f)
{
    Command *cmd;
    CmdToken token;
    cmd_get_token(f, &token);
    while ((token.token)==CMD_TOK_ID) {
        cmd = new Command(token.str, token.row, token.column);
        cmd_list.push_back(cmd);
        cmd->page_ranges.append(PageRange());// add all pages
        // get arguments
        if (cmd_get_args(cmd, f, &token)==-1){
            break;
        }
        // get page ranges for this command
        if (cmd_get_pages_args(cmd, f, &token)==-1){
            break;
        }
    }
    if (token.token==CMD_TOK_EOF) {
        return true;
    }
    if (token.token != CMD_TOK_RCPAR && token.token != CMD_TOK_INT) {
        message(ERROR, "Syntax error at line %d column %d", token.row, token.column);
    }
    return false;
}

// on success, it only returns true, if failed exits the program
static bool cmd_exec(Command *cmd, PdfDocument &doc, bool test)
{
    // find index of command in the command entries
    int cmd_entry_count = sizeof(cmd_commands)/sizeof(cmd_entry)-1;
    int index;
    for (index=0; index<cmd_entry_count; index++){
        if (strcmp(cmd_commands[index].name, cmd->name)==0){
            break;
        }
    }
    if (index == cmd_entry_count) // command name did not match
        message(FATAL, "Unknown command '%s' at line %d column %d", cmd->name, cmd->row, cmd->column);

    int i;
    cmd_param * argument;
    Param params[PARAM_MAX];
    int params_count = cmd_commands[index].params_count;
    assert(cmd_commands[index].params_count <= PARAM_MAX);

    memcpy(params, cmd_commands[index].params, sizeof(Param)*params_count);
    // unnamed arguments
    for (i=0, argument=cmd->params.next;
        argument!=(cmd_param *)(&cmd->params) && i<params_count && argument->name[0]==0;
        argument=argument->next,++i)
    {
        switch (params[i].type){
            case CMD_TOK_INT:
                if(argument->type==CMD_TOK_INT){
                    params[i].val_type = CMD_TOK_INT;
                    params[i].integer = argument->integer;
                }
                else {// bad argument type
                    message(ERROR, "command '%s', param %d of is not integer",cmd_commands[index].name,i+1);
                    return false;
                }
                break;
            case CMD_TOK_REAL:
                switch (argument->type) {
                    case CMD_TOK_INT:
                        params[i].val_type = CMD_TOK_REAL;
                        params[i].real = argument->integer;
                        break;
                    case CMD_TOK_REAL:
                        params[i].val_type = CMD_TOK_REAL;
                        params[i].real = argument->real;
                        break;
                    default:// bad argument type
                        message(ERROR, "command '%s', param %d of is not number",cmd_commands[index].name,i+1);
                        return false;
                }
                break;
            case CMD_TOK_MEASURE:
                switch (argument->type) {
                case CMD_TOK_INT:
                        params[i].val_type = CMD_TOK_REAL;
                        params[i].real = argument->integer;
                    break;
                case CMD_TOK_REAL:
                case CMD_TOK_MEASURE:
                        params[i].val_type = CMD_TOK_REAL;
                        params[i].real = argument->real;
                    break;
                default:
                    message(ERROR, "command '%s', param %d of is not measure",cmd_commands[index].name,i+1);
                    return false;
                }
                break;
            case CMD_TOK_ID:
                if(argument->type==CMD_TOK_ID){
                    params[i].val_type = CMD_TOK_ID;
                    params[i].str = argument->str;
                }
                else {
                    message(ERROR, "command '%s', param %d of is not id",cmd_commands[index].name,i+1);
                    return false;
                }
                break;
            case CMD_TOK_STR:
                if(argument->type==CMD_TOK_STR){
                    params[i].val_type = CMD_TOK_STR;
                    params[i].str = argument->str;
                }
                else {
                    message(ERROR, "command '%s', param %d of is not string",cmd_commands[index].name,i+1);
                    return false;
                }
                break;
            default:
                assert(0);
        }
    }

    // for each named arguments, find and match the name and put value of that argument
    for ( ; argument!=(cmd_param *)(&cmd->params) &&  argument->name[0]!=0;
        argument=argument->next)
    {
        for (i=0; i<params_count && strcmp(params[i].name, argument->name)!=0; ++i);
        if (i==params_count){
            message(ERROR, "command '%s', unknown parameter '%s'",cmd_commands[index].name,argument->name);
            return false;
        }
        switch (params[i].type){
            case CMD_TOK_INT:
                if(argument->type==CMD_TOK_INT){
                    params[i].val_type = CMD_TOK_INT;
                    params[i].integer = argument->integer;
                }
                else {
                    message(ERROR, "command '%s', param '%s' is not integer", cmd_commands[index].name, argument->name);
                    return false;
                }
                break;
            case CMD_TOK_REAL:
                switch(argument->type){
                    case CMD_TOK_INT:
                        params[i].val_type = CMD_TOK_REAL;
                        params[i].real = argument->integer;
                        break;
                    case CMD_TOK_REAL:
                        params[i].val_type = CMD_TOK_REAL;
                        params[i].real = argument->real;
                        break;
                    default:
                        message(ERROR, "command '%s', param '%s' is not number", cmd_commands[index].name, argument->name);
                        return false;// bad argument type
                }
                break;
            case CMD_TOK_MEASURE:
                switch(argument->type){
                case CMD_TOK_INT:
                        params[i].val_type = CMD_TOK_REAL;
                        params[i].real = argument->integer;
                    break;
                case CMD_TOK_REAL:
                case CMD_TOK_MEASURE:
                        params[i].val_type = CMD_TOK_REAL;
                        params[i].real = argument->real;
                    break;
                default:
                    message(ERROR, "command '%s', param '%s' is not measure", cmd_commands[index].name, argument->name);
                    return false;
                }
                break;
            case CMD_TOK_ID:
                if (argument->type==CMD_TOK_ID){
                    params[i].val_type = CMD_TOK_ID;
                    params[i].str = argument->str;
                }
                else {
                    message(ERROR, "command '%s', param '%s' is not id", cmd_commands[index].name, argument->name);
                    return false;
                }
                break;
            case CMD_TOK_STR:
                if(argument->type==CMD_TOK_STR){
                    params[i].val_type = CMD_TOK_STR;
                    params[i].str = argument->str;
                }
                else {
                    message(ERROR, "command '%s', param '%s' is not quoted string", cmd_commands[index].name, argument->name);
                    return false;
                }
                break;
            default:
                assert(0);
        }
    }

    if (argument!=(cmd_param *)(&cmd->params)){
        return false;
    }
    for (i=0; i<params_count; ++i) {
        if (params[i].val_type==CMD_TOK_UNKNOWN){ // value should be set
            message(ERROR, "command '%s' : value of param '%s' must be set", cmd_commands[index].name, params[i].name);
            return false;
        }
    }
    if (test)
        return true;
    cmd->page_ranges.initPageNums(doc.page_list.count());
    return cmd_commands[index].cmd_func(doc, params, cmd->page_ranges);
}


static void cmd_list_exec(CmdList &cmd_list, PdfDocument &doc, bool test)
{
    for (Command *cmd : cmd_list){
        if (not cmd_exec(cmd, doc, test)) {
            message(FATAL, "failed to execute command '%s' at line %d column %d.",cmd->name, cmd->row, cmd->column);
        }
    }
}

// exec the command tree
void doc_exec_commands(PdfDocument &doc, CmdList &cmd_list)
{
    cmd_list_exec(cmd_list, doc, true);// test arguments
    cmd_list_exec(cmd_list, doc, false);// execute commands
    cmd_list_free(cmd_list);
}

// parse the commands string and create command tree
void parse_commands(CmdList &cmd_list, MYFILE *f)
{
    if (not cmd_list_parse_file(cmd_list, f)){
        cmd_list_free(cmd_list);
        message(FATAL, "There were some errors during parsing commands");
    }
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

// parse command string and get tokekn
static int cmd_get_token(MYFILE * f, CmdToken * structure)
{
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
            structure->integer = c-'0';
            structure->token=CMD_TOK_INT;
            while((c=mygetc(f))!=EOF && isdigit(c)){
                update_poz;
                structure->integer = structure->integer * 10 + (c - '0');
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
                structure->token = CMD_TOK_REAL;
                structure->real = structure->integer + ((double)(c - '0'))/frac;
                while((c=mygetc(f))!=EOF && isdigit(c)){
                    update_poz;
                    frac*=10;
                    structure->real += ((double)(c - '0'))/frac;
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
            structure->integer = -1;
            return 0;
        case '?':
            structure->token=CMD_TOK_ODD;
            structure->integer = 0;
            return 0;
        case '+':
            structure->token=CMD_TOK_EVEN;
            structure->integer = 0;
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
                    // un_update_poz;
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

// --------------- Functions for Different Commands -------------------

static bool cmd_read(PdfDocument &doc, Param params[], PageRanges &pages)
{
    PdfDocument new_doc;
    if (not new_doc.open(params[0].str))
        return false;
    doc.mergeDocument(new_doc);
    return true;
}

static bool cmd_write(PdfDocument &doc, Param params[], PageRanges &pages)
{
    return doc.save(params[0].str);
}

static bool cmd_select(PdfDocument &doc, Param params[], PageRanges &pages)
{
    return doc_pages_arrange(doc, pages);
}

static bool cmd_del(PdfDocument &doc, Param params[], PageRanges &pages)
{
    return doc_pages_delete(doc, pages);
}

// if provided {page_ranges} add blank pages at pos, else append to end
static bool cmd_new(PdfDocument &doc, Param params[], PageRanges &pages)
{
    if (pages.array.front().type==PAGE_SET_ALL)
        doc.newBlankPage(-1);
    else {
        pages.sort();
        for (int page_num : pages) {
            doc.newBlankPage(page_num);
        }
    }
    return true;
}

/*
this command iterate pages by given step.
such as if step=4, index are 0, 4, 8, 12 ...
if command is modulo(4){1 2 3} then chosen pages are
for index 0 -> 4*0+1, 4*0+2, 4*0+3 = 1, 2, 3
for index 1 -> 5, 6, 7
thus page numbers will be in 1,2,3,5,6,7,9,10,11.. order
if command is modulo(4){-1}
selected pages are -(4*0+1),  -(4*1+1), -(4*2+1), -(4*3+1) ...
= -1, -5, -9, -13, ...
round option adds extra blank pages to make total page count multiple of round
*/
// modulo(int step, int round){page_ranges}
static bool cmd_modulo(PdfDocument &doc, Param params[], PageRanges &pages)
{
    int modulo = params[0].integer;//step
    int round = params[1].integer;

    round = MAX(round, modulo);
    // add blank pages to make page_count integral multiple of 'round'
    while (doc.page_list.count()%round){
        doc.newBlankPage(-1);
    }
    PageRanges new_ranges;

    int pages_count = doc.page_list.count();

    for (int i=0; i<pages_count; i+=modulo){
        for (auto range : pages.array)
        {
            if (range.begin==-1){// -1 = last page ($)
                range.begin = modulo;
            }
            if (range.end==-1){
                range.end = modulo;
            }
            // if range.negative is true, page will be order in reverse direction
            PageRange new_range(i+range.begin, i+range.end, range.negative);

            new_ranges.append(new_range);
        }
    }
    new_ranges.initPageNums(doc.page_list.count());
    return doc_pages_arrange(doc, new_ranges);
}

static bool cmd_nup (PdfDocument &doc, Param params[], PageRanges &pages)
{
	int n, rows, cols, row, col;
	double dx, dy, margin_x, margin_y;
    float scale, scale_x, scale_y, scaled_page_w, scaled_page_h,
            cell_x, cell_y, move_x, move_y;
	Rect paper;// size of new paper

	n = params[0].integer;
	cols = params[1].integer;
    rows = n/cols + (n%cols ? 1 : 0);
	dx = params[2].real;
	dy = params[3].real;
    margin_x = dx;
    margin_y = dy;
	const char *paper_format = params[4].str;
    // get paper size and orientation
    Orientation orientation = orientation_from_str(params[5].str);
    if (not set_paper_from_name(paper, paper_format, ORIENT_AUTO)) {
        message(WARN, "%s is unknown paper format", paper_format);
        return false;
	}
    // calc paper orientation
    if (orientation == ORIENT_AUTO) {
        if (cols>rows || (cols==rows && doc.page_list[0].paper.isLandscape())) {
            paper_set_orientation(paper, ORIENT_LANDSCAPE);
        }
        else paper_set_orientation(paper, ORIENT_PORTRAIT);
    }
    // add required blank pages
    while (doc.page_list.count() % n){
        doc.newBlankPage(-1);
    }
    int pages_count = doc.page_list.count();
    // calculate available space for fitting each page
    float cell_w = ( paper.right.x - (2*margin_x) - (cols-1)*dx )/cols;
    float cell_h = ( paper.right.y - (2*margin_y) - (rows-1)*dy )/rows;
    // scale and move page to proper position
    for (int page_index=0; page_index < pages_count; page_index++) {
        PdfPage &page = doc.page_list[page_index];
        Rect page_size = page.pageSize();
        scale_x = cell_w/(page_size.right.x - page_size.left.x);
        scale_y = cell_h/(page_size.right.y - page_size.left.y);
        scale = MIN(scale_x, scale_y);

        scaled_page_w = scale * (page_size.right.x - page_size.left.x);
        scaled_page_h = scale * (page_size.right.y - page_size.left.y);
        // move page
        // pages are ordered from left to right and top to bottom
        row = (page_index % n) / cols;
        col = (page_index % n) % cols;
        cell_x = margin_x + (cell_w+dx)*col;// highest col is moved most
        cell_y = margin_y + (cell_h+dy)*(rows-1-row);// smallest row is moved most
        // further moved, to align center, and compensate non zero bottom left
        move_x = cell_x + (cell_w-scaled_page_w)/2 - scale*page_size.left.x;
        move_y = cell_y + (cell_h-scaled_page_h)/2 - scale*page_size.left.y;
        Matrix  matrix;
        matrix.scale(scale);
        matrix.translate(move_x, move_y);
        page.transform(matrix);
    }
    // create new blank pages and merge old pages
    int loops = pages_count / n;
    for (int i=0; i<loops; i++) {
        doc.newBlankPage(-1);
        PdfPage &new_page = doc.page_list[doc.page_list.count()-1];//last page
        new_page.paper = paper;
        new_page.bbox = paper;
        for (int j=0; j<n; j++) {
            PdfPage &page = doc.page_list[0];
            new_page.mergePage(page);
            doc.page_list.remove(0);
        }
    }
    return true;
}

// centerfold booklet format, (use along with nup)
static bool cmd_book (PdfDocument &doc, Param params[], PageRanges &pages)
{
    while (doc.page_list.count()%4){
        doc.newBlankPage(-1);
    }
    int pages_count = doc.page_list.count();
    PageRanges new_ranges;

    for (int i=0; i<pages_count/2; i+=2){
        new_ranges.page_num_array.push_back(pages_count-i);
        new_ranges.page_num_array.push_back(i+1);
        new_ranges.page_num_array.push_back(i+2);
        new_ranges.page_num_array.push_back(pages_count-i-1);
    }
    return doc_pages_arrange(doc, new_ranges);
}

// make outside of this area blank white
static bool cmd_crop (PdfDocument &doc, Param params[], PageRanges &pages)
{
    Orientation orientation = orientation_from_str(params[1].str);
	Rect paper;
	if (not set_paper_from_name(paper, params[0].str, orientation)){
		message(ERROR, "'%s' is unknown paper size", params[0].str);
        return false;
	}
	return doc_pages_crop(doc, pages, paper);
}

static bool cmd_crop2 (PdfDocument &doc, Param params[], PageRanges &pages)
{
    Rect area;
    area.left = Point(params[0].real, params[1].real);
    area.right = Point(params[2].real, params[3].real);
    return doc_pages_crop(doc, pages, area);
}

static bool cmd_matrix (PdfDocument &doc, Param params[], PageRanges &pages)
{
    Matrix matrix(params[0].real, params[1].real, 0,
                  params[2].real, params[3].real, 0,
                  params[4].real, params[5].real, 1);
	return doc_pages_transform(doc, pages, matrix);
}

static bool cmd_scale (PdfDocument &doc, Param params[], PageRanges &pages)
{
    Matrix scale_matrix;
	scale_matrix.scale(params[0].real);
	doc_pages_transform(doc, pages, scale_matrix);
	return true;
}

// scaleto (str paper, float margin_top, margin_right, margin_bottom, margin_left)
static bool cmd_scaleto (PdfDocument &doc, Param params[], PageRanges &pages)
{
    Orientation orientation = orientation_from_str(params[5].str);
	Rect paper;
	if (not set_paper_from_name(paper, params[0].str, orientation)){
		message(ERROR, "'%s' is unknown paper size", params[0].str);
        return false;
	}
	doc_pages_scaleto(doc, pages, paper, params[1].real, params[2].real, params[3].real, params[4].real);
	return true;
}

// scaleto2(width, height, margin_top, right, bottom, left)
static bool cmd_scaleto2 (PdfDocument &doc, Param params[], PageRanges &pages)
{
	Rect paper;
    paper.right = Point(params[0].real, params[1].real);
	return doc_pages_scaleto(doc, pages, paper, params[2].real, params[3].real, params[4].real, params[5].real);
}

static bool cmd_rotate (PdfDocument &doc, Param params[], PageRanges &pages)
{
    int w, h, angle = params[0].integer;
    if (angle%90 != 0) {
        message(ERROR, "rotation angle must be multiple of 90");
        return false;
    }
	angle %= 360;

    Matrix rot_matrix;
    rot_matrix.rotate(angle);

	for (int page_num : pages){
        PdfPage &page = doc.page_list[page_num-1];
        Rect page_size = page.pageSize();
	    Matrix matrix = rot_matrix;
	    w = page_size.right.x;
	    h = page_size.right.y;
	    switch (angle){
		    case 90:
			    matrix.translate(0, w);
			    break;
		    case 180:
			    matrix.translate(w, h);
			    break;
		    case 270:
			    matrix.translate(h, 0);
			    break;
	    }
        page.transform(matrix);
	}
    return true;
}

// flip(id mode)
static bool cmd_flip (PdfDocument &doc, Param params[], PageRanges &pages)
{
	int mode = str_to_id(params[0].str, ids_orient, get_ids_len(ids_orient));

	for (int page_num : pages){
        PdfPage &page = doc.page_list[page_num-1];
        Rect page_size = page.pageSize();
	    Matrix matrix;
        switch (mode) {
            case ORIENT_LANDSCAPE:// rotate 180 deg along x axis (vertically)
                matrix.mat[1][1] = -1;
                matrix.mat[2][1] = page_size.right.y;
                break;
            case ORIENT_PORTRAIT:// rotate 180 deg along y axis (horizontally)
                matrix.mat[0][0] = -1;
                matrix.mat[2][0] = page_size.right.x;
                break;
            default:
                message(ERROR, "invalid flip mode, use v or h");
                return false;
        }
        page.transform(matrix);
	}
    return true;
}

// move(float x, float y)
static bool cmd_move (PdfDocument &doc, Param params[], PageRanges &pages)
{
    return doc_pages_translate(doc, pages, params[0].real, params[1].real);
}

// number(x, y, start, text, size, font)
static bool cmd_number (PdfDocument &doc, Param params[], PageRanges &pages)
{
    return doc_pages_number(doc, pages, params[0].real, params[1].real,
            params[2].integer, params[3].str, params[4].integer, params[5].str);
}
// text(x, y, text, size, font)
static bool cmd_text (PdfDocument &doc, Param params[], PageRanges &pages)
{
    return doc_pages_text(doc, pages, params[0].real, params[1].real,
            params[2].str, params[3].integer, params[4].str);
}

static bool cmd_line (PdfDocument &doc, Param params[], PageRanges &pages)
{
	for (int page_num : pages){
        PdfPage &page = doc.page_list[page_num-1];
        page.drawLine(Point(params[0].real, params[1].real),
                      Point(params[2].real, params[3].real), params[4].real);
    }
    return true;
}

static bool cmd_paper (PdfDocument &doc, Param params[], PageRanges &pages)
{
    Orientation orientation = orientation_from_str(params[1].str);
	Rect paper;
	if (not set_paper_from_name(paper, params[0].str, orientation)){
		message(ERROR, "'%s' is unknown paper size", params[0].str);
        return false;
	}
	return doc_pages_set_paper_size(doc, pages, paper);
}

static bool cmd_paper2 (PdfDocument &doc, Param params[], PageRanges &pages)
{
	Rect paper;
    paper.right = Point(params[0].real, params[1].real);
	return doc_pages_set_paper_size(doc, pages, paper);
}

static bool cmd_spaper (PdfDocument &doc, Param params[], PageRanges &pages)
{
	return add_new_paper_size(params[0].str, params[1].real, params[2].real);
}

static bool cmd_pinfo (PdfDocument &doc, Param params[], PageRanges &pages)
{
	for (int page_num : pages){
        PdfPage &page = doc.page_list[page_num-1];
        const char *bbox_type = page.bbox_is_cropbox ? "CropBox" : "TrimBox";
        printf("%d\n", page_num);
        printf("    Paper %g %g %g %g\n", page.paper.left.x, page.paper.left.y, page.paper.right.x, page.paper.right.y);
        printf("    %s %g %g %g %g\n", bbox_type, page.bbox.left.x, page.bbox.left.y, page.bbox.right.x, page.bbox.right.y);
    }
    return true;
}

void print_cmd_info(FILE *f)
{
	for (int i=0; cmd_commands[i].help!=NULL; ++i)
    {
        // print command name
		fprintf(f, "%s", cmd_commands[i].name);
        // print command args within bracket
		if (cmd_commands[i].params_count) {
            fprintf(f,"(");
			for (int j=0; j<cmd_commands[i].params_count || (fprintf(f,")"),0); ++j){
                //print param name and the = sign
				fprintf(f, "%s=", cmd_commands[i].params[j].name);
				switch (cmd_commands[i].params[j].type){
					case CMD_TOK_INT:
						if (cmd_commands[i].params[j].val_type==CMD_TOK_INT){
							fprintf(f, "%ld", cmd_commands[i].params[j].integer);
						}
						else {
							fprintf(f, "<int>");
						}
						break;
					case CMD_TOK_REAL:
						if (cmd_commands[i].params[j].val_type==CMD_TOK_REAL){
							fprintf(f, "%.1f", cmd_commands[i].params[j].real);
						}
						else {
							fprintf(f,"<real>");
						}
						break;
					case CMD_TOK_ID:
						if (cmd_commands[i].params[j].val_type==CMD_TOK_ID){
							if (cmd_commands[i].params[j].str){
								fprintf(f, "%s", cmd_commands[i].params[j].str);
							}
							else{
								fprintf(f, "auto");
							}
						}
						else {
							fprintf(f, "<id>");
						}
						break;
					case CMD_TOK_STR:
						if (cmd_commands[i].params[j].val_type==CMD_TOK_STR){
							if (cmd_commands[i].params[j].str){
								fprintf(f, "\"%s\"", cmd_commands[i].params[j].str);
							}
							else {
								fprintf(f, "\"auto\"");
							}
						}
						else{
							fprintf(f, "<str>");
						}
						break;
					case CMD_TOK_MEASURE:
						if (cmd_commands[i].params[j].val_type==CMD_TOK_MEASURE){
							fprintf(f, "%g pt", cmd_commands[i].params[j].real);
						}
						else {
							fprintf(f, "<measure>");
						}
						break;
					default:
						assert(0);
						break;
				}
                if (j < cmd_commands[i].params_count-1 )
                    fprintf(f, ",");// put a comma before next arg
			}
		}
		/*if (cmd_commands[i].pages){
			fprintf(f,"{page_ranges}");
		}*/
		fprintf(f, "\n                  %s\n", (cmd_commands[i].help) && strlen(cmd_commands[i].help)  ? (cmd_commands[i].help) : ("missing"));
	}
}

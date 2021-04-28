/*
    pdfcook : A prepress preparation tool for PDF files
    Copyright (C) 2021  Arindam Chaudhuri <ksharindam@gmail.com>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, see <http://www.gnu.org/licenses/>.
*/
#include "common.h"
#include "debug.h"
#include "pdf_doc.h"
#include "doc_edit.h"
#include "cmd_exec.h"
#include <cstdio>
#include <getopt.h>


char pusage[][LLEN] = {
    "Usage: pdfcook [<options>] [<commands>] <infile> ... <outfile>",
    "  -h   Display this help screen",
    "  -q --quiet   Supress warning and log messages",
    "     --fonts   Show available standard font names",
    "  -p --papers  Show available paper sizes",
    "commands: '<cmd1> <cmd2> ... <cmd_n>'",
    "command: name(arg_1, ... arg_name=arg_value){page_range1 page_range2 ...}",
    "args eg. : <int> 12,  <real> 12.0,  <id> a4,  <str> \"Helvetica\"",
    "           <measure> 612.0 (without unit pt) or 8.5in (with unit mm,cm,in)",
    "list of commands :"
};

static void print_help (FILE * stream, int exit_code)
{
    fprintf(stream, "pdfcook %s\n", PROG_VERSION);
    for (size_t i = 0; i < sizeof(pusage) / LLEN; ++i){
        fprintf(stream, "%s\n", pusage[i]);
    }
    print_cmd_info(stream);
    exit(exit_code);
}
// if an option requires argument, put a colon (:) after it in shortoptions
static const char *short_options = "hqfp";
// here, in 4th column, any integer can be used instead
static struct option long_options[] = {
    {"help", no_argument, 0, 'h'},
    {"quiet", no_argument, 0, 'q'},
    {"fonts", no_argument, 0, 'f'},
    {"papers", no_argument, 0, 'p'},
    {NULL, 0, 0, 0}
};

typedef struct {
    int    infile;
    int    outfile;
    char  *commands;
} Conf;


static void parseargs (int argc, char *argv[], Conf * conf)
{
    conf->infile = -1;
    conf->outfile = -1;
    conf->commands = NULL;
    int next_opt;
    while ((next_opt = getopt_long(argc, argv, short_options, long_options, NULL))!= -1) {

        switch (next_opt) {
        case '?'://unknown option, or option requires argument but not provided
        case 'h':
            print_help(stderr, 1);
            break;
        case 'q':
            quiet_mode = 1;
            break;
        case 'f':
            print_font_names();
            exit(1);
        case 'p':
            print_paper_sizes();
            exit(1);
        }
    }
    // now optind is index of first non-option argument
    switch (argc-optind) {
    case 1:
        conf->infile = optind;
        break;
    case 2:
        conf->infile = optind;
        conf->outfile = optind + 1;
        break;
    case 3:
        conf->commands = argv[optind];
        conf->infile = optind + 1;
        conf->outfile = optind + 2;
        break;
    default:// for more than 3 args, first is command, last is outfile and rest are infiles
        if (argc-optind>3) {
            conf->commands = argv[optind];
            conf->infile = optind + 1;
            conf->outfile = argc - 1;
            break;
        }
        print_help(stderr, 1);//no argument
        break;
    }
}

bool file_exist (const char * name)
{
    FILE *f = fopen(name,"r");
    if (f==NULL) {
        return false;
    }
    fclose(f);
    return true;
}

int main (int argc, char *argv[])
{
    // parse command line arguments
    Conf conf;
    parseargs(argc, argv, &conf);// if no args given, program exits here

    PdfDocument doc;
    // check if it exists and try to open first document
    if (!file_exist(argv[conf.infile])){
        message(FATAL,"File '%s' not found", argv[conf.infile]);
    }
    if (not doc.open( argv[conf.infile] ))
        message(FATAL, "Failed to open file '%s'", argv[conf.infile]);

    // read all other input files (if any) and join them
    for (int i=conf.infile + 1; i<conf.outfile && conf.infile>0; i++){
        if (!file_exist(argv[i])){
            message(FATAL,"File '%s' not found", argv[i]);
        }
        PdfDocument new_doc;
        if (not new_doc.open( argv[i] ))
            message(FATAL,"Failed to open file '%s'", argv[i]);
        doc.mergeDocument(new_doc);
    }
    // build command tree
    CmdList cmd_list;
    MYFILE *commands = stropen(conf.commands);
    if (commands != NULL) {
        parse_commands(cmd_list, commands);
        myfclose(commands);
        // execute command tree
        doc_exec_commands(doc, cmd_list);
    }

    if (conf.outfile != -1){
        if (not doc.save( argv[conf.outfile] ))
            return -1;
    }
    return 0;
}



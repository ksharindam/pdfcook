/* This file is a part of pdfcook program, which is GNU GPLv2 licensed */
#pragma once
#include "common.h"
#include "fileio.h"
#include "pdf_doc.h"
#include <list>


class Command;

typedef std::list<Command*> CmdList;

void parse_commands(CmdList &cmd_list, MYFILE *f);
void doc_exec_commands(PdfDocument &doc, CmdList &cmd_list);

void print_cmd_info(FILE *f);

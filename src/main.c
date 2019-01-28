#include "common.h"
#ifdef HAVE_GETOPT_LONG
#include <getopt.h>
#endif
#include <unistd.h>
#include <signal.h>
#include "fileio.h"
#include "vdoc.h"
#include "cmdexec.h"

#define STD_IN_OUT 		1

#ifdef ENABLE_RC
	#define PSPDFTOOL_RC		".pspdftoolrc"
	#define PSPDFTOOL_RC_ETC	"/etc/pspdftoolrc"
#endif

struct conf {
	int           infile;
	int           outfile;
	char           *commands;
	char	       *f_cmd;
};
enum {
	c_cmdexec,c_cmd_info
};

char            pusage[][LLEN] = {
	"Usage: pspdftool [<options>] -f<command>|<commands> <infile> ... <outfile>",
	"  -h --help	Display this help screen",
	"  -f --file    <command file>",
/*	"  -q --quiet   Supress screen output",
	"  -v --verbose ",*/
	"commands: \"<cmd1> <cmd2> ... <cmd_n>\"",
	"command: name(arg_1, ... arg_name=arg_value){page_begin..page_end commands ... -page_begin..page_end}",
	"list of commands:"
};

/* retezec obsahujici kratke volby */
static const char     *short_options = "qhf:v";
#ifdef HAVE_GETOPT_LONG
/* Pole struktur s dlouhymi volbami */
static const struct option long_options[] = {
	{"quiet", 0, NULL, 'q'},
	{"verbose", 0, NULL, 'v'},
	{"file", 0, NULL, 'f'},
	{"help", 0, NULL, 'h'},
	{NULL, 0, NULL, 0}
};
#endif

/* vypise parametry programu */
static void 
print_help(FILE * stream, int exit_code)
{
	size_t          i;
	fprintf(stream, "pspdftool %s\n", PACKAGE_VERSION);
	for (i = 0; i < sizeof(pusage) / LLEN; ++i){
		fprintf(stream, "%s\n", pusage[i]);
	}
	cmd_print_info(stream);
	exit(exit_code);

}/* END print_help() */


void sig_handler(int sig){
         printf("Memory violation.\n");
         exit( -100);
}
			 


/* nacte vstupni parametry programu */
static void 
parseargs(int argc, char *argv[], struct conf * conf)
{
	int             next_option;
	conf->commands = NULL;
	conf->infile = -1;
	conf->outfile =-1;
	conf->f_cmd=NULL;
	conf->commands=NULL;
	do {
#ifdef HAVE_GETOPT_LONG
		next_option = getopt_long(argc, argv, short_options, long_options, NULL);
#else
		next_option = getopt(argc,argv,short_options);
#endif

		switch (next_option) {
		case 'u':
			print_help(stdout, 0);
			break;
		case 'h':
		case '?':
			print_help(stderr, 1);
			break;
		case 'v':
			break;
		case 'f':
			
			conf->f_cmd = optarg;
			break;
		}

	} while (next_option != -1);
	
	switch(argc-optind){
	case 3:
		conf->commands = argv[optind];
		conf->infile = optind + 1;
		conf->outfile = optind + 2;
		break;
	case 2:
		if (conf->f_cmd != NULL){
			conf->infile = optind;
			conf->outfile = optind + 1;
		}
		else{
#ifdef STD_IN_OUT
			conf->infile = -2;
#endif
			conf->commands = argv[optind];
			conf->outfile = optind + 1;
		}
		break;
	case 1:
		if (conf->f_cmd != NULL){
			conf->infile = optind;
		}
		else{
			conf->commands = argv[optind];

#ifdef STD_IN_OUT
			conf->infile = -2;
			conf->outfile = -2;
#endif
		}
		break;
	case 0:
		if (conf->f_cmd != NULL){
#ifdef STD_IN_OUT
			conf->infile = -2;
			conf->outfile = -2;
#endif
			break;
		}
	default:
		if (argc-optind>3){
			if (conf->f_cmd == NULL){
				conf->commands=argv[optind];
				++optind;
			}
			conf->infile = optind;
			conf->outfile = argc - 1;
			break;
		}
		print_help(stderr, 1);
		break;
	}
}				/* END parseargs() */

int file_exist(const char * name){
	FILE * f;
	f = fopen(name,"r");
	if (f==NULL){
		return 0;
	}
	fclose(f);
	return 1;
}

int main(int argc, char *argv[]){
	struct conf conf;
	cmd_ent_struct_head cmd_tree;
	MYFILE * commands;
#ifdef ENABLE_RC
	MYFILE * commands_rc = NULL;
	MYFILE * commands_rc_etc = NULL;
	char path[PATH_MAX];
	cmd_ent_struct_head cmd_tree_rc;
	cmd_ent_struct_head cmd_tree_rc_etc;
#endif
	page_list_head * p_doc=NULL;
	char filein[]=".tmp.XXXXXXXX";
	int result;
	int i;
	signal(SIGBUS,sig_handler);
	atexit(doc_free_format);
	parseargs(argc, argv, &conf);

	if (conf.f_cmd){
		commands = myfopen(conf.f_cmd,"rt");
	}
	else{
		commands = stropen(conf.commands);
	}
#ifdef ENABLE_RC
	snprintf(path, PATH_MAX, "%s/%s", getenv("HOME"), PSPDFTOOL_RC);
	if (file_exist(path)) {
		commands_rc = myfopen(path, "rt");
	}
	if (file_exist(PSPDFTOOL_RC_ETC)) {
		commands_rc_etc = myfopen(PSPDFTOOL_RC_ETC, "rt");
	}

	if (commands_rc_etc != NULL) {
		if (cmd_preexec(&cmd_tree_rc_etc, commands_rc_etc) == -1) {
			return -1;
		}
	}

	if (commands_rc != NULL) {
		if (cmd_preexec(&cmd_tree_rc, commands_rc) == -1) {
			return -1;
		}
	}
#endif
	if (commands != NULL) {
		if (cmd_preexec(&cmd_tree, commands) == -1) {
			return -1;
		}
	}

	if (conf.infile != -1){
		if (conf.infile==-2 || (strcmp(argv[conf.infile],"-")==0)){
			char BUFFER[1024];
			FILE * f;
			int fd;
			int i;
			fd=mkstemp(filein);
			f = fdopen(fd,"wb");
			if (f==NULL){
				return -1;
			}
			do {
				i=fread(BUFFER,sizeof(char),1024,stdin);
				i=fwrite(BUFFER,sizeof(char),i,f);
			}while (i==1024);
			fclose(f);
		}else{
			if (!file_exist(argv[conf.infile])){
				message(FATAL,"File \"%s\" not found.\n",argv[conf.infile]);
			}
		}
	}



	if (conf.infile!=-1){
		p_doc=pages_list_open((conf.infile==-2 || (argv[conf.infile][0]=='-'))?filein:argv[conf.infile]);
	}
	else{
		p_doc=pages_list_open(NULL);
	}
	if (p_doc==NULL){
		message(FATAL,"File \"%s\" is not PS, PDF format.\n",(conf.infile>0)?argv[conf.infile]:"stdin");
		/*soubor se nepodarilo nacist*/
		return 1;
	}

	for(i=conf.infile + 1;conf.infile>0 && i<conf.outfile;++i){
		int retval;
		page_list_head * p_doc_new;
		if (!file_exist(argv[conf.infile])){
			message(FATAL,"File \"%s\" not found.\n",argv[i]);
		}
		p_doc_new=pages_list_open(argv[i]);
		if (p_doc_new==NULL){
			message(FATAL,"File \"%s\" is not PS, PDF format.\n",argv[i]);
			return 1;
		}
		retval=pages_list_cat(p_doc,p_doc_new);
		if (retval!=0){
			pages_list_delete(p_doc_new);
			message(FATAL,"I cannot cat this file \"%s\".\n",argv[i]);
		}
	}

	if (commands==NULL){
		pages_list_delete(p_doc);
		return -1;
	}
#ifdef ENABLE_RC
	if (commands_rc_etc != NULL) {
		result=cmd_exec(p_doc, &cmd_tree_rc_etc, commands_rc_etc);
		myfclose(commands_rc_etc);
		if (result == -1){
		/**error*/
			pages_list_delete(p_doc);
		return -1;
		}
	}

	if (commands_rc != NULL) {
		result=cmd_exec(p_doc, &cmd_tree_rc, commands_rc);
		myfclose(commands_rc);
		if (result == -1){
		/**error*/
			pages_list_delete(p_doc);
		return -1;
		}
	}
#endif

	result=cmd_exec(p_doc, &cmd_tree, commands);
	myfclose(commands);
	if (result == -1){
		/**error*/
		pages_list_delete(p_doc);
		return -1;
	}
	if (conf.outfile != -1){
		char * _name;
		if (conf.outfile==-2) {
			_name = "-";
		}
		else {
			_name = argv[conf.outfile];
		}
		if (pages_list_save(p_doc,_name)==-1){
			printf("File wasn't saved ...\n");
		}
	}
	pages_list_delete(p_doc);
	if ((conf.infile!=-1) && (conf.infile==-2 ||  strcmp(argv[conf.infile],"-")==0)){
		remove(filein);
	}
	return 0;
}				/* END main() */

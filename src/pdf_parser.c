#include "common.h"
#include "fileio.h"
#include "pdf_parser.h"

typedef struct mystring{
	char *str;
	size_t size;
	size_t cpoz;
}mystring;

static int mystring_new(mystring * s){
	s->size=10;
	s->str=(char *) malloc(sizeof(char) * s->size);
	s->str[0]=0;
	s->cpoz=1;
	if (s->str==NULL){
		s->size=0;
		return -1;
	}
	return 0;
}

static int mystring_add_char(mystring * s,char c){
	char * pom;
	if (s->size==s->cpoz){
		s->size=s->size * 2;
		pom=(char *) realloc(s->str,sizeof(char) * s->size);
		if (pom==NULL){
			s->size=s->size / 2;
			return -1;
		}
		s->str=pom;
	}
	s->str[s->cpoz-1]=c;
	s->str[s->cpoz]=0;
	++(s->cpoz);
	return 0;
	
}
long pdf_get_obj_poz(pdf_object_table * xref, pdf_object * p_obj){
	if ( xref==NULL 
		|| p_obj==NULL 
		|| p_obj->type!=PDF_OBJ_INDIRECT_REF)
	{
		return -1;
	}
	if (xref->count<=p_obj->val.reference.major 
	    || xref->table[p_obj->val.reference.major].minor!=p_obj->val.reference.minor 
	    || xref->table[p_obj->val.reference.major].used!='n')
	{
		return -1;
	}
	return xref->table[p_obj->val.reference.major].offset;
}


int pdf_get_object_from_str(pdf_object * obj, char * str){
	MYFILE * f;
	int retval;
	f=stropen(str);
	if (f==NULL){
		return -1;
	}
	retval=pdf_get_object(f,obj,NULL,NULL);
	myfclose(f);
	return retval;
}

int pdf_get_object(MYFILE * f, pdf_object * p_obj, pdf_object_table * xref, pdf_tok * last_tok){
	pdf_tok tok;
	long stream_len=0;
	pdf_object t_obj;
	pdf_object * pom_obj;
	pdf_array * pom_array;
	pdf_dict * pom_dict;
	long fpos;
	if (last_tok==NULL){
		last_tok=&tok;
	}
	p_obj->type=PDF_OBJ_UNKNOWN;
	while (pdf_get_tok(f,last_tok)!=-1){
		switch (last_tok->type){
		case PDF_T_INT:
			p_obj->type=PDF_OBJ_INT;
			p_obj->val.int_number=last_tok->int_number;
			if (last_tok->sign){
				return 0;
			}
			fpos=myftell(f);
			pdf_get_tok(f,last_tok);
			if (last_tok->type!=PDF_T_INT || last_tok->sign){
				pdf_free_tok(last_tok);
				if (myfseek(f,fpos,SEEK_SET)==EOF){
					message(FATAL,"myfseek()  error in file %s at line %d.\n", __FILE__, __LINE__);
					return -1;
				}
				return 0;
			}
			p_obj->val.reference.major=p_obj->val.int_number;
			p_obj->val.reference.minor=last_tok->int_number;
			
			pdf_get_tok(f,last_tok);
			if (last_tok->type!=PDF_T_ID){
				pdf_free_tok(last_tok);
				if (myfseek(f,fpos,SEEK_SET)==EOF){
					message(FATAL,"myfseek()  error at in file %s line  %d.\n",__FILE__, __LINE__);
					return -1;
				}
				return 0;
			}
			if (strcmp(last_tok->id,"obj")==0){
				/*indircet object*/
				p_obj->type=PDF_OBJ_INDIRECT;
				p_obj->val.reference.obj=pdf_new_object();
#if 0
				printf("%lu\n",myftell(f));
				fflush(f->f);
#endif
				if (pdf_get_object(f,p_obj->val.reference.obj,xref,last_tok)==-1){
					message(FATAL, "pdf_read_object() error in file %s  at line  %d.\n",__FILE__,__LINE__);
					return -1;
				}
				pdf_get_tok(f,last_tok);
				if (last_tok->type!=PDF_T_ID || strcmp(last_tok->id,"endobj")){
					return -1;
				}
				return 0;
			}
			
			if (strcmp(last_tok->id,"R")==0){
				/*indircet object reference*/
				p_obj->type=PDF_OBJ_INDIRECT_REF;
				return 0;
			}
			/*two int numbers*/
			if (myfseek(f,fpos,SEEK_SET)==EOF){
				message(FATAL,"myfseek()  error in file %s at line %d.\n",__FILE__,__LINE__);
				return -1;
			}
			return 0;
		case PDF_T_REAL:
			p_obj->type=PDF_OBJ_REAL;
			p_obj->val.real_number=last_tok->real_number;
			return 0;
		case PDF_T_NAME:
			p_obj->type=PDF_OBJ_NAME;
			p_obj->val.name=(char *)malloc(sizeof(char)* PDF_NAME_MAX_LEN);
			assert(p_obj->val.name!=NULL);
			strncpy(p_obj->val.name,last_tok->name,PDF_NAME_MAX_LEN);
			return 0;
		case PDF_T_STR:
			p_obj->type=PDF_OBJ_STR;
			p_obj->val.str.type=last_tok->str.type;
			p_obj->val.str.str=last_tok->str.str;
			return 0;
		case PDF_T_BDICT:
			p_obj->val.dict.next=p_obj->val.dict.prev=(pdf_dict*)&(p_obj->val.dict);
			p_obj->type=PDF_OBJ_DICT;
			while (pdf_get_object(f,&t_obj,xref,last_tok)==0 && isName(&t_obj)){
				pom_dict=(pdf_dict *) malloc(sizeof(pdf_dict));
				if (pom_dict==NULL){
					return -1;
				}
				pom_dict->name=t_obj.val.name;
				pom_obj=pdf_new_object();
				if (pdf_get_object(f,pom_obj,xref,last_tok)==-1){
					free(pom_dict);
					free(pom_obj);
					message(FATAL,"in file %s at line %d pdf_get_object() fail\n",__FILE__,__LINE__);
					return -1;
				}
				pom_dict->obj=pom_obj;
				if (strncmp(pom_dict->name,"Length",PDF_NAME_MAX_LEN)==0){
					p_obj->type=PDF_OBJ_STREAM;
					switch (pom_obj->type){
						case PDF_OBJ_INT:
							stream_len=pom_obj->val.int_number;
							break;
						case PDF_OBJ_INDIRECT_REF:
							{	
							pdf_object plobj;
							long pos;
							pos=myftell(f);
							if (myfseek(f,pdf_get_obj_poz(xref, pom_obj),SEEK_SET)){
								message(FATAL,"myfseek()  error\n");
								return -1;
							}
							if (pdf_get_object(f,&plobj,xref,NULL)==-1){
								message(FATAL,"in file %s at line %d pdf_get_object() fail\n",__FILE__,__LINE__);
								return -1;
							}
							if (plobj.type!=PDF_OBJ_INDIRECT 
							    || plobj.val.reference.major!=pom_obj->val.reference.major
							    || plobj.val.reference.minor!=pom_obj->val.reference.minor
							    || plobj.val.reference.obj->type!=PDF_OBJ_INT){
								message(FATAL, "in file %s at line %d, length can be only int or indirctt object.\n",__FILE__,__LINE__);
								return -1;
							}
							if (myfseek(f,pos,SEEK_SET)){
								message(FATAL,"myfseek()  error\n");
								return -1;
							}
							stream_len=plobj.val.reference.obj->val.int_number;
							pdf_delete_object(&plobj);
						}
							break;
						default:
							return -1;
					}
				}
				__list_add((pdf_dict*)&p_obj->val.dict,pom_dict);
			}
			if (last_tok->type!=PDF_T_EDICT){
				return -1;
			}
			if (p_obj->type==PDF_OBJ_STREAM){
				pom_obj=pdf_new_object();
				pom_obj->type=PDF_OBJ_DICT;
				pom_obj->val.dict.next=p_obj->val.dict.next;
				pom_obj->val.dict.prev=p_obj->val.dict.prev;
				pom_obj->val.dict.next->prev=(pdf_dict*)&(pom_obj->val.dict);
				pom_obj->val.dict.prev->next=(pdf_dict*)&(pom_obj->val.dict);
				p_obj->val.stream.dict=pom_obj;
				p_obj->val.stream.stream=NULL;
				if (pdf_get_tok(f,last_tok)!=0
					|| last_tok->type!=PDF_T_ID
					|| strcmp(last_tok->id,"stream")!=0){
					return -1;
				}
				if (stream_len==1){
					stream_len=0;
				}
				switch(mygetc(f)){
					case EOF:
						return -1;
					case char_cr:
						if (mygetc(f)!=char_lf){
							stream_len-=2;
							stream_len=stream_len<0?0:stream_len;
							
						}
					case char_lf:
						break;
					default:
						myungetc(f);
						break;
				}	
				p_obj->val.stream.begin=myftell(f);
				p_obj->val.stream.len=stream_len;
				if (stream_len){
					p_obj->val.stream.stream=(char *)malloc(sizeof(char) * stream_len);
					if (p_obj->val.stream.stream==NULL){
						message(FATAL,"maloc() error\n");
					}
					if (myfread(p_obj->val.stream.stream,sizeof(char),stream_len,f)!=stream_len){
						message(FATAL,"fread() error\n");
					}
				}
				else {
					p_obj->val.stream.stream=(char *)malloc(sizeof(char) * 1);
					if (p_obj->val.stream.stream==NULL){
						message(FATAL,"maloc() error\n");
					}
				}
				if (pdf_get_tok(f,last_tok)!=0
					|| last_tok->type!=PDF_T_ID
					|| strcmp(last_tok->id,"endstream")!=0){
					return -1;
				}
				
			}
			return 0;
		case PDF_T_BARRAY:
			p_obj->val.array.next=p_obj->val.array.prev=(pdf_array*)&(p_obj->val.array);
			p_obj->type=PDF_OBJ_ARRAY;
			pom_obj=pdf_new_object();
			while (pdf_get_object(f,pom_obj,xref,last_tok)==0){
				pom_array=(pdf_array *)malloc(sizeof(pdf_array));
				if (pom_array==NULL){
					return -1;
				}
				pom_array->obj=pom_obj;
				__list_add((pdf_array*)&(p_obj->val.array),pom_array);
				/*add pom to array*/
				pom_obj=pdf_new_object();
			}
			free(pom_obj);
			if (last_tok->type!=PDF_T_EARRAY){
				return -1;
			}
			return 0;
		case PDF_T_ID:
			if (strcmp(last_tok->id,"null")==0){
				p_obj->type=PDF_OBJ_NULL;
				return 0;
			}
			if (strcmp(last_tok->id,"true")==0){
				p_obj->type=PDF_OBJ_BOOL;
				p_obj->val.boolean=1;
				return 0;
			}
			if (strcmp(last_tok->id,"false")==0){
				p_obj->type=PDF_OBJ_BOOL;
				p_obj->val.boolean=0;
				return 0;
			}
			message(FATAL,"unknown id \"%s\"\n",last_tok->id);
			/*identifikator*/
		case PDF_T_EOLN:
		case PDF_T_BOOL:
		case PDF_T_NULL:
			assert(0);
			return 0;
		case PDF_T_EOF:
		case PDF_T_EARRAY:
		case PDF_T_EDICT:
		case PDF_T_UNKNOWN:
		default:
			return -1;
		}
	}
	return -1;
}

int pdf_free_tok(pdf_tok * p_tok){
	switch(p_tok->type){
		case PDF_T_STR:
			free(p_tok->str.str);
			break;
		default:
			break;
	}
	return 0;
}

int pdf_get_tok(MYFILE * f, pdf_tok * p_tok){
	int c, minus=0, parenthes;
	mystring mstr={NULL,0,0};
	size_t i;
	pdf_int_number number;
	pdf_real_number real_number, frac;
	int  new_line=0;
	while (1){
		c=mygetc(f);
		switch(c){
			case EOF:
				p_tok->type=PDF_T_EOF;
				return 0;
			/*bily znak*/
			case char_ff:
			case char_sp:
			case char_tab:
				new_line=0;
				break;
			/*konec radky*/
			case char_lf:
			case char_cr:
				new_line=1;
				break;
			default:
				goto whend;
		}
	}
whend:
	p_tok->new_line=new_line;
	switch(c){
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
		case '-':
		case '+':/*number*/
		case '.':
			if (c!='+' && c!='-'){
				number=c-'0';
				p_tok->sign=0;
			}
			else{
				number=0;
				switch(c){
					case '+':
						p_tok->sign=1;
						break;
					case '-':
						minus=-1;
						p_tok->sign=-1;
						break;
				}
			}
			if (c=='.'){
				goto real_num;
			}
			while ((c=mygetc(f))!=EOF && isdigit(c)){
				number=number*10+(c-'0');
			}
			switch(c){
			/*bily znak*/
			case char_ff:
			case char_sp:
			case char_tab:
			/*konec radky*/
			case char_lf:
			case char_cr:
			default:
				myungetc(f);
			case EOF:
				p_tok->type=PDF_T_INT;
				p_tok->int_number=number * ((minus==-1)?-1:1);
				return 0;
			case '.':
				break;
			}
		real_num:
			real_number=number;
			frac=10;
			while ((c=mygetc(f))!=EOF && isdigit(c)){
				real_number=real_number+(c-'0')/frac;
				frac=frac * 10;
			}
			switch(c){
			/*bily znak*/
			case char_ff:
			case char_sp:
			case char_tab:
				new_line=1;
			/*konec radky*/
			case char_lf:
			case char_cr:
			case ']':
			case '>':
			case '/':
				myungetc(f);
			case EOF:
				p_tok->type=PDF_T_REAL;
				p_tok->real_number=real_number * ((minus==-1)?-1:1);
				return 0;
			default:
				p_tok->type=PDF_T_UNKNOWN;
				return -1;

			}
			
			break;
		case '[': /*begin array*/
			p_tok->type=PDF_T_BARRAY;
			return 0;
		case ']': /*end array*/
			p_tok->type=PDF_T_EARRAY;
			return 0;
		case '<': /*hexadecimal string or dictionary*/ 
			if ((c=mygetc(f))=='<'){
					p_tok->type=PDF_T_BDICT;
					return 0;
				}
				else{
					p_tok->str.type=PDF_STR_HEX;
					mystring_new(&mstr);
					/*hexadecimalni string*/
					while (c!=EOF &&c!='>'){
						mystring_add_char(&mstr,c);
						c=mygetc(f);
						
					}
					if (c=='>'){
						p_tok->type=PDF_T_STR;
						p_tok->str.str=mstr.str;
						return 0;
					}
					else{
						p_tok->type=PDF_T_UNKNOWN;
						return -1;
					}
				}

			break;
		case '>': /*end dictionary*/
				if (mygetc(f)=='>'){
					p_tok->type=PDF_T_EDICT;
					return 0;
				}
				else{
					p_tok->type=PDF_T_UNKNOWN;
					myungetc(f);
					return -1;
				}
			break;
		case '(': /*literal string*/
				parenthes=0;
				p_tok->str.type=PDF_STR_CHR;
				mystring_new(&mstr);
				while ((c=mygetc(f))!=EOF){
					switch(c){
						case '\\':
							mystring_add_char(&mstr,c);
							c=mygetc(f);
							
							break;
						case '(':
							++parenthes;
							break;
						case ')':
							if (parenthes==0){
								goto wend383;
							}
							--parenthes;
							break;
					}
					mystring_add_char(&mstr,c);
				}
wend383:
				if (c==EOF){
					p_tok->type=PDF_T_UNKNOWN;
					return -1;
				}
				p_tok->type=PDF_T_STR;
				p_tok->str.str=mstr.str;
				return 0;
			break;
		case '/' /*name object*/:
			i=0;
			while ((c=mygetc(f))!=EOF){
				switch(c){
					case char_ff:
					case char_sp:
					case char_tab:
					case char_lf:
					case char_cr:
					case '<':
					case '>':
					case '{':
					case '}':
					case '/':
					case '%':
					case '(':
					case ')':
					case '[':
					case ']':
						myungetc(f);
						goto out167;
				}
				if (i+1<PDF_NAME_MAX_LEN){
					p_tok->name[i]=c;
					++i;
				}
				else{
					break;
				}
			}
out167:
			p_tok->name[i]=0;
			p_tok->type=PDF_T_NAME;
			return 0;
		case '%':/*comment, skip characters to end of line and call pdf_get_token*/
			while ((c=mygetc(f))!=EOF && c!=char_lf && c!=char_cr)
				;
			if (c==EOF){
				p_tok->type=PDF_T_UNKNOWN;
				return 0;
			}
			else{
				myungetc(f);
				return pdf_get_tok(f,p_tok);
			}
			break;
		default:
			i=0;
			do{
			switch(c){
				case char_ff:
				case char_sp:
				case char_tab:
				case char_lf:
				case char_cr:
				case '<':
				case '>':
				case '{':
				case '}':
				case '/':
				case '%':
				case '(':
				case ')':
				case '[':
				case ']':
					myungetc(f);
					goto out235;
				}
				if (i+1<PDF_ID_MAX_LEN){
					p_tok->id[i]=c;
					++i;
				}
				else{
					break;
				}
			} while ((c=mygetc(f))!=EOF);
out235:
			p_tok->id[i]=0;
			p_tok->type=PDF_T_ID;
			return 0;
	}
	return 0;
}

pdf_object * pdf_add_dict_name_value(pdf_object * p_obj, char *  name){
	pdf_object * pom;
	pdf_dict * pom_dict;
	if (p_obj->type!=PDF_OBJ_DICT){
		return NULL;
	}
	pom=pdf_get_dict_name_value(p_obj,name);
	if (pom){
		return pom;
	}
	pom_dict=(pdf_dict *) malloc(sizeof(pdf_dict));
	if (pom_dict==NULL){
		return NULL;
	}
	pom_dict->name = strdup(name);
	assert(pom_dict->name!=NULL);
	pom_dict->obj=pdf_new_object();
	if (pom_dict->obj==NULL){
		return NULL;
	}
	__list_add((pdf_dict*)&p_obj->val.dict,pom_dict);
	return pom_dict->obj;
}

void pdf_filter_dict(pdf_object * p_obj, char * filter[]){
	pdf_dict * it, * pom;
	char ** ptn;
	if (!isDict(p_obj)){
		return;
	}
	for (it=p_obj->val.dict.next;it!=(pdf_dict *)&(p_obj->val.dict);){
		for (ptn=filter;*ptn!=NULL;++ptn){
			if (strcmp(it->name,*ptn)==0){
				break;
			}
		}
			pom=it;
			it=it->next;
		if (*ptn==NULL){
			__list_elm_remove(pom);
			pdf_delete_object(pom->obj);
			free(pom->obj);
			free(pom->name);
			free(pom);
		}
	}
}

pdf_object * pdf_get_dict_name_value(pdf_object * p_obj, char *  name){
	pdf_dict * pom;
	if (!isDict(p_obj) || name==NULL){
		return NULL;
	}
	for (pom=p_obj->val.dict.next;pom!=(pdf_dict *)&(p_obj->val.dict);pom=pom->next){
		if (strcmp(pom->name,name)==0){
			return pom->obj;
		}
	}
	return NULL;
}

int pdf_write_object (pdf_object * p_obj, FILE * f){
	int ret_val = -1;
	switch(p_obj->type){
		 case PDF_OBJ_BOOL:
			 if (p_obj->val.boolean){
				ret_val = fprintf(f,"true");
			 }
			 else{
				 ret_val = fprintf(f,"false");
			 }
			 return ret_val<0?ret_val:0;
		 case PDF_OBJ_INT:
			 ret_val = fprintf(f,"%d", p_obj->val.int_number);
			 return ret_val<0?ret_val:0;
		 case PDF_OBJ_REAL:
			 ret_val = fprintf(f,"%f", p_obj->val.real_number);
			 return ret_val<0?ret_val:0;
		 case PDF_OBJ_STR:
			 switch (p_obj->val.str.type){
				 case PDF_STR_CHR:
					 ret_val = fprintf(f,"(%s)", p_obj->val.str.str);
					 break;
				 case PDF_STR_HEX:
					ret_val = fprintf(f,"<%s>", p_obj->val.str.str);
					break;
			 }
			 return ret_val<0?ret_val:0;
		 case PDF_OBJ_NAME:
			 ret_val = fprintf(f,"/%s",p_obj->val.name);
			 return ret_val<0?ret_val:0;
		 case PDF_OBJ_ARRAY:
			 ret_val = fprintf(f,"[ ");
			{
			pdf_array *  p_array=p_obj->val.array.next;
			while (p_array!=(pdf_array *)&(p_obj->val.array)){
				ret_val = pdf_write_object(p_array->obj,f);
				p_array=p_array->next;
				if (p_array!=(pdf_array *)&(p_obj->val.array)){
					ret_val = fprintf(f," ");
				}
			}
			}
			 /*vypis jenotlivych prvku*/
			 ret_val = fprintf(f," ]");
			 return ret_val<0?ret_val:0;
		 case PDF_OBJ_DICT:
			 fprintf(f,"<<\n");
			{
			pdf_dict * dict=p_obj->val.dict.next;
			while(dict!=(pdf_dict *) &(p_obj->val.dict)){
				fprintf(f,"/%s ",dict->name);
				pdf_write_object(dict->obj,f);
				dict=dict->next;
				 fprintf(f,"\n");
			}
			}
			 fprintf(f,">>");
			 return 0;
		 case PDF_OBJ_STREAM:
			{
			pdf_object * pom;
			pom=pdf_add_dict_name_value(p_obj->val.stream.dict,"Length");
			pdf_delete_object(pom);
			pom->type=PDF_OBJ_INT;
			pom->val.int_number=p_obj->val.stream.len;
			pdf_write_object(p_obj->val.stream.dict,f);
			fprintf(f,"\nstream\n");

			assert (p_obj->val.stream.stream!=NULL);
			if (p_obj->val.stream.len){
				if (fwrite(p_obj->val.stream.stream,sizeof(char),p_obj->val.stream.len,f)!=p_obj->val.stream.len){
					message(FATAL,"fwrite() error\n");
				}
			}
			fprintf(f,"\nendstream");
			}
			 return 0;
		 case PDF_OBJ_NULL:
			 fprintf(f,"null");
			 return 0;
		 case PDF_OBJ_INDIRECT:
			 fprintf(f,"%d %d obj\n",p_obj->val.reference.major, p_obj->val.reference.minor);
			pdf_write_object(p_obj->val.reference.obj,f);
			fprintf(f,"\nendobj\n");
			 return 0;
		 case PDF_OBJ_INDIRECT_REF:

			fprintf(f,"%d %d R",p_obj->val.reference.major,p_obj->val.reference.minor);
			 return 0;
		default:
			 assert(0);
	}
	return 0;
}


int pdf_count_size_object (pdf_object * p_obj){
	int count;
	switch(p_obj->type){
		 case PDF_OBJ_BOOL:
		 case PDF_OBJ_INT:
		 case PDF_OBJ_REAL:
		 case PDF_OBJ_NULL:
		 case PDF_OBJ_INDIRECT_REF:
		 case PDF_OBJ_UNKNOWN:
			 return sizeof(pdf_object);
		 case PDF_OBJ_STR:
			return sizeof(pdf_object) + strlen(p_obj->val.str.str) + 1; 
	 
		 case PDF_OBJ_NAME:
			return sizeof(pdf_object) + strlen(p_obj->val.name) + 1; 
		 case PDF_OBJ_ARRAY:
			{
				pdf_array *  p_array=p_obj->val.array.next;
				count = sizeof(pdf_object);
				while (p_array!=(pdf_array *)&(p_obj->val.array)){
					p_array=p_array->next;
					count=pdf_count_size_object(p_array->prev->obj);
					count+=sizeof(pdf_object);
					count+=sizeof(pdf_array);
				}
			}
			 return count;
		 case PDF_OBJ_DICT:
			{
				pdf_dict * dict=p_obj->val.dict.next;
				count = sizeof(pdf_object);
				while(dict!=(pdf_dict *) &(p_obj->val.dict)){
					dict=dict->next;
					count += pdf_count_size_object(dict->prev->obj);
					count += sizeof(pdf_dict);
					count += sizeof(pdf_object);
					count +=strlen(dict->prev->name) + 1;
				}
			}
			 return count;
		 case PDF_OBJ_STREAM:
			 count = 2 * sizeof(pdf_object) + pdf_count_size_object (p_obj->val.stream.dict) + p_obj->val.stream.len;
			 return count;
		 case PDF_OBJ_INDIRECT:
			return pdf_count_size_object(p_obj->val.reference.obj) + 2 * sizeof(pdf_object);
		 default:
			 assert(0);
	}
	return 0;
}


int pdf_delete_object (pdf_object * p_obj){
#if 0
	printf("PDO: %p %d\n",p_obj,p_obj->type);
#endif
	switch(p_obj->type){
		 case PDF_OBJ_BOOL:
			 return 0;
		 case PDF_OBJ_INT:
			 return 0;
		 case PDF_OBJ_REAL:
			 return 0;
		 case PDF_OBJ_STR:
			 free(p_obj->val.str.str);
			return 0;	 
		 case PDF_OBJ_NAME:
			 free(p_obj->val.name);
			 return 0;
		 case PDF_OBJ_ARRAY:
			{
				pdf_array *  p_array=p_obj->val.array.next;
				while (p_array!=(pdf_array *)&(p_obj->val.array)){
					p_array=p_array->next;
					pdf_delete_object(p_array->prev->obj);
					free(p_array->prev->obj);
					free(p_array->prev);
				}
			}
			 return 0;
		 case PDF_OBJ_DICT:
			{
				pdf_dict * dict=p_obj->val.dict.next;
				while(dict!=(pdf_dict *) &(p_obj->val.dict)){
					dict=dict->next;
					pdf_delete_object(dict->prev->obj);
					free(dict->prev->obj);
					free(dict->prev->name);
					free(dict->prev);
				}
			}
			 return 0;
		 case PDF_OBJ_STREAM:
			if (p_obj->val.stream.stream!=NULL){
				free(p_obj->val.stream.stream);
			}
			pdf_delete_object(p_obj->val.stream.dict);
			free(p_obj->val.stream.dict);
			return 0;
		 case PDF_OBJ_NULL:
			 return 0;
		 case PDF_OBJ_INDIRECT:
			pdf_delete_object(p_obj->val.reference.obj);
			free(p_obj->val.reference.obj);
			return 0;
		 case PDF_OBJ_INDIRECT_REF:
			 return 0;
		 case PDF_OBJ_UNKNOWN:
			 break;
		 default:
			 assert(0);
	}
	return 0;
}

pdf_object * pdf_new_object(void){
	pdf_object * pom;
	pom=(pdf_object *)malloc(sizeof(pdf_object));
#if 0
	printf("PNO: %p\n",pom);
#endif
	if (pom==NULL){
		message(FATAL,"malloc()\n");
		return NULL;
	}else{
		pom->type=PDF_OBJ_UNKNOWN;
		return pom;
	}
}

int pdf_copy_object (pdf_object * new_obj, pdf_object * old_obj){
	 new_obj->type=old_obj->type;
	switch(old_obj->type){
		 case PDF_OBJ_BOOL:
			 new_obj->val.boolean=old_obj->val.boolean;
			 return 0;
		 case PDF_OBJ_INT:
			 new_obj->val.int_number=old_obj->val.int_number;
			 return 0;
		 case PDF_OBJ_REAL:
			 new_obj->val.real_number=old_obj->val.real_number;
			 return 0;
		 case PDF_OBJ_STR:
			 new_obj->val.str.type=old_obj->val.str.type;
			 new_obj->val.str.str=strdup(old_obj->val.str.str);
			 if (new_obj->val.str.str==NULL){
				 return -1;
			 }
			return 0;	 
		 case PDF_OBJ_NAME:
		 	new_obj->val.name = strdup(old_obj->val.name);
			assert(new_obj->val.name!=NULL);
			return 0;
		 case PDF_OBJ_ARRAY:
			{
				pdf_array *  p_array=old_obj->val.array.next, * pom_array;
				new_obj->val.array.next=new_obj->val.array.prev=(pdf_array*)&(new_obj->val.array);
				while (p_array!=(pdf_array *)&(old_obj->val.array)){
					pom_array=(pdf_array *)malloc(sizeof(pdf_array));
					if (pom_array==NULL){
						return -1;
					}
					pom_array->obj=pdf_new_object();
					pdf_copy_object(pom_array->obj,p_array->obj);
					__list_add((pdf_array*)&(new_obj->val.array),pom_array);
					p_array=p_array->next;
				}
			}
			return 0;
		 case PDF_OBJ_DICT:
			{
			pdf_dict * new_dict;
			pdf_dict * old_dict=old_obj->val.dict.next;
			new_obj->val.dict.next=new_obj->val.dict.prev=(pdf_dict*)&(new_obj->val.dict);
			while(old_dict!=(pdf_dict *) &(old_obj->val.dict)){
					new_dict=(pdf_dict *) malloc(sizeof(pdf_dict));
					if (new_dict==NULL){
						return -1;
					}

					new_dict->name=strdup(old_dict->name);
					assert(new_dict->name!=NULL);
					new_dict->obj=pdf_new_object();

					pdf_copy_object(new_dict->obj,old_dict->obj);
					__list_add((pdf_dict*)&(new_obj->val.dict),new_dict);
					old_dict=old_dict->next;
				}
			}
			 return 0;
		 case PDF_OBJ_STREAM:
			new_obj->val.stream.len=old_obj->val.stream.len;
			new_obj->val.stream.dict=pdf_new_object();
			pdf_copy_object(new_obj->val.stream.dict,old_obj->val.stream.dict);
			assert (old_obj->val.stream.stream!=NULL);
			if (old_obj->val.stream.len){
				new_obj->val.stream.stream=(char *) malloc(sizeof(char) * (old_obj->val.stream.len));
				if (new_obj->val.stream.stream==NULL){
					return -1;
				}
				memcpy(new_obj->val.stream.stream,old_obj->val.stream.stream,old_obj->val.stream.len);
				}
			else {
				new_obj->val.stream.stream=(char *) malloc(sizeof(char) * 1);
				if (new_obj->val.stream.stream==NULL){
					return -1;
				}
			}
			return 0;
		 case PDF_OBJ_NULL:
			 return 0;
		 case PDF_OBJ_INDIRECT:
			new_obj->val.reference.major=old_obj->val.reference.major;
			new_obj->val.reference.minor=old_obj->val.reference.minor;
			new_obj->val.reference.obj=pdf_new_object();
			pdf_copy_object(new_obj->val.reference.obj,old_obj->val.reference.obj);
			return 0;
		 case PDF_OBJ_INDIRECT_REF:
			new_obj->val.reference.major=old_obj->val.reference.major;
			new_obj->val.reference.minor=old_obj->val.reference.minor;
			 return 0;
		default:
			 assert(0);
	}
	return 0;
}

int pdf_del_dict_name_value(pdf_object * d, char * name){
	pdf_dict * it;
	if (!isDict(d)){
		return -1;
	}
	for (it=d->val.dict.next;it!=(pdf_dict*)&d->val.dict;it=it->next){
		if(strcmp(name,it->name)==0){
			__list_elm_remove(it);
			pdf_delete_object(it->obj);
			free(it->obj);
			free(it->name);
			free(it);
			return 0;
		}
	}
	return -1;
}
int pdf_merge_two_dict(pdf_object * d1, pdf_object *d2){
	pdf_dict * it, *pom_it;
	pdf_object * pom_obj;
	if (isDict(d1) && isDict(d2)){
		for (it=d2->val.dict.next;it!=(pdf_dict*)&d2->val.dict;){
			pom_it=it;
			it=it->next;
			if ((pom_obj=pdf_get_dict_name_value(d1,pom_it->name))==NULL){
				__list_elm_remove(pom_it);
				__list_add((pdf_dict *) &(d1->val.dict),pom_it);				
			}else{
				/*pdf_merge_two_dict(it->obj,pom_obj);*/

			}
			
		}
		pdf_delete_object(d2);
		return 0;
	}
	return -1;
}

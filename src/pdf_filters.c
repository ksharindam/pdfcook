#include "pdf_filters.h"
#include "pdf_parser.h"
#include "common.h"

#ifdef HAVE_ZLIB
#include <zlib.h>

int flate_decode_filter(char **stream, long  *len, pdf_object *dict){
	if (*len==0) return 0;  // in some stream dict /Length in 0
    // decompress stream using zlib
	long new_stream_len = *len * 3;
	char *new_stream_content = (char *) malloc(sizeof(char) * new_stream_len);
_z_d_try:
	switch (uncompress((Bytef *) new_stream_content,(uLongf *)&new_stream_len,(Bytef *) *stream,*len)){
	case Z_OK:
		break;
	case Z_BUF_ERROR:
		new_stream_len*=2;
		new_stream_content = (char *) realloc(new_stream_content,sizeof(char) * new_stream_len);
		assert(new_stream_content);
		goto _z_d_try;
	case Z_MEM_ERROR:
        message(WARN, "memory error in zlib\n");
	case Z_DATA_ERROR:
        message(WARN, "zlib : invalid input data\n");
	default:
        pdf_write_object(dict, stdout);
        free(new_stream_content);
		return -1;
	}
	free(*stream);
    // decode the decompressed stream
    int predictor = 1;
    pdf_object *dec_params = pdf_get_dict_name_value(dict, "DecodeParms");
    if (dec_params)
        predictor = pdf_get_dict_name_value(dec_params, "Predictor")->val.int_number;
    if (predictor == 12) { // 10-15 png filter
        int cols = pdf_get_dict_name_value(dec_params, "Columns")->val.int_number;
        cols++; // the leading extra byte that stores filter type
        int rows = new_stream_len/cols;
        char *row_data, *prev_row_data;
        char *empty_row = (char *)calloc(1,cols);;
        prev_row_data = empty_row;
        for (int row=0;row<rows;row++) {
            row_data = new_stream_content + (row*cols);
            // Predictor = 12, PNG Up filter in all rows
            for (int col=1;col<cols;col++) {
                row_data[col] = (row_data[col] + prev_row_data[col])%256;
            }
            prev_row_data = row_data;
        }
        free(empty_row);
        // remove leading byte in each row
        for (int row=0;row<rows;row++) {
            memcpy(new_stream_content + row*(cols-1), new_stream_content+(row*cols+1), cols-1);
        }
        new_stream_len = rows*(cols-1);
    }
    else if (predictor>1) {
        message(WARN, "Unsupported FlateDecode predictor of type %d\n", predictor);
    }
	*stream = new_stream_content;
	*len = new_stream_len;
	return 0;
}

int zlib_compress_filter(char ** stream, long  * len, pdf_object * dict){
	char * new_stream_content;
	long new_stream_len = 0;
	new_stream_len = *len  * 2;
	new_stream_content = (char *) malloc(sizeof(char) * new_stream_len);
	assert(new_stream_len);
try:
	switch (compress((Bytef *) new_stream_content,(uLongf *)&new_stream_len,(Bytef *) *stream,*len)){
	case Z_OK:
		break;
	case Z_MEM_ERROR:
	case Z_DATA_ERROR:
	default:
		assert(0);
		return -1;
		break;
	case Z_BUF_ERROR:
		new_stream_len*=2;
		new_stream_content = (char *) realloc(new_stream_content,sizeof(char) * new_stream_len);
		assert(new_stream_content);
		goto try;
	}
	free(*stream);
	*stream = new_stream_content;
	*len = new_stream_len;
	return 0;
}
#endif

#ifdef HAVE_LZW
#define DICT_LEN 4096
struct lzw_dict{
	size_t symbol;
	size_t prev;
	size_t len;
};

enum { LZW_CL_DICT = 256, LZW_END_STREAM = 257 };

static int lzw_raw_get_ch(unsigned char * buf,size_t len,size_t * index, size_t * offset,size_t length){
	int out;
	if (*index == len){
		return EOF;
	}
	out = ( (1<<(8 - *offset)) - 1) & buf[*index];
/*
	if (length<=(8-*offset)){
		(*offset) += length;
		(*offset) %= 8;
		out &=((1<<length) -1) << (8 - *offset);
		return out;
	}*/
	length = length - 8 + *offset;
	*offset = 0;
	(*index)++;
	if (*index == len){
		return EOF;
	}
	while (length>=8){
		length-=8;
		out = (out << 8) | buf[*index];
		(*index)++;
		if (*index == len){
			return EOF;
		}
	}
	if (length){
		out = (out << (length)) | ((buf[*index]>>(8-length)) & ((1<<length) - 1));
	}
	*offset = length;
	(*offset) %= 8;
	return out;
}

static void lzw_clear_dict(struct lzw_dict dict[DICT_LEN], size_t alpha_len){
	int i;
	for (i=0;i<alpha_len;++i){
		dict[i].symbol = i;
	        dict[i].prev = 0;
		dict[i].len = 1;
	}

	for (i=alpha_len;i>DICT_LEN;++i){
		dict[i].symbol = 0;
		dict[i].prev = 0;
		dict[i].len = 0;
	}
}

#define lzw_add_prefix(dict,prev_word,word,d_index) do{\
	if (d_index>DICT_LEN){\
		message(WARN,"Bad LZW stream - expected clear-table code\n");\
		return -1;\
	}\
	if (word<d_index && word>=0){\
		lzw_dict[d_index].symbol = word;\
		lzw_dict[d_index].prev = prev_word;\
		lzw_dict[d_index].len = dict[prev_word].len + 1;\
			prev_word=word;\
			while (lzw_dict[prev_word].prev){\
				prev_word = lzw_dict[prev_word].prev;\
			}\
			lzw_dict[d_index].symbol = prev_word;\
	}\
	else{\
		if (word==d_index){\
			lzw_dict[d_index].prev = prev_word;\
			lzw_dict[d_index].len = dict[prev_word].len + 1;\
			while (lzw_dict[prev_word].prev){\
				prev_word = lzw_dict[prev_word].prev;\
			}\
			lzw_dict[d_index].symbol = prev_word;\
		}\
		else{\
			message(WARN,"Bad LZW stream - unexpected code\n");\
			return -1;\
		}\
	}\
	++d_index;\
	switch (d_index + early){\
	case 512:\
		w_size=10;\
		break;\
	case 1024:\
		w_size=11;\
		break;\
	case 2048:\
		  w_size=12;\
		break;\
	}\
}while(0)

static void lzw_put_prefix(int word, struct lzw_dict dict[DICT_LEN], char ** out, int * len, int * index){
	char * tmp;
	if ( word>DICT_LEN ||  word<0 || dict[word].len<=0){
		return;
	}
	if (dict[word].len == 1){
		if (*len == *index){
			*len *=2;
			tmp = (char *)realloc(*out,*len);
			if (tmp){
				*out = tmp;
			}
			else{
				/*there can fail realloc, fixme*/
				*len /=2;
				assert(0);
				return;
			}
		}

		(*out)[*index] = dict[word].symbol;
		(*index)++;
	}
	else{
		lzw_put_prefix(dict[word].prev,dict,out,len,index);
		lzw_put_prefix(dict[word].symbol,dict,out,len,index);
	}
}

int lzw_decompress_filter(char ** stream, long  * len, pdf_object * dict){
	struct lzw_dict lzw_dict[DICT_LEN +1];
	size_t index, offset, w_size, d_index = LZW_END_STREAM + 1;
	int word;
	int prev_word = LZW_CL_DICT;
	char * out_buf;
	int out_len = 3 * (*len);
	int out_index = 0;
	int early = 1;
	pdf_object * early_val;

	out_buf = (char *) malloc(sizeof(char) * out_len);
	assert(out_buf!=NULL);

	early_val = pdf_get_dict_name_value(dict,"EarlyChange");
	if (early_val!=NULL && early_val->type == PDF_OBJ_INT){
		early = early_val->val.int_number;
	}

	index = 0;
	w_size = 9;

	do{
		offset = 0;
		word = lzw_raw_get_ch((unsigned char *)*stream,*len,&index,&offset,w_size);
	}while(word != EOF && word != LZW_CL_DICT);

	if (word == EOF){
		printf("EOF\n");
		return -1;
	}

	do{
		if (word==LZW_CL_DICT){
			lzw_clear_dict(lzw_dict,256);
			d_index = LZW_END_STREAM + 1;
			w_size=9;
		}
		else{
			if (prev_word!=LZW_CL_DICT){
				lzw_add_prefix(lzw_dict,prev_word,word,d_index);
			}
			lzw_put_prefix(word,lzw_dict,&out_buf,&out_len,&out_index);
		}
		prev_word = word;
		word = lzw_raw_get_ch((unsigned char *)*stream,*len,&index,&offset,w_size);
	}while(word != EOF && word != LZW_END_STREAM);


	*len = out_index;
	free(*stream);
	*stream = out_buf;
	return 0;

}
#endif

/*filter mapping for decompression*/
stream_filters  _decompress_filters[]= {
    {"FlateDecode",flate_decode_filter},
    {"LZWDecode",lzw_decompress_filter},
    {"ASCII85Decode",NULL},
    {"DCTDecode",NULL},
    {"RunLengthDecode",NULL},
    {"CCITTFaxDecode",NULL},
    {"JBIG2Decode",NULL},
    {"JPXDecode",NULL},
    {"Crypt",NULL}
};
/*filter mapping for compressions*/
stream_filters  _compress_filters[]= {
    {"FlateDecode",zlib_compress_filter},
    {"LZWDecode",lzw_compress_filter},
    {"ASCII85Decode",NULL},
    {"DCTDecode",NULL},
    {"RunLengthDecode",NULL},
    {"CCITTFaxDecode",NULL},
    {"JBIG2Decode",NULL},
    {"JPXDecode",NULL},
    {"Crypt",NULL}
};

int apply_filter(char *name, char **stream, long  *len, pdf_object *dict, stream_filters *filters, size_t f_len){
	int i;
	for (i=0;i<f_len;++i){
		if (strcmp(name,filters[i].name)==0){
			if (filters[i].filter != NULL){
				return  filters[i].filter(stream,len,dict);
			}
			else{
				return -1;
			}
		}
	}
	return -1;
}

int apply_decompress_filter(char *name, char **stream, long  *len, pdf_object *dict) {
    return apply_filter(name, stream, len, dict, _decompress_filters, sizeof(_decompress_filters)/sizeof(stream_filters));
}

int apply_compress_filter(char *name, char **stream, long  *len, pdf_object *dict) {
    return apply_filter(name, stream, len, dict, _compress_filters, sizeof(_decompress_filters)/sizeof(stream_filters));
}

int decompress_stream_object(pdf_object *stream) {
	pdf_object *p_obj, *new_stream, *tmp;
	pdf_array * p_array;
	p_obj = pdf_get_dict_name_value(stream->val.stream.dict, "Filter");
	if (p_obj == NULL) {
		return 0;
	}
	switch (p_obj->type){
	case PDF_OBJ_ARRAY:
		{
		new_stream = pdf_new_object();
		pdf_copy_object(new_stream,stream);
		pdf_del_dict_name_value(new_stream->val.stream.dict, "Filter");

		for (p_array = p_obj->val.array.next;p_array!=(pdf_array *)&(p_obj->val.array);p_array=p_array->next){
			tmp = p_array->obj;
            assert(tmp->type==PDF_OBJ_NAME);
			/*while (tmp->type == PDF_OBJ_INDIRECT_REF){ // FIXME if error occurs
                // get the object from reference
			}*/
			if (apply_decompress_filter(tmp->val.name, &(new_stream->val.stream.stream), &(new_stream->val.stream.len),new_stream->val.stream.dict) != 0){
				return -1;
			}
		}
		break;
    }
	case PDF_OBJ_NAME:
		new_stream = pdf_new_object();
		pdf_copy_object(new_stream,stream);
		p_obj = pdf_get_dict_name_value(new_stream->val.stream.dict, "Filter");
		if (apply_decompress_filter(p_obj->val.name, &(new_stream->val.stream.stream), &(new_stream->val.stream.len),new_stream->val.stream.dict) == 0){
			pdf_del_dict_name_value(new_stream->val.stream.dict, "Filter");
			break;
		}
		else{
            message(FATAL, "failed to decompress object\n");
		}
	default: // FIXME : it can be indirect object
        message(FATAL, "could not decompress stream obj of type %d\n", p_obj->type);
        return -1;
	}
    pdf_delete_object(stream);
    *stream = *new_stream;
    return 0;
}

/* This file is a part of pdfcook program, which is GNU GPLv2 licensed */
#include "common.h"
#include "pdf_objects.h"
#include <cstring>
#include <cassert>
#include "debug.h"
#include "pdf_filters.h"


// *********** ------------- Array Object ----------------- ***********
//allows range based for loop
ArrayIter ArrayObj:: begin() {
    return array.begin();
}
ArrayIter ArrayObj:: end() {
    return array.end();
}
int ArrayObj:: count () {
    return array.size();
}
PdfObject* ArrayObj:: at (int index) {
    return array[index];
}
void ArrayObj:: append (PdfObject *item) {
    array.push_back(item);
}

void ArrayObj:: deleteItems()
{
    for (PdfObject *item : array){
        delete item;
    }
    array.clear();
}

int ArrayObj:: write (FILE *f)
{
    int ret_val = fprintf(f, "[ ");
    for (PdfObject *obj : this->array){
        ret_val = obj->write(f);
        ret_val = fprintf(f, " ");
    }
    ret_val = fprintf(f, "]");
    return ret_val<0?ret_val:0;
}

// *********** ------------ Dictionary Object -------------- ***********
void DictObj:: setDict (std::map<std::string, PdfObject*> &map){
    this->dict = map;
}

bool DictObj:: contains (std::string key) {
    return (dict.count(key) > 0);
}

PdfObject* DictObj:: get (std::string key) {
    if (dict.count(key) < 1)
        return NULL;
    return dict[key];
}

void DictObj:: add (std::string key, PdfObject *val) {
    dict[key] = val;
};

PdfObject* DictObj:: newItem (std::string key)
{
    if (dict.count(key) > 0) {
        dict[key]->clear();
    }
    else {
        dict[key] = new PdfObject();
    }
    return dict[key];
}

// hard copy all items from src_dict to this, overwrite if exists
// this dict and src_dict must be different object, otherwise will cause segfault
void DictObj:: merge(DictObj *src_dict)
{
    for (auto it : src_dict->dict) {
        // if val of key is dict obj, merge the dicts
        if (this->contains(it.first) && dict[it.first]->type==PDF_OBJ_DICT &&
                                        it.second->type==PDF_OBJ_DICT) {
            dict[it.first]->dict->merge(it.second->dict);
            continue;
        }
        PdfObject *item = this->newItem(it.first);
        item->copyFrom(it.second);
    }
}

void DictObj:: filter(DictFilter &filter_set)
{
    for (auto it=dict.begin(); it!=dict.end();) {
        std::string key = it->first;
        PdfObject *val = it->second;
        it++;//it must be placed before dict.erase()
        if (filter_set.count(key) == 0) {
            delete val;
            dict.erase(key);
        }
    }
}

void DictObj:: deleteItem (std::string key)
{
    if (dict.count(key) > 0) {
        PdfObject *val = dict[key];
        delete val;
        dict.erase(key);
    }
}

void DictObj:: deleteItems()
{
    for (auto it : dict) {
        delete it.second;
    }
    dict.clear();
}

int DictObj:: write (FILE *f)
{
    fprintf(f, "<<\n");

    for (auto it : dict){
        fprintf(f, "/%s ", it.first.c_str());
        PdfObject *val = it.second;
        val->write(f);
        fprintf(f,"\n");
    }
    fprintf(f,">>");
    return 0;
}

MapIter DictObj:: begin() {
    return dict.begin();
}
MapIter DictObj:: end() {
    return dict.end();
}

PdfObject* DictObj:: operator[] (std::string key) {
    if (dict.count(key) < 1)
        return NULL;
    return dict[key];
}

// *********** ------------- Stream Object ----------------- ***********

StreamObj:: StreamObj() {
    stream = NULL;
    len = 0;
    decompressed = false;
}

int StreamObj:: write (FILE *f)
{
    if (!dict.contains("Length")){
        PdfObject *item = this->dict.newItem("Length");
        item->type = PDF_OBJ_INT;
    }
    this->dict["Length"]->integer = this->len;
    this->dict.write(f);
    fprintf(f,"\nstream\n");

    if (this->len){
        assert (this->stream!=NULL);// TODO : remove it later
        if (fwrite(this->stream, 1, this->len, f) != this->len){
            message(FATAL, "StreamObj : fwrite() error");
        }
    }
    fprintf(f, "\nendstream");
    return 0;
}

bool StreamObj:: decompress()
{
    if (decompressed)
        return true;
    PdfObject *p_obj = this->dict["Filter"];
    if (!p_obj or len==0) {
        decompressed = true;
        return true;
    }
    switch (p_obj->type){
    case PDF_OBJ_ARRAY:
        {
        for (PdfObject *filter : *p_obj->array){
            assert(filter->type==PDF_OBJ_NAME);
            /*while (filter->type == PDF_OBJ_INDIRECT_REF){ // FIXME if error occurs
                // get the object from reference
            }*/
            if (apply_decompress_filter(filter->name, &(this->stream), &(this->len), this->dict) != 0){
                return false;
            }
        }
        break;
    }
    case PDF_OBJ_NAME:
        if (apply_decompress_filter(p_obj->name, &(this->stream), &(this->len), this->dict) != 0){
            message(FATAL, "failed to decompress object");
        }
        break;
    default: // FIXME : it can be indirect object
        message(FATAL, "could not decompress stream obj of type %d", p_obj->type);
        return false;
    }
    this->dict.deleteItem("Filter");
    decompressed = true;
    return true;
}

bool StreamObj:: compress (const char *filter)
{
    char *ch;
    if (len==0)
        return true;

    if (apply_compress_filter(filter, &(this->stream), &(this->len), this->dict) != 0){
        return false;
    }
    PdfObject *filter_obj = this->dict.get("Filter");

    if (!filter_obj) {
        filter_obj = this->dict.newItem("Filter");
        asprintf(&ch,"/%s",filter);
        filter_obj->readFromString(ch);
        free(ch);
    }
    else {// already contains a filter
        switch (filter_obj->type){
        case PDF_OBJ_ARRAY:
            {
            asprintf(&ch,"/%s",filter);
            PdfObject *array_item = new PdfObject();
            array_item->readFromString(ch);
            free(ch);
            filter_obj->array->append(array_item);
            }
            break;
        case PDF_OBJ_NAME:
            asprintf(&ch, " [ /%s /%s ] ", filter_obj->name, filter);
            filter_obj->clear();
            filter_obj->readFromString(ch);
            free(ch);
            break;
        default:
            assert(0);
        }
    }
    return true;
}

StreamObj:: ~StreamObj() {
    if (stream!=NULL){
        free(stream);
    }
    dict.deleteItems();
}


// *********** -------------- Pdf Object ----------------- ***********

enum {
    DICT_KEY,
    DICT_VAL
};

PdfObject:: PdfObject() {
    type = PDF_OBJ_UNKNOWN;
}

void
PdfObject:: setType(ObjectType obj_type)
{
    type = obj_type;//TODO : if prev type is not PDF_OBJ_UNKNOWN , clear()
    switch (type)
    {
    case PDF_OBJ_DICT:
        dict = new DictObj();
        break;
    case PDF_OBJ_ARRAY:
        array = new ArrayObj();
        break;
    case PDF_OBJ_STREAM:
        stream = new StreamObj();
        break;
    case PDF_OBJ_INDIRECT:
        indirect.obj = new PdfObject();
        break;
    default:
        break;
    }
}

// create a MYFILE from given string and call PdfObject::get()
bool
PdfObject:: readFromString (const char *str)
{
    MYFILE *f = stropen(str);
    if (f==NULL){
        return false;
    }
    bool retval = this->read(f, NULL, NULL);
    myfclose(f);
    return retval;
}

/*To read an obj at particular pos, seek file in that pos and call this function.
  Returns false if object is completely unusable and should be discarded.
  Returns true if obj is usable, even if not read properly.
  Dictionary and Array return false only if ending bracket not found before reaching EOF.
*/
bool
PdfObject:: read (MYFILE *f, ObjectTable *xref, Token *last_tok)
{
    uint stream_len = 0;
    size_t fpos/*, key_pos, nextkey_pos, val_pos*/;
    Token tok;
    if (last_tok==NULL){
        last_tok=&tok;
    }
    //printf("get obj %ld\n", myftell(f));
    while (last_tok->get(f)){
        switch (last_tok->type){
        case TOK_INT://maybe integer, indirect, or indirect reference obj
        {
            this->setType(PDF_OBJ_INT);
            this->integer = last_tok->integer;
            if (last_tok->sign){//it is integer, not indirect object
                return true;
            }
            fpos = myftell(f);
            last_tok->get(f);
            if (last_tok->type!=TOK_INT || last_tok->sign){// not indirect object
                last_tok->freeData();
                if ( myfseek(f, fpos, SEEK_SET)==EOF ){
                    message(FATAL,"myfseek()  error in file %s at line %d", __FILE__, __LINE__);
                }
                return true;
            }
            // we have two integers, check if there is 'obj' or 'R' next to it
            last_tok->get(f);
            if (last_tok->type!=TOK_ID){
                last_tok->freeData();
                if (myfseek(f,fpos,SEEK_SET)==EOF){
                    message(FATAL,"myfseek()  error in file %s at line  %d",__FILE__, __LINE__);
                }
                return true;
            }
            this->indirect.major = this->integer;
            this->indirect.minor = last_tok->integer;

            if (strcmp(last_tok->id,"R")==0){
                this->setType(PDF_OBJ_INDIRECT_REF);
                return true;
            }
            if (strcmp(last_tok->id,"obj")==0){
                this->setType(PDF_OBJ_INDIRECT);
                if (not this->indirect.obj->read(f,xref,last_tok)){
                    debug("IndirectObj %d %d : failed to read", indirect.major, indirect.minor);
                    return false;
                }
                last_tok->get(f);
                if (last_tok->type!=TOK_ID || strcmp(last_tok->id,"endobj")!=0){
                    debug("IndirectObj %d %d : endobj keyword not found", indirect.major, indirect.minor);
                }
                return true;
            }
            // two int numbers and a TOK_ID next to it other than 'R' and 'obj'
            if (myfseek(f,fpos,SEEK_SET)==EOF){
                message(FATAL,"myfseek()  error in file %s at line %d",__FILE__,__LINE__);
            }
            return true;
        }
        case TOK_REAL:
        {
            this->setType(PDF_OBJ_REAL);
            this->real = last_tok->real;
            return true;
        }
        case TOK_NAME:
        {
            this->setType(PDF_OBJ_NAME);
            this->name = strdup(last_tok->name);
            return true;
        }
        case TOK_STR:
        {
            this->setType(PDF_OBJ_STR);
            this->str = last_tok->str;
            //this->str.type = last_tok->str.type;
            return true;
        }
        case TOK_BDICT:// dictionary or stream obj
        {
            std::map<std::string, PdfObject*>  new_dict;
            PdfObject *obj, *val=NULL, *len_obj=NULL;
            std::string key;
            int next_obj = DICT_KEY;
            while ((obj = new PdfObject())) {
                if (not obj->read(f, xref, last_tok)) {
                    delete obj;
                    if (last_tok->type==TOK_EDICT or last_tok->type==TOK_EOF){
                        if (val)
                            new_dict[key] = val;
                        break;
                    }
                    next_obj = DICT_KEY;// if could not read key or val, next object should be key
                }
                else if (next_obj==DICT_KEY){
                    if (obj->type==PDF_OBJ_NAME){
                        if (val){
                            new_dict[key] = val;
                            val = NULL;
                        }
                        key = obj->name;
                        next_obj = DICT_VAL;
                    }
                    else if (val) {// have read object, but it is not PdfName
                        delete val;// previous val is invalid
                        val = NULL;
                    }
                    delete obj;
                }
                else {// next_obj==DICT_VAL
                    val = obj;
                    next_obj = DICT_KEY;
                }
            }
            if (last_tok->type==TOK_EOF){// last token should be TOK_EDICT
                debug("Dictionary : ending bracket not found");
                this->setType(PDF_OBJ_DICT);
                this->dict->setDict(new_dict);
                return false;
            }
            fpos = myftell(f);
            // if dict has stream keyword, then it is stream object
            if ( (not last_tok->get(f))
                    || last_tok->type!=TOK_ID
                    || strcmp(last_tok->id, "stream")!=0) {
                this->setType(PDF_OBJ_DICT);
                this->dict->setDict(new_dict);
                myfseek(f, fpos, SEEK_SET);
                return true;
            }
            this->setType(PDF_OBJ_STREAM);
            this->stream->dict.setDict(new_dict);
            // if stream length is indirect obj, get length as integer
            if (new_dict.count("Length")==0){
                debug("StreamObj : /Length key not found");
                return false;
            }
            len_obj = new_dict["Length"];
            if (len_obj->type==PDF_OBJ_INDIRECT_REF){
                fpos = myftell(f);
                xref->readObject(f, len_obj->indirect.major);
                len_obj = xref->table[len_obj->indirect.major].obj;
                myfseek(f, fpos, SEEK_SET);
            }
            if (!isInt(len_obj)){
                debug("StreamObj : invalid stream length obj type %d", len_obj->type);
                return false;
            }
            stream_len = len_obj->integer;
            this->stream->dict.deleteItem("Length");
            // read stream after the newline
            switch (mygetc(f)){
                case EOF:
                    return false;
                case CHAR_CR:
                    if (mygetc(f)!=CHAR_LF){
                        myungetc(f);
                    }
                case CHAR_LF:
                    break;
                default:
                    myungetc(f);
                    break;
            }
            this->stream->begin = myftell(f);
            this->stream->len = stream_len;
            if (stream_len){
                this->stream->stream = (char*) malloc(stream_len);
                if (this->stream->stream==NULL){
                    message(FATAL,"malloc() error");
                }
                if (myfread(this->stream->stream,1,stream_len,f)!=stream_len){
                    message(FATAL,"fread() error");
                }
            }

            if (not last_tok->get(f)
                || last_tok->type!=TOK_ID
                || strcmp(last_tok->id,"endstream")!=0){
                debug("StreamObj : endstream keyword not found");
                return false;
            }
            return true;
        }
        case TOK_BARRAY:
        {
            this->setType(PDF_OBJ_ARRAY);
            // if start bracket is found, read until end bracked or EOF is reached
            PdfObject *item_obj;
            while ((item_obj = new PdfObject())){
                if (item_obj->read(f,xref,last_tok)){
                    this->array->append(item_obj);
                }
                else {
                    delete item_obj;
                    if (last_tok->type==TOK_EARRAY || last_tok->type==TOK_EOF)
                        break;
                }
            }
            if (last_tok->type!=TOK_EARRAY){
                debug("Array : ending bracket not found");
                return false;
            }
            return true;
        }
        case TOK_ID:
        {
            if (strcmp(last_tok->id,"null")==0){
                this->setType(PDF_OBJ_NULL);
                return true;
            }
            if (strcmp(last_tok->id,"true")==0){
                this->setType(PDF_OBJ_BOOL);
                this->boolean = true;
                return true;
            }
            if (strcmp(last_tok->id,"false")==0){
                this->setType(PDF_OBJ_BOOL);
                this->boolean = false;
                return true;
            }
            debug("unknown id '%s'", last_tok->id);
            return false;
        }
        case TOK_EOF:
        case TOK_EARRAY:
        case TOK_EDICT:
        case TOK_UNKNOWN:
        default:
            return false;
        }
    }
    return false;
}

int
PdfObject:: write (FILE * f)
{
    int ret_val = -1;

    switch (this->type)
    {
    case PDF_OBJ_BOOL:
        if (this->boolean){
            ret_val = fprintf(f, "true");
        }
        else {
            ret_val = fprintf(f, "false");
        }
        return ret_val<0?ret_val:0;
    case PDF_OBJ_INT:
        ret_val = fprintf(f, "%d", this->integer);
        return ret_val<0?ret_val:0;
    case PDF_OBJ_REAL:
    {
        ret_val = fprintf(f, "%s", double2str(this->real).c_str());
        return ret_val<0?ret_val:0;
    }
    case PDF_OBJ_STR:
        ret_val = fwrite(this->str.data, this->str.len, 1, f);
        return ret_val<0?ret_val:0;
    case PDF_OBJ_NAME:
        ret_val = fprintf(f, "/%s", this->name);
        return ret_val<0?ret_val:0;
    case PDF_OBJ_ARRAY:
        return this->array->write(f);
    case PDF_OBJ_DICT:
        return this->dict->write(f);
    case PDF_OBJ_STREAM:
        return this->stream->write(f);
    case PDF_OBJ_NULL:
        fprintf(f, "null");
        return 0;
    case PDF_OBJ_INDIRECT:
        fprintf(f, "%d %d obj\n", this->indirect.major, this->indirect.minor);
        this->indirect.obj->write(f);
        fprintf(f, "\nendobj\n");
        return 0;
    case PDF_OBJ_INDIRECT_REF:
        fprintf(f, "%d %d R", this->indirect.major, this->indirect.minor);
        return 0;
    default:
        assert(0);
    }
    return 0;
}

int
PdfObject:: copyFrom (PdfObject *src_obj){
    // create deep copy of all objects
    this->setType(src_obj->type);
    switch (src_obj->type){
        case PDF_OBJ_BOOL:
            this->boolean = src_obj->boolean;
            return true;
        case PDF_OBJ_INT:
            this->integer = src_obj->integer;
            return true;
        case PDF_OBJ_REAL:
            this->real = src_obj->real;
            return true;
        case PDF_OBJ_STR:
            str.len = src_obj->str.len;
            str.data = (char*) malloc( str.len+1);
            assert(str.data!=NULL);
            memcpy(str.data, src_obj->str.data, str.len+1);
            return true;
        case PDF_OBJ_NAME:
            this->name = strdup(src_obj->name);
            if (this->name==NULL)
                return false;
            return true;
        case PDF_OBJ_ARRAY:
            for (PdfObject *item : *src_obj->array){
                PdfObject *new_item = new PdfObject();
                new_item->copyFrom(item);
                this->array->append(new_item);
            }
            return true;
        case PDF_OBJ_DICT:
            for (auto it : *src_obj->dict){
                PdfObject *new_obj = new PdfObject();
                new_obj->copyFrom(it.second);
                this->dict->add(it.first, new_obj);
            }
            return true;
        case PDF_OBJ_STREAM:
            this->stream->len = src_obj->stream->len;
            // copy stream dictionary recursively
            for (auto it : src_obj->stream->dict){
                PdfObject *new_obj = new PdfObject();
                new_obj->copyFrom(it.second);
                this->stream->dict.add(it.first, new_obj);
            }
            if (src_obj->stream->len){
                assert (src_obj->stream->stream!=NULL);//TODO : remove it later
                this->stream->stream = (char*) malloc(src_obj->stream->len);
                if (this->stream->stream==NULL){
                    return false;
                }
                memcpy(this->stream->stream, src_obj->stream->stream, src_obj->stream->len);
            }
            return true;
        case PDF_OBJ_INDIRECT:
            this->indirect.major = src_obj->indirect.major;
            this->indirect.minor = src_obj->indirect.minor;
            this->indirect.obj = new PdfObject();
            this->indirect.obj->copyFrom(src_obj->indirect.obj);
            return true;
         case PDF_OBJ_INDIRECT_REF:
            this->indirect.major = src_obj->indirect.major;
            this->indirect.minor = src_obj->indirect.minor;
            return true;
        case PDF_OBJ_NULL:
            return true;
        default:
            assert(0);
    }
    return true;
}

void PdfObject:: clear()
{
    switch (type)
    {
    case PDF_OBJ_STR:
        free(str.data);
        break;
    case PDF_OBJ_NAME:
        free(name);
        break;
    case PDF_OBJ_ARRAY:
        array->deleteItems();
        delete array;
        break;
    case PDF_OBJ_DICT:
        dict->deleteItems();
        delete dict;
        break;
    case PDF_OBJ_STREAM:
        delete stream;
        break;
    case PDF_OBJ_INDIRECT:
        delete indirect.obj;
        break;
    default:
        break;
    }
    this->type = PDF_OBJ_UNKNOWN;
}

PdfObject:: ~PdfObject() {
    clear();
}


// *********** -------------- Pdf ObjectTable ----------------- ***********
int
ObjectTable:: count() {
    return table.size();
}

void
ObjectTable:: expandToFit (size_t size) {
    if (size > table.size()) {
        ObjectTableItem item = {NULL,0,0,0,0,0,0};
        table.resize(size, item);
    }
}

// take obj no. and get and indirect object using this table
bool
ObjectTable:: readObject(MYFILE *f, int major)
{
    if (table[major].obj != NULL) return true;// already read
    // read object if nonfree object
    if (table[major].type==NONFREE_OBJ)
    {
        int offset = table[major].offset;
        // some bad xref table may have offset==0, or offset > file size
        if (offset==0 or myfseek(f, offset, SEEK_SET)){
            debug("object %d : invalid offset %d", major, offset);
            goto fail;
        }
        PdfObject obj;
        if (!obj.read(f, this, NULL) or obj.type!=PDF_OBJ_INDIRECT){
            debug("object %d : failed to parse object", major);
            goto fail;
        }
        if (obj.indirect.major!=major || obj.indirect.minor!=table[major].minor){
            debug("object %d : mismatched obj_no %d or gen_no %d", obj.indirect.major, obj.indirect.minor);
        }
        table[major].obj = obj.indirect.obj;
        obj.type = PDF_OBJ_UNKNOWN;// this is to prevent obj.indirect.obj from being deleted
    }
    // read object if compressed nonfree object
    else if (table[major].type==COMPRESSED_OBJ) {
        // this object is inside a object stream.
        int obj_stm_no = table[major].obj_stm;
        this->readObject(f, obj_stm_no);
        if (not isStream(table[obj_stm_no].obj)) {
            debug("object %d : invalid source obj stream %d", major, obj_stm_no);
            goto fail;
        }
        StreamObj *obj_stm = table[obj_stm_no].obj->stream;
        obj_stm->decompress();
        int n = obj_stm->dict["N"]->integer; // number of objects in this stream
        int first = obj_stm->dict["First"]->integer;// offset of first member inside stream
        // open stream as file, parse and get all objects inside it
        // stream contains : obj_no1 offset1 obj_no2 offset2 ... obj_1 obj2 ...
        MYFILE *file = streamopen(obj_stm->stream, obj_stm->len);
        Token tok;
        for (int i=0; i<n; i++) {
            tok.get(file);
            int obj_no = tok.integer;
            tok.get(file);
            int offset = first + tok.integer;
            if (table[obj_no].obj_stm != obj_stm_no)// the object table says,
                continue;   // this obj no is stored in another stream
            size_t last_seek = myftell(file);
            myfseek(file, offset, SEEK_SET);
            PdfObject *new_obj = new PdfObject();
            if (not new_obj->read(file, this, NULL)){
                debug("compressed obj %d : failed to read", obj_no);
                new_obj->type = PDF_OBJ_NULL;
            }
            table[obj_no].obj = new_obj;
            myfseek(file, last_seek, SEEK_SET);
        }
        myfclose(file);
        // the object stream is no longer required, as we have loaded all objects inside it
        delete table[obj_stm_no].obj;
        table[obj_stm_no].obj = NULL;
        table[obj_stm_no].type = FREE_OBJ;
    }
    return true;
fail:
    table[major].obj = new PdfObject();
    table[major].obj->type = PDF_OBJ_NULL;
    return false;
}

// read all objects after loading xref table
void ObjectTable:: readObjects(MYFILE *f)
{
    // at first load nonfree objects and then decompress object streams
    for (size_t i=1; i<table.size(); ++i) {
        //message(LOG, "reading obj %d, type %d", i, xref->table[i].type);
        switch (table[i].type) {
            case FREE_OBJ:
                break;
            case NONFREE_OBJ:
                readObject(f, i);
                break;
            case COMPRESSED_OBJ:
                readObject(f, i);// here obj has been decompressed and read
                table[i].type = NONFREE_OBJ;
                break;
            default:
                debug("obj_table item %d : invalid obj type", i);
        }
    }
}

int getXrefType(MYFILE *f)
{
    char line[LLEN];
    skipspace(f);
    long fpos = myftell(f);
    if (myfgets(line, LLEN, f)==NULL){
        return XREF_INVALID;
    }
    if (starts(line, "xref")) {
        myfseek(f, fpos, SEEK_SET);
        return XREF_TABLE;
    }
    // if it is indirect obj, then it is xref stream
    myfseek(f, fpos, SEEK_SET);
    Token tok;
    if (tok.get(f) && tok.type==TOK_INT && tok.get(f) && tok.type==TOK_INT
            && tok.get(f) && tok.type==TOK_ID){
        myfseek(f, fpos, SEEK_SET);
        return XREF_STREAM;
    }
    tok.freeData();
    return XREF_INVALID;
}

bool ObjectTable:: read (MYFILE *f, size_t xref_pos)
{
    size_t pos=0;
    int len=0, object_id=0, object_count=0;
    char line[LLEN];
    ObjectTableItem *elm;

    if (myfseek(f, xref_pos, SEEK_SET)==-1){
        return false;
    }
    // bad pdf may contain a newline before 'xref'
    skipspace(f);
    if (myfgets(line,LLEN,f)==NULL){
        return false;
    }
    if (!starts(line, "xref")) {
        return false;
    }
    //FILE *fd = fopen("xref", "wb");
    while ((pos = myftell(f)) && myfgets(line,LLEN,f)!=NULL){
        char *entry = line;
        while (isspace(*entry)) // fixes for leading spaces in xref table
            entry++;
        len = strlen(entry)-1;
        if (len==-1) continue; // skip empty lines
        while (len >= 0 && isspace((unsigned char)(entry[len]))){
            entry[len] = 0;
            --len;
        }
        if (strlen(entry)==XREF_ENT_LEN){
            int field1, field2;
            char obj_type;
            if (sscanf(entry,"%d %d %c", &field1, &field2, &obj_type)!=3){
                break;
            }
            //fprintf(fd, "%s\n", entry);
            elm = &(this->table[object_id]);
            if (elm->type==0){ // skip if already set by next xreftable
                elm->major = object_id;
                elm->type = obj_type=='f'? FREE_OBJ : NONFREE_OBJ;
                elm->offset = field1;
                elm->minor = field2;
            }
            object_id++;
            object_count--;
        }
        else {
            int object_begin_tmp, object_count_tmp;
            if (sscanf(entry,"%d %d", &object_begin_tmp, &object_count_tmp)!=2){
                myfseek(f, pos, SEEK_SET);// seek before trailer keyword
                break;
            }
            object_id = object_begin_tmp;
            object_count = object_count_tmp;
            this->expandToFit(object_begin_tmp + object_count);
        }
    }
    //fclose(fd);
    if (object_count!=0){
        return false;
    }
    // in some pdf table size is found to be 0
    if (table.size()>0 and table[0].type!=FREE_OBJ){// obj 0 is nonfree in bad xref tables
        debug("obj no 0 is not free");
        table[0].type = FREE_OBJ;// obj 0 is always free
        table[0].minor = 65535; // and it has maximum gen id
        table[0].offset = 0;
    }
    return true;
}

// from PDF 1.5 the xreftable can be a stream in an indirect object.
// the dictionary of stream is the trailer dictionary.
// essential keys : Type, Size and W . Optional keys : Index, Prev
bool ObjectTable:: read (PdfObject *stream, PdfObject *p_trailer)
{
    //FILE *fd;
    //fd = fopen("xref", "wb");
    stream->stream->decompress();
    // table_size is the max object number + 1
    int table_size = p_trailer->dict->get("Size")->integer;
    this->expandToFit(table_size);
    // split stream into table, W parameter is array of length 3
    ArrayObj *w_arr_obj = p_trailer->dict->get("W")->array;
    int w_arr[3];
    for (int i=0; i<3; ++i) {
        w_arr[i] = w_arr_obj->at(i)->integer;
    }
    int row_len = w_arr[0] + w_arr[1] + w_arr[2];
    // Index is array of pairs of integers. Each pair has obj number and obj count
    PdfObject *index = p_trailer->dict->get("Index");
    if (index==NULL) {
        index = new PdfObject();
        char s[24];
        snprintf(s, 23, "[ 0 %d ]", table_size);
        assert( index->readFromString(s) );
        p_trailer->dict->add("Index", index);
    }
    auto item = index->array->begin();
    for (int i=0; item != index->array->end(); item++) {
        int first = (*item)->integer;
        item++;
        int count = (*item)->integer;
        for (int major=first; major<first+count; major++) {
            if (this->table[major].type!=FREE_OBJ){//skip when already set by next xref table
                i+=row_len;
                continue;
            }
            char *row = stream->stream->stream + i;
            int field1 = w_arr[0] ? arr2int(row, w_arr[0]) : 1;// this field may be absent
            int field2 = arr2int(row+w_arr[0], w_arr[1]);
            int field3 = w_arr[2] ? arr2int(row+w_arr[0]+w_arr[1], w_arr[2]) : 0;
            //fprintf(fd, "%d %d %d %d\n", major, field1, field2, field3);
            ObjectTableItem *elm = &(this->table[major]);
            elm->major = major;
            elm->type = field1;
            switch (field1) {
            case FREE_OBJ:
                elm->next_free = field2;
                elm->minor = field3;
                break;
            case NONFREE_OBJ:
                elm->offset = field2;
                elm->minor = field3;
                break;
            case COMPRESSED_OBJ:
                elm->obj_stm = field2;// minor=0
                elm->index = field3;
                break;
            default:
                break;
            }
            i += row_len;
        }
    }
    //fflush(fd);
    //fclose(fd);
    if (table[0].type!=FREE_OBJ){//in some bad xref tables
        debug("obj no 0 is not free");
        table[0].type = FREE_OBJ;
        table[0].minor = 65535;
        table[0].offset = 0;
    }
    return true;
}

int ObjectTable:: addObject (PdfObject *obj)
{
    int major = table.size();
    ObjectTableItem item = {NULL,0,0,0,0,0,0};
    table.resize(major+1, item);

    table[major].major = major;
    table[major].type = NONFREE_OBJ;
    table[major].obj = obj;
    return major;
}

PdfObject* ObjectTable:: getObject(int major, int minor)
{
    if (major<(int)table.size() && minor==table[major].minor)
        return table[major].obj;
    debug("warning : could not get object (%d,%d) from ObjectTable", major,minor);
    return NULL;
}

void ObjectTable:: writeObjects (FILE *f)
{
    for (size_t i=1; i<table.size(); ++i){
        switch (table[i].type){
            case FREE_OBJ:
                continue;
            case NONFREE_OBJ:
                table[i].offset = ftell(f);
                if (fprintf(f,"%d %d obj\n", table[i].major, table[i].minor)<0){
                    message(FATAL,"writeObjects() : I/O error");
                }
                if (table[i].obj->write(f)<0){
                    message(FATAL,"writeObjects() : I/O error");
                }
                if (fprintf(f,"\nendobj\n")<0){
                    message(FATAL,"writeObjects() : I/O error");
                }
                break;
            default:
                assert(0);
        }
    }
}

void ObjectTable:: writeXref (FILE *f)
{
    fprintf(f, "xref\n%d %d\n", 0, table.size());
    for (size_t i=0; i<table.size(); ++i){
        char type = (table[i].type!=FREE_OBJ) ? 'n' : 'f';
        if (fprintf(f,"%010d %05d %c \n", table[i].offset, table[i].minor, type)<0){
            message(FATAL, "writeXref() : I/O error");
        }
    }
}

ObjectTableItem& ObjectTable:: operator[] (int index) {
    assert(index >=0 && index<(int)table.size());
    return table[index];
}

// *********** ------------- Token Parser ----------------- ***********

typedef struct {
    char *str;
    size_t size;// allocated size
    size_t cpoz;//current position, str length+1
} mystring;

static int mystring_new(mystring * s){
    s->size = 10;
    s->str = (char *) malloc(sizeof(char) * s->size);
    s->str[0] = 0;
    s->cpoz = 1;
    if (s->str==NULL){
        s->size = 0;
        return -1;
    }
    return 0;
}

static int mystring_add_char(mystring * s,char c)
{
    char *new_str;
    if (s->size == s->cpoz){
        s->size = s->size * 2;
        new_str = (char *) realloc(s->str, sizeof(char) * s->size);
        if (new_str==NULL){
            s->size = s->size / 2;
            return -1;
        }
        s->str = new_str;
    }
    s->str[s->cpoz-1] = c;
    s->str[s->cpoz] = 0;
    s->cpoz++;
    return 0;
}


Token:: Token() {
    type = TOK_UNKNOWN;
}

bool
Token:: get (MYFILE * f)
{
    int c, minus=0, parenthes;
    mystring mstr = {NULL,0,0};
    size_t i;
    int number;
    double real_number, frac;
    // skip whitespace characters
    int newline = 0;
    while (1){
        c = mygetc(f);
        switch (c){
            case EOF:
                this->type = TOK_EOF;
                return true;
            // white space
            case CHAR_FF:
            case CHAR_SP:
            case CHAR_TAB:
                newline = 0;
                break;
            // new line
            case CHAR_LF:
            case CHAR_CR:
                newline = 1;
                break;
            default:
                goto end_wh_sp;
        }
    }
end_wh_sp:
    this->new_line = newline;
    switch (c){
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
        case '+':
        case '.':
            if (c!='+' && c!='-'){// digit
                number = c-'0';
                this->sign = 0;
            }
            else {
                number = 0;
                switch (c) {
                    case '+':
                        this->sign = 1;
                        break;
                    case '-':
                        minus = -1;
                        this->sign = -1;
                        break;
                }
            }
            if (c=='.'){ // number beginning with '.' eg - .21
                goto real_num;
            }
            while ((c=mygetc(f))!=EOF && isdigit(c)){
                number = number*10+(c-'0');
            }
            switch (c){
            // white spaces
            case CHAR_FF:
            case CHAR_SP:
            case CHAR_TAB:
            case CHAR_LF:
            case CHAR_CR:
            default:
                myungetc(f);
            case EOF:
                this->type = TOK_INT;
                this->integer = number * ((minus==-1)?-1:1);
                return true;
            case '.':
                break;
            }
        real_num:
            real_number = number;
            frac = 10;
            while ((c=mygetc(f))!=EOF && isdigit(c)){
                real_number = real_number + (c-'0')/frac;
                frac = frac * 10;
            }
            switch(c){
            /*bily znak*/
            case CHAR_FF:
            case CHAR_SP:
            case CHAR_TAB:
            // new line
            case CHAR_LF:
            case CHAR_CR:
            case ']':
            case '>':
            case '/':
                myungetc(f);
            case EOF:
                this->type = TOK_REAL;
                this->real = real_number * ((minus==-1)?-1:1);
                return true;
            default:
                this->type = TOK_UNKNOWN;
                return false;
            }
            break;

        case '[': /*begin array*/
            this->type = TOK_BARRAY;
            return true;
        case ']': /*end array*/
            this->type = TOK_EARRAY;
            return true;
        case '<': /*hexadecimal string or dictionary*/
            if ((c=mygetc(f))=='<'){
                this->type = TOK_BDICT;
                return true;
            }
            else {
                //this->str.type=PDF_STR_HEX;
                mystring_new(&mstr);
                mystring_add_char(&mstr, '<');
                /*hexadecimal string*/
                while (c!=EOF && c!='>'){
                    mystring_add_char(&mstr,c);
                    c = mygetc(f);
                }
                if (c=='>') {
                    mystring_add_char(&mstr, '>');
                    mstr.str = (char*) realloc(mstr.str, mstr.cpoz);
                    this->type = TOK_STR;
                    this->str.len = mstr.cpoz-1;
                    this->str.data = mstr.str;
                    return true;
                }
                else {//EOF, should free mystring data
                    this->type = TOK_UNKNOWN;
                    return false;
                }
            }
            break;

        case '>': //end dictionary
                if (mygetc(f)=='>'){
                    this->type = TOK_EDICT;
                    return true;
                }
                else {
                    this->type = TOK_UNKNOWN;
                    myungetc(f);
                    return false;
                }
            break;
        case '(': // literal string, it may contain balanced parentheses
                parenthes = 0;
                //this->str.type=PDF_STR_CHR;
                mystring_new(&mstr);
                mystring_add_char(&mstr, '(');
                while ((c=mygetc(f))!=EOF){
                    switch(c){
                        case '\\':
                            mystring_add_char(&mstr,c);
                            c=mygetc(f);

                            break;
                        case '(':
                            parenthes++;
                            break;
                        case ')':
                            if (parenthes==0){
                                goto end_lit_str;
                            }
                            --parenthes;
                            break;
                    }
                    mystring_add_char(&mstr,c);
                }
end_lit_str:
                if (c==EOF){//should free mystring data here
                    this->type = TOK_UNKNOWN;
                    return false;
                }
                mystring_add_char(&mstr, ')');
                mstr.str = (char*) realloc(mstr.str, mstr.cpoz);
                this->type = TOK_STR;
                this->str.len = mstr.cpoz-1;// string sometimes contains null byte, so need to store size
                this->str.data = mstr.str;
                return true;
            break;
        case '/':  //name object
            i=0;
            while ((c=mygetc(f))!=EOF){
                switch(c){
                    case CHAR_FF:
                    case CHAR_SP:
                    case CHAR_TAB:
                    case CHAR_LF:
                    case CHAR_CR:
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
                        goto end_name;
                }
                if (i+1<PDF_NAME_MAX_LEN){
                    this->name[i] = c;
                    ++i;
                }
                else {
                    break;
                }
            }
end_name:
            this->name[i] = 0;
            this->type = TOK_NAME;
            return true;
        case '%': //comment, skip characters to end of line, then find next token
            while ((c=mygetc(f))!=EOF && c!=CHAR_LF && c!=CHAR_CR)
                ;
            if (c==EOF){
                this->type = TOK_UNKNOWN;
                return true;
            }
            else {
                myungetc(f);
                return this->get(f);
            }
            break;
        default:
            i=0;
            do {
            switch (c){
                case CHAR_FF:
                case CHAR_SP:
                case CHAR_TAB:
                case CHAR_LF:
                case CHAR_CR:
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
                    goto end_id;
                }
                if (i+1<PDF_ID_MAX_LEN){
                    this->id[i] = c;
                    ++i;
                }
                else {
                    break;
                }
            } while ((c=mygetc(f))!=EOF);
end_id:
            this->id[i] = 0;
            this->type = TOK_ID;
            return true;
    }
    return true;
}

void
Token:: freeData()
{
    switch (this->type) {
        case TOK_STR:
            free(this->str.data);
            break;
        default:
            break;
    }
}



static int char2int(char input)
{
  if(input >= '0' && input <= '9')
    return input - '0';
  if(input >= 'A' && input <= 'F')
    return input - 'A' + 10;
  if(input >= 'a' && input <= 'f')
    return input - 'a' + 10;
  throw std::invalid_argument("Invalid input string");
}

// convert literal and hex pdfstring to normal string
std::string pdfstr2bytes(String str, int *str_type)
{
    std::string out_str="";
    if (str.data[0]=='(' && str.data[str.len-1]==')')
    {
        *str_type = BYTE_STR;

        for (int i=1; i<str.len-1; i++)
        {
            if (str.data[i]=='\\') {
                i++;
                switch (str.data[i]) {
                case '\\':
                case '(':
                case ')':
                    out_str.push_back(str.data[i]);
                    break;
                case 'n':
                    out_str.push_back('\n');
                    break;
                case 'r':
                    out_str.push_back('\r');
                    break;
                case 't':
                    out_str.push_back('\t');
                    break;
                case 'b':
                    out_str.push_back('\b');
                    break;
                case 'f':
                    out_str.push_back('\f');
                    break;
                case '\r':// ignore newline after '\'
                    if (i+1<str.len-1 && str.data[i+1]=='\n')
                        i++;
                    break;
                default:
                    if (isdigit(str.data[i])) {// octal digit
                        out_str.push_back('\\');
                        out_str.push_back(str.data[i]);
                    }
                }
            }
            else {
                out_str.push_back(str.data[i]);
            }
        }
    }
    else if (str.data[0]=='<' && str.data[str.len-1]=='>')
    {
        *str_type = HEX_STR;
        // if no. of chars is odd, last char is assumed to be 0
        if (str.len%2!=0)
            str.data[str.len-1] = '0';

        for (int i=1; i<str.len-1; i+=2)
        {
            int c = 16*char2int(str.data[i]) + char2int(str.data[i+1]);
            out_str.push_back(c);
        }
    }
    return out_str;
}

void bytes2pdfstr(std::string str, String &out_str, int str_type)
{
    std::string tmp_str;

    if (str_type==HEX_STR){
        tmp_str.push_back('<');
        char hex[3];
        for (unsigned int i=0; i<str.size(); i++){
            snprintf(hex, 3, "%02x", str[i]);
            tmp_str.push_back(hex[0]);
            tmp_str.push_back(hex[1]);
        }
        tmp_str.push_back('>');
    }
    else {//BYTE_STR
        tmp_str.push_back('(');
        for (unsigned int i=0; i<str.size(); i++){
            char c = str[i];
            switch (c) {
                case '(':
                case ')':
                    tmp_str.push_back('\\');
                    tmp_str.push_back(c);
                    break;
                case '\\':
                    tmp_str.push_back('\\');
                    if (i+1<str.size() && isdigit(str[i+1]))// octal
                        break;
                default:
                    tmp_str.push_back(c);
            }
        }
        tmp_str.push_back(')');
    }
    if (out_str.len < (int)tmp_str.size())
        out_str.data = (char*)realloc(out_str.data, tmp_str.size());
    memcpy(out_str.data, tmp_str.data(), tmp_str.size());
    out_str.len = tmp_str.size();
};

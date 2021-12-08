/* This file is a part of pdfcook program, which is GNU GPLv2 licensed */
#include "common.h"
#include "pdf_doc.h"
#include "debug.h"
#include <set>

static void updateRefs(PdfDocument &doc);
static void deleteUnusedObjects(PdfDocument &doc);

static DictFilter trailer_filter({ "Size", "Root", "ID"});
static DictFilter catalog_filter({ "Pages", "Type"});
static DictFilter page_filter({ "Type", "Parent", "Resources", "Contents" });
static DictFilter xobject_filter({ "Type", "Subtype", "FormType", "BBox", "Resources", "Length", "Filter"});

// These standard 14 font names are supported by all PDF viewers
static std::set<std::string> standard_fonts({
    "Times-Roman", "Times-Bold", "Times-Italic", "Times-BoldItalic",
    "Helvetica", "Helvetica-Bold", "Helvetica-Oblique", "Helvetica-BoldOblique",
    "Courier", "Courier-Bold", "Courier-Oblique", "Courier-BoldOblique",
    "Symbol", "ZapfDingbats"
});

void print_font_names()
{
    fprintf(stderr, "Standard 14 Fonts :\n");
    for (auto font : standard_fonts) {
        fprintf(stderr, "    %s\n", font.c_str());
    }
}

void PageList:: append(PdfPage &page) {
    array.push_back(page);
}
void PageList:: remove(int index)
{
    array.erase(array.begin()+index);
}
void PageList:: clear()
{
    array.clear();
}

int PageList:: count() {
    return array.size();
}
PageIter PageList:: begin() {
    return array.begin();
}
PageIter PageList:: end() {
    return array.end();
}


PdfDocument:: PdfDocument()
{
    trailer = new PdfObject();
    trailer->setType(PDF_OBJ_DICT);
    // set default paper size
    paper.right = Point(595, 842);
    bbox = paper;
    encrypted = false;
    have_encrypt_info = false;
    decryption_supported = false;
}

PdfDocument:: ~PdfDocument()
{
    page_list.clear();
    int count = obj_table.count();
    for (int i=0; i<count; i++) {
        PdfObject *obj = obj_table[i].obj;
        if (obj!=NULL)
            delete obj;
    }
    obj_table.table.clear();
    delete trailer;
}

// Read pdf header and get version (major and minor)
bool PdfDocument:: getPdfHeader (MYFILE *f, char *line)
{
    int major=1, minor=4;
    char *s;
    // read until %PDF- or EOF is reached
    while ( (s=myfgets(line, LLEN, f))!=NULL && (!starts(line, "%PDF-")) );

    if (s==NULL) return false;
    // get pdf version
    char *endptr;
    major = strtol(line+5, &endptr, 10);
    if (*endptr=='.'){
        endptr++;
        minor = strtol(endptr, &endptr, 10);
    }
    if (major<1 || minor<0 || (major==1 && minor<4) ){// minimum version is set 1.4
        major = 1;
        minor = 4;
    }
    this->v_major = major;
    this->v_minor = minor;
    return true;
}

bool PdfDocument:: getPdfTrailer (MYFILE *f, char *line, long offset)
{
    // read from end of file and find last xref offset
    if (offset==-1){
        int i, n, c;
        char *p;
        char buff[STARTXREF_OFFSET + 1];

        if (myfseek(f, -1*STARTXREF_OFFSET, SEEK_END)==-1){
            message(ERROR, "Seek error");
            return false;
        }
        for (n=0; n<STARTXREF_OFFSET && (c=mygetc(f))!=EOF; ++n){
            buff[n] = c;
        }
        buff[n] = '\0';
        // find startxref
        for (i = n-9; i >= 0; --i) {
            if (!strncmp(buff+i, "startxref", 9))
                break;
        }
        if (i < 0) {
            message(FATAL,"'startxref' not found");
            return false;
        }
        for (p = buff+i+9; isspace(*p); p++);
        offset = atol(p);
    }
    myfseek(f, offset, SEEK_SET);

    int xref_type = getXrefType(f);
    if (xref_type==XREF_INVALID){
        message(ERROR, "failed to determine xref type");
        return false;
    }

    if (xref_type==XREF_TABLE){
        if (not obj_table.read(f, offset)){
            message(FATAL,"xreftable read error");
        }
        // skip trailer keyword
        long fpos;
        do {
            fpos = myftell(f);
            if (myfgets(line,LLEN,f)==NULL){
                message(ERROR, "'trailer' keyword not found");
                return false;
            }
        }
        while (!starts(line,"trailer"));
        // some pdfs may have space after trailer keyword instead of newline
        // set seek pos just after trailer keyword
        myfseek(f, fpos+7, SEEK_SET);
    }
    // read trailer dictionary
    PdfObject content;
    if (not content.read(f, NULL, NULL)) {
        message(FATAL, "Unable to read trailer object");
    }
    PdfObject *p_trailer = new PdfObject();

    if (content.type==PDF_OBJ_INDIRECT && content.indirect.obj->type==PDF_OBJ_STREAM){
        p_trailer->setType(PDF_OBJ_DICT);
        p_trailer->dict->merge(&content.indirect.obj->stream->dict);
    }
    else if (content.type==PDF_OBJ_DICT) {
        p_trailer->copyFrom(&content);
    }
    else {
        debug("trailer obj is neither dict nor stream");
        delete p_trailer;
        return false;
    }
    if (xref_type==XREF_STREAM) {
        if (not obj_table.read(content.indirect.obj, p_trailer)){
            message(FATAL,"xreftable read error");
            return false;
        }
    }
    if (p_trailer->dict->contains("Encrypt")){
        if (xref_type==XREF_STREAM){
            message(FATAL, "Can not handle encrypted PDF with Xref stream");
        }
        if (!have_encrypt_info) {
            encrypted = true;
            PdfObject *encrypt_dict = p_trailer->dict->get("Encrypt");

            if (isRef(encrypt_dict)){
                if (not obj_table.readObject(f, encrypt_dict->indirect.major))
                    return false;
                encrypt_dict = obj_table.getObject(encrypt_dict->indirect.major,
                                                    encrypt_dict->indirect.minor);
            }
            if (isDict(encrypt_dict)){
                crypt.getEncryptionInfo(encrypt_dict, p_trailer);
                have_encrypt_info = true;
                decryption_supported = crypt.decryptionSupported();
            }
        }
        p_trailer->dict->deleteItem("Encrypt");
    }
    PdfObject *prev = p_trailer->dict->get("Prev");
    if (prev){
        if (prev->type!=PDF_OBJ_INT){
            message(FATAL,"Object in dict of trailer Prev is not int");
            return false;
        }
        if (not getPdfTrailer(f, line, prev->integer)){
            return false;
        } // this->trailer = Prev trailer, p_trailer = current trailer
        p_trailer->dict->deleteItem("Prev");
    }
    if (not repair_mode)
        p_trailer->dict->filter(trailer_filter);
    this->trailer->dict->merge(p_trailer->dict);
    delete p_trailer;
    return true;
}

bool PdfDocument:: getAllPages(MYFILE *f)
{
    PdfObject *pobj = trailer->dict->get("Root");//get Catalog

    if (not isRef(pobj)) {
        message(FATAL,"Trailer dictionary doesn't contain Root entry");
    }
    pobj = obj_table.getObject(pobj->indirect.major, pobj->indirect.minor);
    if (not repair_mode)
        pobj->dict->filter(catalog_filter);
    pobj = pobj->dict->get("Pages");
    if (not isRef(pobj)){
        message(FATAL,"Catalog dictionary dosn't contain Pages entry");
    }
    bool ret_val = getPdfPages(f, pobj->indirect.major, pobj->indirect.minor);
    return ret_val;
}

bool PdfDocument:: open (const char *fname)
{
    MYFILE *f;
    char iobuffer[LLEN];

    if (!file_exist(fname)){
        message(FATAL,"File '%s' not found", fname);
    }
    if ((f=myfopen(fname, "rb"))==NULL){
        return false;
    }
    filename = fname;
    if (not getPdfHeader(f,iobuffer)){
        message(ERROR, "failed to read PDF header");
        myfclose(f);
        return false;
    }
    message(LOG, fname);
    if (not getPdfTrailer(f,iobuffer,-1)){
        message(ERROR, "failed to read PDF trailer");
        myfclose(f);
        return false;
    }
    if (encrypted){
        if (have_encrypt_info){
            decrypt("");// if user password is empty, we can decrypt it
            return true;
        }
        return false;
    }
    obj_table.readObjects(f);
    getAllPages(f);
    myfclose(f);

    debug("    Version : %d.%d", v_major, v_minor);
    debug("    Objects : %d", obj_table.table.size());
    message(LOG, "    Pages : %d", page_list.count());
    return true;
}

bool PdfDocument:: decrypt(const char *password)
{
    MYFILE *f;
    if ((f=myfopen(filename, "rb"))==NULL){
        return false;
    }
    if (!decryption_supported){
        message(ERROR, "decryption is not supported for this PDF");
        return false;
    }
    // if object table is loaded, decrypt all objects in object table
    if (not crypt.authenticate(password)){
        if (strlen(password)!=0)
            message(ERROR, "Incorrect password !");
        return false;
    }
    obj_table.readObjects(f);

    for (int obj_no=0; obj_no<obj_table.count(); obj_no++) {
        if (obj_table[obj_no].obj!=NULL) {
            crypt.decryptIndirectObject(obj_table[obj_no].obj, obj_no, obj_table[obj_no].minor);
        }
    }
    encrypted = false;
    getAllPages(f);
    myfclose(f);
    debug("    Version : %d.%d", v_major, v_minor);
    debug("    Objects : %d", obj_table.table.size());
    message(LOG, "    Pages : %d", page_list.count());
    return true;
}

bool PdfDocument:: getPdfPages(MYFILE *f, int major, int minor)
{
    PdfObject *pages, *pages_type, *kids, *child_pg;
    PdfObject *resources, *child_resources, *resources_tmp;
    Rect tmp_bbox;
    Rect tmp_paper;

    pages = obj_table.getObject(major, minor);
    pages_type = pages->dict->get("Type");
    if (not isName(pages_type)){
        message(FATAL,"Pages or Page dictionary dosn't contain /Type entry");
    }
    /*Pages node*/
    if (strcmp("Pages", pages_type->name)==0){
        tmp_paper = this->paper;
        tmp_bbox = this->bbox;
        // set paper size
        this->paper.getFromObject(pages->dict->get("MediaBox"), obj_table);
        // set bounding box
        bbox_is_cropbox = false;
        if (!this->bbox.getFromObject(pages->dict->get("CropBox"), obj_table)){
            if (!this->bbox.getFromObject(pages->dict->get("TrimBox"), obj_table)){
                this->bbox = Rect();
            }
        }
        else bbox_is_cropbox = true;
        // get all childs, each child may be a Pages Node, or a Page Object
        kids = derefObject(pages->dict->get("Kids"), obj_table);

        if (not isArray(kids)){
            message(FATAL,"Pages dictionary doesn't contain /Kids entry");
        }
        resources = derefObject(pages->dict->get("Resources"), obj_table);

        for (auto kid=kids->array->begin(); kid!=kids->array->end(); kid++)
        {
            if ((*kid)->type!=PDF_OBJ_INDIRECT_REF){
                message(FATAL,"Kids array item is not indirect ref object");
            }
            // add resources of Pages Node to child page Resources
            if (isDict(resources)){
                child_pg = obj_table.getObject((*kid)->indirect.major, (*kid)->indirect.minor);
                // child has Resources entry, merge with parent's Resources Dict
                if ((child_resources = child_pg->dict->get("Resources"))!=NULL){
                    child_resources = derefObject(child_resources, obj_table);
                    assert(child_resources->type==PDF_OBJ_DICT);
                    // both resources may be same indirect obj, no need to merge then
                    if (resources != child_resources){
                        PdfObject *new_res = new PdfObject();
                        new_res->copyFrom(resources);
                        new_res->dict->merge(child_resources->dict);
                        child_pg->dict->deleteItem("Resources");
                        child_pg->dict->add("Resources", new_res);
                    }
                }
                else {// child doesn't have Resources entry, copy all Resources from parent
                    resources_tmp = child_pg->dict->newItem("Resources");
                    resources_tmp->copyFrom(resources);
                }
            }
            getPdfPages(f, (*kid)->indirect.major, (*kid)->indirect.minor);
        }
        this->paper = tmp_paper;
        this->bbox = tmp_bbox;
        return true;
    }
    /*Page leaf*/
    if (strcmp("Page", pages_type->name)==0){
        PdfPage new_page;
        /* Page Boundaries are very confusing. There are 4 types of Boundaries
           MediaBox = Paper Size on which page is printed
           CropBox = this is the displayed page size in Viewer. When printed, outside
           this area is not printed. The printer fits CropBox inside printable
           area of paper when FitToPaper is on in printer settings.
           TrimBox = Same as CropBox When FitToPage is on in printer, otherwise no effect.
           BleedBox and ArtBpx has no effect either in printer or in viewer
        */
        if (!new_page.paper.getFromObject(pages->dict->get("MediaBox"), obj_table)) {
            // Page does not have this entry, means it has to be inherited from Parent Pages node
            new_page.paper = this->paper;
        }
        // in a pdfviewer, the visible page size is the CropBox
        if (new_page.bbox.getFromObject(pages->dict->get("CropBox"), obj_table)){
            new_page.bbox_is_cropbox = true;
        }
        else if (!new_page.bbox.getFromObject(pages->dict->get("TrimBox"), obj_table)){
            if (not this->bbox.isZero()) {
                new_page.bbox = this->bbox;
                new_page.bbox_is_cropbox = this->bbox_is_cropbox;
            }
            else
                new_page.bbox = new_page.paper;
        }
        if (not repair_mode)
            pages->dict->filter(page_filter);
        new_page.major = major;
        new_page.minor = minor;
        new_page.doc = this;
        page_list.append(new_page);
        return true;
    }
    message(FATAL,"PdfDocument::getPdfPages : Object isn't Page or Pages");
    return false;
}

#define NODE_MAX 50
/* distribute the pages in a tree, so that each Pages node contains maximum of
 50 childs (Page objects or Pages nodes). So, if document contains 100 pages,
 at first run it creates 50 Pages nodes, it is run once again recursively and
 create a single Pages node that contains previous 50 nodes
  @arg nodes -> array of object numbers of pages
  @arg pages_count -> count of members in nodes array
*/
static int makePagesTree(int *nodes, int pages_count, ObjectTable &obj_table)
{
    PdfObject *node, *page, *kids, *pobj, *count_obj;
    int nodes_count, count, major=0;
    // calculate how many nodes we need to contain all pages
    nodes_count = (pages_count/NODE_MAX)+((pages_count%NODE_MAX)?1:0);
    for (int i=0; i<nodes_count; ++i){
        // create new Pages node object
        node = new PdfObject();
        node->readFromString("<< /Type /Pages /Count 0 /Kids [ ]  /Parent 0 0 R >>");
        major = obj_table.addObject(node);
        kids = node->dict->get("Kids");
        count = 0;
        for (int j=0; j<NODE_MAX; ++j){
            int pg_num = i*NODE_MAX + j;//index of this child on nodes array
            if (pg_num>=pages_count){
                break;
            }
            page = obj_table[ nodes[pg_num] ].obj;// Page leaf or Pages Node
            if (((pobj=page->dict->get("Count"))!=NULL) && isInt(pobj)){
                count += pobj->integer;
            }
            else{
                count++;
            }
            pobj = page->dict->get("Parent");
            pobj->indirect.major = major;
            pobj->indirect.minor = obj_table[major].minor;

            // add the ref of Page obj to Kids array of Pages node
            pobj = new PdfObject();
            pobj->setType(PDF_OBJ_INDIRECT_REF);
            pobj->indirect.major = nodes[pg_num];
            pobj->indirect.minor = obj_table[ nodes[pg_num] ].minor;
            kids->array->append(pobj);
        }
        count_obj = node->dict->get("Count");
        count_obj->integer = count;
        // add this node to nodes array, so this function can be run recursively
        nodes[i] = major;
    }
    if (nodes_count>1) {
        return makePagesTree(nodes, nodes_count, obj_table);
    }
    if (nodes_count==1) {
        node = obj_table[ nodes[0] ].obj;
        node->dict->deleteItem("Parent");
        return major;
    }
    return -1;//nodes_count==0
}


void PdfDocument:: putPdfPages()
{
    PdfObject *pobj;

    if (page_list.count()<1){
        message(FATAL, "Cannot create PDF with zero pages");
    }
    int *nodes = (int*) malloc2(sizeof(int) * page_list.count());
    int count=0;
    // store major nums in nodes array, which is required for creating pages tree
    for (auto page=page_list.begin(); page!=page_list.end(); page++,count++){
        nodes[count] = page->major;
        pobj = obj_table.getObject(page->major, page->minor);
        // set paper size and bbox size in Page Dict
        page->paper.setToObject(pobj->dict->newItem("MediaBox"));

        if (!page->bbox.isZero()) {
            const char *bbox_name = page->bbox_is_cropbox ? "CropBox" : "TrimBox";
            page->bbox.setToObject(pobj->dict->newItem(bbox_name));
        }
    }
    makePagesTree(nodes, count, obj_table);

    // get catalog object from trailer,
    pobj = trailer->dict->get("Root");
    pobj = obj_table[pobj->indirect.major].obj;
    // get Pages node obj from catalog
    pobj = pobj->dict->get("Pages");
    // set reference of Pages Node to root of pages tree
    pobj->indirect.major = nodes[0];
    pobj->indirect.minor = obj_table[ nodes[0] ].minor;
    free(nodes);
}

bool PdfDocument:: save (const char *filename)
{
    PdfObject *pobj;
    FILE *f = stdout;

    if (strcmp(filename,"-")!=0){
        f = fopen(filename,"wb");
        if (f==NULL){
            message(ERROR, "Cannot open for writing file '%s'",filename);
            return false;
        }
    }
    // write header
    fprintf(f, "%%PDF-%d.%d\n", v_major, v_minor);
    // second line of file should contain at least 4 non-ASCII characters in
    char binary[] = {(char)0xDE,(char)0xAD,' ',(char)0xBE,(char)0xEF,'\n',0};
    fprintf(f, binary);
    // build Pages tree, and insert root Pages node in Catalog
    applyTransformations();// apply transformation matrix of all pages
    putPdfPages();
    deleteUnusedObjects(*this);//remove unused objects from object table
    obj_table.writeObjects(f);
    // write cross reference table
    long xref_poz = ftell(f);
    obj_table.writeXref(f);
    // write trailer dictionary
    fprintf(f, "trailer\n");
    pobj = trailer->dict->get("Size");
    pobj->integer = obj_table.count();
    trailer->write(f);
    // startxref, xref offset, and %%EOF must be in three separate lines
    fprintf(f, "\nstartxref\n%ld\n%%%%EOF\n", xref_poz);
    fclose(f);
    return true;
}

// insert parameter doc structure into current doc structure
void
PdfDocument:: mergeDocument(PdfDocument &doc)
{
    int offset = obj_table.count();
    // new obj_table size is one less than size of the two tables.
    // because, we dont need to copy first item of the second obj_table.
    obj_table.expandToFit(obj_table.count()+doc.obj_table.count()-1);

    for (int i=1; i<doc.obj_table.count(); ++i){
        doc.obj_table[i].major = offset+i-1;
        doc.obj_table[i].minor = 0;
    }
    updateRefs(doc);

    for (auto &page : doc.page_list) {
        page.doc = this;
        page_list.append(page);
    }
    for (int i=1; i<doc.obj_table.count(); i++) {
        ObjectTableItem item = doc.obj_table[i];
        obj_table[item.major] = item;
    }
    doc.obj_table.table.clear();
}


// flag used=1 for used/referenced objects
// flag 1 objects will not be deleted during write
static void flag_used_objects (PdfObject *obj, ObjectTable &table)
{
    switch (obj->type){
        case PDF_OBJ_BOOL:
        case PDF_OBJ_INT:
        case PDF_OBJ_REAL:
        case PDF_OBJ_STR:
        case PDF_OBJ_NAME:
        case PDF_OBJ_NULL:
            return;
        case PDF_OBJ_ARRAY:
            for (auto it : *obj->array) {
                flag_used_objects(it, table);
            }
            return;
        case PDF_OBJ_DICT:
            for (auto it : *obj->dict){
                flag_used_objects(it.second, table);
            }
            return;
        case PDF_OBJ_STREAM:
            for (auto it : obj->stream->dict){
                flag_used_objects(it.second, table);
            }
            return;
        case PDF_OBJ_INDIRECT_REF:
            if (table[obj->indirect.major].used){
                return;
            }
            if (table[obj->indirect.major].obj==NULL){
                // in some bad pdfs even if the object is free, the object is referenced
                debug("warning : referencing free obj : %d %d R", obj->indirect.major, obj->indirect.minor);
                obj->type = PDF_OBJ_NULL;
                return;
            }
            table[obj->indirect.major].used = true;
            flag_used_objects(table[obj->indirect.major].obj, table);
            return;
        default:
            assert(0);
    }
}

// Replace old references with new references of same object
static void update_obj_ref(PdfObject *obj, ObjectTable &table)
{
    switch (obj->type){
        case PDF_OBJ_BOOL:
        case PDF_OBJ_INT:
        case PDF_OBJ_REAL:
        case PDF_OBJ_STR:
        case PDF_OBJ_NAME:
        case PDF_OBJ_NULL:
            return;
        case PDF_OBJ_ARRAY:
            for (auto it : *obj->array) {
                update_obj_ref(it, table);
            }
            return;
        case PDF_OBJ_DICT:
            for (auto it : *obj->dict){
                update_obj_ref(it.second, table);
            }
            return;
        case PDF_OBJ_STREAM:
            for (auto it : obj->stream->dict){
                update_obj_ref(it.second, table);
            }
            return;
        case PDF_OBJ_INDIRECT_REF:
            obj->indirect.minor = table[obj->indirect.major].minor;
            obj->indirect.major = table[obj->indirect.major].major;
            return;
        default:
            assert(0);
    }
}

static void updateRefs(PdfDocument &doc)
{
    for (auto& page : doc.page_list) {// page.minor must be set before page.major
        page.minor = doc.obj_table[page.major].minor;
        page.major = doc.obj_table[page.major].major;
    }
    update_obj_ref(doc.trailer, doc.obj_table);
    for (int i=1; i<doc.obj_table.count(); i++) {// obj 0 may be nonfree in bad pdfs, causing segfault
        if (doc.obj_table[i].type) {
            update_obj_ref(doc.obj_table[i].obj, doc.obj_table);
        }
    }
}

/* first mark as unused for all objects in object table recursively. Then recursively
 find used objects in trailer dictionary. Then delete unused objects and change
 object id (major) of objects, so that there is no free objects.
*/
static void deleteUnusedObjects(PdfDocument &doc)
{
    // flag all objects as unused
    for (int i=0; i<doc.obj_table.count(); ++i){
        doc.obj_table[i].major = i;
        doc.obj_table[i].used = false;
    }
    // flag used=1 for used objects
    flag_used_objects(doc.trailer, doc.obj_table);
    // delete unused objects
    for (int i=1,major=1; i<doc.obj_table.count(); ++i){
        if (doc.obj_table[i].used){
            doc.obj_table[i].type = 1;
            doc.obj_table[i].major = major;
            //printf("%d %d\n", i, major);
            ++major;
        }
        else if (doc.obj_table[i].type!=0){ // obj loaded in memory but not used
            doc.obj_table[i].type = 0;
            delete doc.obj_table[i].obj;
            doc.obj_table[i].obj = NULL;
        }
    }
    updateRefs(doc);
    // copy ObjectTable items from old position to new position, and shrink obj_table
    int max_obj_no = 0;
    for (int i=1; i<doc.obj_table.count(); ++i){
        if (doc.obj_table[i].used){
            if (i!=doc.obj_table[i].major){
                memcpy(&doc.obj_table[doc.obj_table[i].major], &doc.obj_table[i], sizeof(ObjectTableItem));
            }
            max_obj_no = doc.obj_table[i].major;
        }
    }
    doc.obj_table.table.resize(max_obj_no+1);
    doc.obj_table[0].offset = 0;
}

void
PdfDocument:: applyTransformations()
{
    for (auto &page : page_list) {
        page.applyTransformation();
    }
}

/********************* ------------------------- ***********************
                            Document Editing
________________________________________________________________________*/
// if the page_num is -1, blank page is added at the end
bool
PdfDocument:: newBlankPage(int page_num)
{
    if (page_num==-1) {
        page_num = page_list.count()+1;
    }
    else if (page_num < 1 and page_num > (page_list.count()+1)) {
        message(WARN, "newBlankPage() : invalid page num %d", page_num);
        return false;
    }
    PdfObject *page, *content;

    page = new PdfObject();
    // to set this page as compressed=false, Resources dict must be present (even empty)
    assert(page->readFromString("<< /Type /Page /Parent 3 0 R /Resources << >> \n >> "));
    // create an empty stream object and add to obj_table, then use it as content stream
    content = new PdfObject();
    content->setType(PDF_OBJ_STREAM);
    int major = obj_table.addObject(content);

    content = page->dict->newItem("Contents");
    content->setType(PDF_OBJ_INDIRECT_REF);
    content->indirect.major = major;
    content->indirect.minor = obj_table[major].minor;

    major = obj_table.addObject(page);
    PdfPage p_page;
    p_page.major = major;
    p_page.minor = obj_table[major].minor;
    p_page.compressed = false;
    p_page.doc = this;
    int ref_page_num = page_num;// if odd page and not last page, use next page size
    // if new page is last page or page no. is even, use prev page size
    if (page_num > page_list.count() or page_num%2==0)
        ref_page_num = page_num-1;
    // use the same cropbox and papersize as reference page
    p_page.paper = page_list[ref_page_num-1].paper;
    p_page.bbox = page_list[ref_page_num-1].pageSize();
    p_page.bbox_is_cropbox = page_list[ref_page_num-1].bbox_is_cropbox;

    if (page_num > page_list.count()) {
        page_list.append(p_page);
    }
    else {
        page_list.array.emplace(page_list.array.begin()+(page_num-1), p_page);//c++11
    }
    return true;
}

Font
PdfDocument:: newFontObject(const char *font_name)
{
    Font font;
    if (font_name == NULL){
        font_name = "Helvetica";
    }
    else if (standard_fonts.count(font_name)==0) {
        message(LOG, "'%s' is not a standard font, using Helvetica Font instead", font_name);
        font_name = "Helvetica";
    }
    char *str;
    asprintf(&str,"<< /Type /Font /Subtype /Type1 /BaseFont /%s /Name /F%s /Encoding /MacRomanEncoding >>",font_name, font_name);
    PdfObject *font_obj = new PdfObject();
    assert(font_obj->readFromString(str));
    free(str);
    font.major = obj_table.addObject(font_obj);
    font.minor = obj_table[font.major].minor;
    font.name = font_name;
    return font;
}

static void pdf_stream_prepend(PdfObject *stream, const char *str, int len)
{
    char *new_stream = (char*) malloc2(len + stream->stream->len);
    memcpy(new_stream, str, len);
    if (stream->stream->len!=0) {
        memcpy(new_stream+len, stream->stream->stream, stream->stream->len);
        free(stream->stream->stream);
    }
    stream->stream->stream = new_stream;
    stream->stream->len += len;
}

// get a stream object and append a char stream to it
static void pdf_stream_append(PdfObject *stream, const char *str, int len)
{
    int old_len = stream->stream->len;
    stream->stream->len += len;
    stream->stream->stream = (char*) realloc(stream->stream->stream, stream->stream->len);
    if (stream->stream->stream==NULL)
        message(FATAL, "realloc() failed !");
    char *ptr = stream->stream->stream + old_len;
    memcpy(ptr, str, len);
}

/*
 takes a content stream obj and a page obj, creates a new xobject, then copy the
 content stream and resources of page to the xobject
 contents = a content stream (stream obj)
 contents must be direct or indirect stream obj
*/
static int
stream_to_xobj (PdfObject *contents, PdfObject *page, Rect &bbox, ObjectTable &obj_table)
{
    PdfObject * xobj, *tmp, *pg_res, *xobj_res;

    while (isRef(contents)){
        contents = obj_table.getObject(contents->indirect.major, contents->indirect.minor);
    }
    assert(isStream(contents));

    xobj = new PdfObject();
    xobj->copyFrom(contents);

    tmp = new PdfObject;
    tmp->readFromString("<< /Type /XObject /Subtype /Form /FormType 1 >>");
    bbox.setToObject(tmp->dict->newItem("BBox"));
    xobj->stream->dict.merge(tmp->dict);
    delete tmp;
    // copy page resources to xobject resources
    pg_res = derefObject(page->dict->get("Resources"), obj_table);

    if (pg_res!=NULL){
        assert(pg_res->type==PDF_OBJ_DICT);
        xobj_res = xobj->stream->dict.newItem("Resources");
        xobj_res->copyFrom(pg_res);
    }
    xobj->stream->dict.filter(xobject_filter);
    return obj_table.addObject(xobj);
}

/* first create a new page object, and add this to object table. get old page contents,
create new XObject using the contents, and add it to object table.
*/
static void pdf_page_to_xobj (PdfPage *page)
{
    PdfDocument *doc = page->doc;
    int major;
    PdfObject *new_page, *new_page_contents, *new_page_xobject,
                *contents, *cont, *pg, *xobj, *xobj_val;
    char * xobjname;
    char * stream_content = NULL;
    static int revision = 1;
    if (not page->compressed)// we have already converted to xobj, nothing to do
        return;

    //get_page_object
    pg = doc->obj_table.getObject(page->major, page->minor);

    // create new_page object
    new_page = new PdfObject();
    new_page->readFromString("<</Type /Page /Contents 0 0 R  /Parent 0 0 R /Resources << /ProcSet [ /PDF ]  /XObject <<  >> >> >>");
    //add new page to xref table
    major = doc->obj_table.addObject(new_page);
    page->major = major;
    page->minor = doc->obj_table[major].minor;
    page->compressed = false;

    new_page_xobject = new_page->dict->get("Resources")->dict->get("XObject");

    cont = derefObject(pg->dict->get("Contents"), doc->obj_table);// it may be null

    if (isStream(cont)){
        major = stream_to_xobj(cont, pg, page->bbox, doc->obj_table);
        asprintf(&xobjname, "xo%d", revision++);
        xobj_val = new_page_xobject->dict->newItem(xobjname);

        xobj_val->setType(PDF_OBJ_INDIRECT_REF);
        xobj_val->indirect.major = major;
        xobj_val->indirect.minor = doc->obj_table[major].minor;

        asprintf(&stream_content,"q /%s Do Q", xobjname);
        free(xobjname);
    }
    else if (isArray(cont)) {
        // array contains indirect objects of streams. Join all streams
        // to create a single merged stream. create xobject from that stream
        if (cont->array->count()==0){// if empty array, nothing to do
            goto empty_cont;
        }
        PdfObject *tmp_stream = NULL;
        PdfObject *new_stream = new PdfObject;
        new_stream->setType(PDF_OBJ_STREAM);

        for (auto it = cont->array->begin(); it!=cont->array->end(); it++)
        {
            tmp_stream = derefObject((*it), doc->obj_table);//decompressed stream
            if (not tmp_stream->stream->decompress() ){
                message(FATAL, "Can not decompress content stream");
            }
            pdf_stream_append(new_stream, " ", 1);
            pdf_stream_append(new_stream, tmp_stream->stream->stream,
                                            tmp_stream->stream->len);
        }
        major = stream_to_xobj(new_stream, pg, page->bbox, doc->obj_table);

        xobj = doc->obj_table.getObject(major, doc->obj_table[major].minor);
        assert( xobj->stream->compress("FlateDecode") );
        // each time different xobject rev numbers are used, so that we can
        // join content streams of two pages without conflict
        asprintf(&xobjname, "xo%d", revision++);
        xobj_val = new_page_xobject->dict->newItem(xobjname);

        xobj_val->setType(PDF_OBJ_INDIRECT_REF);
        xobj_val->indirect.major = major;
        xobj_val->indirect.minor = doc->obj_table[major].minor;

        asprintf(&stream_content,"q /%s Do Q", xobjname);
        free(xobjname);
    }
    else {
        message(WARN, "Page contents is neither stream nor array obj");
empty_cont:
        asprintf(&stream_content, " ");
    }
    //create content stream for new page
    contents = new PdfObject();
    contents->setType(PDF_OBJ_STREAM);
    contents->stream->len = strlen(stream_content);
    contents->stream->stream = stream_content;
    // add content stream to object table
    major = doc->obj_table.addObject(contents);

    new_page_contents = new_page->dict->get("Contents");
    new_page_contents->indirect.major = major;
    new_page_contents->indirect.minor = doc->obj_table[major].minor;
}


// *************----------- PdfPage Object -------------*************

// this constructor is called in getPdfPages() and newBlankPage() function
PdfPage:: PdfPage()
{
    major = 0;
    compressed = true;
    bbox_is_cropbox = false;
    doc = NULL;
}

Rect
PdfPage:: pageSize()
{
    if (bbox_is_cropbox)
        return bbox;
    return paper;
}

void
PdfPage:: drawLine (Point begin, Point end, float width)
{
    PdfObject *page_obj, *cont;
    char *cmd;

    asprintf(&cmd, "\nq %g w %g %g m %g %g l S Q", width, begin.x, begin.y, end.x, end.y);
    applyTransformation();
    // convert to xobject so that drawing commands can be appended
    pdf_page_to_xobj(this);
    page_obj = doc->obj_table.getObject(this->major, this->minor);
    // create new stream by joining page stream and line drawing commands
    cont = page_obj->dict->get("Contents");
    cont = doc->obj_table.getObject(cont->indirect.major, cont->indirect.minor);
    pdf_stream_append(cont, cmd, strlen(cmd));
    free(cmd);
}

/* when used as resources, F is prepended before font name.
 if font.name is "Helvetica", its name used in resource dict is FHelvetica.
 As only base fonts are used, we can safely merge font dictionary and content stream.
 That means if the page contains /FHelvetica already, and we are drawing text using
 Helvetica fonts, this will replace and add the same /FHelvetica item, and both font
 objects will be identical.
*/
void
PdfPage:: drawText (const char *text, Point &pos, int size, Font font)
{
    char *str;
    PdfObject *font_obj, *page, *stream, *cont, *res, *font_dict;

    applyTransformation();
    pdf_page_to_xobj(this);
    page = doc->obj_table.getObject(this->major, this->minor);
    // /Resources << /Font << /FHelvetica 4 0 R >> XObject <</xo1 5 0 R >> >>
    res = page->dict->get("Resources");
    font_dict = res->dict->get("Font");
    if (not font_dict) {
        font_dict = res->dict->newItem("Font");
        font_dict->setType(PDF_OBJ_DICT);
    }
    asprintf(&str, "F%s", font.name);
    font_obj = font_dict->dict->newItem(str);
    font_obj->setType(PDF_OBJ_INDIRECT_REF);
    font_obj->indirect.major = font.major;
    font_obj->indirect.minor = font.minor;
    free(str);

    cont = page->dict->get("Contents");
    stream = doc->obj_table.getObject(cont->indirect.major, cont->indirect.minor);
    // we dont want trailing zeros in a float, so we used %g instead of %f
    asprintf(&str, "\nq BT /F%s %d Tf  %g %g Td  (%s) Tj ET Q", font.name, size, pos.x, pos.y, text);
    pdf_stream_append(stream, str, strlen(str));
    free(str);
}

void
PdfPage:: crop (Rect box)
{
    PdfObject *page_obj, *cont;
    char *cmd;

    asprintf(&cmd, "q %g %g %g %g re W n\n", box.left.x, box.left.y, box.right.x-box.left.x, box.right.y-box.left.y);
    applyTransformation();
    // convert to xobject so that drawing commands can be appended
    pdf_page_to_xobj(this);
    page_obj = doc->obj_table.getObject(this->major, this->minor);
    // create new stream by joining page stream and crop commands
    cont = page_obj->dict->get("Contents");
    cont = doc->obj_table.getObject(cont->indirect.major, cont->indirect.minor);
    pdf_stream_prepend(cont, cmd, strlen(cmd));
    pdf_stream_append(cont, " Q", 2);
    free(cmd);
}

void
PdfPage:: mergePage (PdfPage &p2)
{
    PdfObject *page1, *page2, *res1, *res2, *cont, *stream1, *stream2;

    applyTransformation();
    p2.applyTransformation();
    pdf_page_to_xobj(this);
    pdf_page_to_xobj(&p2);

    page1 = doc->obj_table.getObject(this->major, this->minor);
    page2 = doc->obj_table.getObject(p2.major, p2.minor);
    //page2->write(stdout);
    // pages has been already converted to xobject. So xobjects and fonts are the
    // only resources of page objects. No two different XObjects or Fonts have
    // same name. So we can merge the Resources dicts safely
    res1 = page1->dict->get("Resources");
    res2 = page2->dict->get("Resources");
    //res2->write(stdout);
    res1->dict->merge(res2->dict);

    cont = page1->dict->get("Contents");
    stream1 = doc->obj_table.getObject(cont->indirect.major, cont->indirect.minor);
    cont = page2->dict->get("Contents");
    stream2 = doc->obj_table.getObject(cont->indirect.major, cont->indirect.minor);

    pdf_stream_append(stream1, " ", 1);
    pdf_stream_append(stream1, stream2->stream->stream, stream2->stream->len);
}

/* Apply the transformation matrix in PdfPage if the matrix is not unity matrix
 transform_page() must be called before save doc, draw line, draw text and crop page.
*/
void
PdfPage:: applyTransformation()
{
    if (this->matrix.isIdentity()) {
        return;
    }
    PdfObject *page_obj, *stream;
    char *str;

    page_obj = doc->obj_table.getObject(this->major, this->minor);

    stream = page_obj->dict->get("Contents");
    stream = doc->obj_table.getObject(stream->indirect.major, stream->indirect.minor);
    if (stream->stream->len==0){
        return;
    }
    asprintf(&str, "q %s %s %s %s %s %s cm\n",
            double2str(matrix.mat[0][0]).c_str(), double2str(matrix.mat[0][1]).c_str(),
            double2str(matrix.mat[1][0]).c_str(), double2str(matrix.mat[1][1]).c_str(),
            double2str(matrix.mat[2][0]).c_str(), double2str(matrix.mat[2][1]).c_str() );

    pdf_stream_prepend(stream, str, strlen(str));
    pdf_stream_append(stream, " Q", 2);
    free(str);

    Matrix identity_matrix;
    this->matrix = identity_matrix;
}

/*  transforms page content, bounding box and paper using the given matrix */
void
PdfPage:: transform (Matrix mat)
{
    pdf_page_to_xobj(this);
    // transform page content
    matrix.multiply(mat);
    // transform bounding box & paper
    mat.transform(bbox);
    mat.transform(paper);
}


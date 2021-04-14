#pragma once
/* This file is a part of pdfcook program, which is GNU GPLv2 licensed */
#include "pdf_objects.h"
#include "geometry.h"
#include <cassert>

class PdfDocument;

typedef struct
{
    const char *name;
    int major;
    int minor;
} Font;

void print_font_names();

class PdfPage
{
public:
    Rect paper;
    Rect bbox;
    bool bbox_is_cropbox;
    Matrix matrix;
    int major;// of Page Object
    int minor;
    bool compressed;// converted to xobject
    PdfDocument *doc;

    PdfPage();
    Rect pageSize();// get CropBox or MediaBox
    void drawLine (Point begin, Point end, float width);
    void drawText (const char *text, Point &pos, int size, Font font);
    void crop (Rect box);
    void mergePage (PdfPage &p2);
    void transform (Matrix mat);
    void applyTransformation ();
    //void duplicateContent();
};

typedef std::vector<PdfPage>::iterator PageIter;

class PageList
{
public:
    std::vector<PdfPage> array;

    int count();
    void append(PdfPage &page);
    void remove(int index);// page index val is one less than page number
    void clear();
    // allows range based for-loop
    PageIter begin();
    PageIter end();
    // allows indexing operator
    PdfPage& operator[] (int index)
    {
        assert( index>=0 && index<(int)array.size() );
        return array[index];
    }
};

class PdfDocument
{
public:
    int v_major;
    int v_minor;
    Rect paper;
    Rect bbox;
    bool bbox_is_cropbox;
    //List of PdfPage
    PageList page_list;
    ObjectTable obj_table;
    PdfObject *trailer;

    PdfDocument();
    ~PdfDocument();

    bool getPdfHeader (MYFILE *f, char *line);
    bool getPdfTrailer (MYFILE *f, char *line, long offset);
    bool getAllPages (MYFILE *f);
    bool getPdfPages (MYFILE *f, int major, int minor);
    bool open (const char *fname);
    void mergeDocument(PdfDocument &doc);

    void putPdfPages();
    bool save (const char *filename);

    Font newFontObject(const char *font);
    bool newBlankPage(int page_num);
    void applyTransformations();
};

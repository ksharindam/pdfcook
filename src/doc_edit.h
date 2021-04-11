#pragma once
/* This file is a part of pdfcook program, which is GNU GPLv2 licensed */
#include "pdf_doc.h"
#include <list>

typedef enum {
    PAGE_SET_ALL,
    PAGE_SET_ODD,
    PAGE_SET_EVEN,
    PAGE_SET_RANGE
} PageSetType;


class PageRange
{
public:
    PageSetType type;
    int16_t begin;
    int16_t end;
    bool negative;

    PageRange ();
    PageRange (PageSetType _type);
    PageRange (int begin, int end, bool neg);
};

typedef std::vector<int>::iterator PageNumIter;

class PageRanges {
public:
    std::list<PageRange> array;
    std::vector<int> page_num_array;

    void append(PageRange range);
    // convert list of PageRange to list of page numbers
    void initPageNums(int max_page_num);
    void sort();
    void clear();
    PageNumIter begin();
    PageNumIter end();
};


bool doc_pages_transform(PdfDocument &doc, PageRanges &pages, Matrix mat);

bool doc_pages_scaleto (PdfDocument &doc, PageRanges &pages, Rect paper,
                        float top, float right, float bottom, float left);

bool doc_pages_translate(PdfDocument &doc, PageRanges &pages, float x, float y);

bool doc_pages_delete(PdfDocument &doc, PageRanges &pages);

bool doc_pages_arrange(PdfDocument &doc, PageRanges &pages);

bool doc_pages_number(PdfDocument &doc, PageRanges &pages,
                    int x, int y, int start, const char *text, int size, const char *font);
bool doc_pages_text(PdfDocument &doc, PageRanges &pages,
                    int x, int y, const char *text, int size, const char *font);

bool doc_pages_crop (PdfDocument &doc, PageRanges &pages, Rect box);

bool doc_pages_set_paper_size (PdfDocument &doc, PageRanges &pages, Rect paper);

bool add_new_paper_size (std::string name, float w, float h);

typedef enum {
    ORIENT_AUTO,
    ORIENT_PORTRAIT,
    ORIENT_LANDSCAPE
} Orientation;

bool set_paper_from_name(Rect &paper, std::string name, Orientation orientation);
void paper_set_orientation (Rect &paper, Orientation orientation);
void print_paper_sizes();

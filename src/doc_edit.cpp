/* This file is a part of pdfcook program, which is GNU GPLv2 licensed */
#include "doc_edit.h"
#include "debug.h"
#include "common.h"
#include <algorithm>// sort()

PageRange:: PageRange () {
    type = PAGE_SET_ALL;
    begin = 1; end = -1; negative = false;
}
PageRange:: PageRange (PageSetType _type) : PageRange() {
     type = _type;
}
PageRange:: PageRange (int begin, int end, bool neg) : begin(begin), end(end), negative(neg) {
    type = PAGE_SET_RANGE;
}

//------------------ List of PageRange object -------------------------

void PageRanges:: append (PageRange range) {
    array.push_back(range);
}

void PageRanges:: initPageNums (int max_page_num)
{
    for (PageRange range : array) {
        switch (range.type) {
        case PAGE_SET_RANGE:
            if (range.negative) {
                range.begin *= -1;
                range.end *= -1;
            }
            if (range.begin < 0) {// converts -1 to last page no
                range.begin = max_page_num + range.begin + 1;
            }
            if (range.end < 0) {
                range.end = max_page_num + range.end + 1;
            }
            if (range.begin <= range.end) {// eg. -> 1..5 or 4..4
                for (int i=range.begin; i<=range.end /*&& i<=max_page_num*/; i++)
                    page_num_array.push_back(i);
            }
            else if (range.begin<=max_page_num) {// eg. -> 5..1
                for (int i=range.begin; i>=range.end; i--)
                    page_num_array.push_back(i);
            }
            break;
        case PAGE_SET_ODD:
            for (int i=1; i<=max_page_num; i+=2)
                page_num_array.push_back(i);
            break;
        case PAGE_SET_EVEN:
            for (int i=2; i<=max_page_num; i+=2)
                page_num_array.push_back(i);
            break;
        case PAGE_SET_ALL:
        default:
            for (int i=1; i<=max_page_num; i++)
                page_num_array.push_back(i);
        }
    }
}

static bool sort_func (int i, int j) { return i<j; }

void PageRanges:: sort()
{
    std::sort(page_num_array.begin(), page_num_array.end(), sort_func);
}

void PageRanges:: clear() {
    array.clear();
    page_num_array.clear();
}

PageNumIter PageRanges:: begin() {
    return page_num_array.begin();
}
PageNumIter PageRanges:: end() {
    return page_num_array.end();
}


bool doc_pages_delete (PdfDocument &doc, PageRanges &pages)
{
    pages.sort();
    int deleted = 0;
	for (int page_num : pages) {
        doc.page_list.remove(page_num-1-deleted);
        deleted++;
    }
    return true;
}

bool doc_pages_arrange (PdfDocument &doc, PageRanges &pages)
{
    PageList pg_list = doc.page_list;//copies page list
    doc.page_list.clear();
    for (int page_num : pages) {
        if (page_num > pg_list.count())
            return false;
        PdfPage page = pg_list[page_num-1];
        doc.page_list.append(page);
    }
    return true;
}

bool doc_pages_number (PdfDocument &doc, PageRanges &pages,
                    int x, int y, int start, const char *text, int size, const char *font_name)
{
    start -= 1;// this will help to calc page number to print
	Point poz;
    char *str;
	// make sure that given text contains a %d, which is replaced by page number
    const char *p1 = strstr(text, "%d");// find %d in the given text
    const char *p2 = strstr(text, "%");// first % is followed by d
    if (p1==NULL || p1!=p2){
        message(ERROR, "text does not contain %%d");
        return false;
    }
    p2 = strstr(p1+1, "%");// no other % character in string
    if (p2!=NULL){
        return false;
    }
    Font font = doc.newFontObject(font_name);

	for (int page_num : pages) {
        PdfPage &page = doc.page_list[page_num-1];// page index = page_num -1
        Rect page_size = page.pageSize();
        poz.x = (x!=-1) ? page_size.left.x +x :
                        page_size.left.x + (page_size.right.x - page_size.left.x)/2;
        poz.y = (y!=-1) ? page_size.left.y + y : page_size.left.y + size+10;
		asprintf(&str, text, start+page_num);
        if (start+page_num>0)
		    page.drawText(str, poz, size, font);
		free(str);
	}
	return true;
}

bool doc_pages_text (PdfDocument &doc, PageRanges &pages,
                    int x, int y, const char *text, int size, const char *font_name)
{
	Point poz;
    Font font = doc.newFontObject(font_name);

	for (int page_num : pages) {
        PdfPage &page = doc.page_list[page_num-1];// page index = page_num -1
        Rect page_size = page.pageSize();
        poz.x = page_size.left.x + x;
        poz.y = page_size.left.y + y;
        page.drawText(text, poz, size, font);
	}
	return true;
}

bool doc_pages_crop (PdfDocument &doc, PageRanges &pages, Rect crop_area)
{
	for (int page_num : pages){
        PdfPage &page = doc.page_list[page_num-1];
        page.crop(crop_area);
    }
    return true;
}

bool doc_pages_transform(PdfDocument &doc, PageRanges &pages, Matrix mat)
{
	for (int page_num : pages) {
        PdfPage &page = doc.page_list[page_num-1];// page index = page_num -1
        page.transform(mat);
    }
    return true;
}

bool doc_pages_translate(PdfDocument &doc, PageRanges &pages, float x, float y)
{
    Rect paper;

	Matrix  matrix;
	matrix.translate(x,y);

	for (int page_num : pages) {
        PdfPage &page = doc.page_list[page_num-1];
        Rect page_size = page.pageSize();
        // this transforms page content, bbox and paper size
        page.transform(matrix);
        // we dont want to transform paper so restoring it
        page.paper = page_size;
        page.bbox_is_cropbox = false;// if cropbox is not removed, translation wont be seen
	}
	return true;
}

bool doc_pages_scaleto (PdfDocument &doc, PageRanges &pages, Rect paper,
                        float top, float right, float bottom, float left)//margins
{
	double scale, scale_x, scale_y;
	double move_x, move_y;
	double old_page_w, old_page_h, avail_w, avail_h;

	Rect bbox = paper;
	bbox.right.x -= right;
	bbox.right.y -= top;
	bbox.left.x  += left;
	bbox.left.y  += bottom;
    // available width and height inside margin
	avail_w = bbox.right.x - bbox.left.x;
	avail_h = bbox.right.y - bbox.left.y;

	for (int page_num : pages){
        PdfPage &page = doc.page_list[page_num-1];
        Rect page_size = page.pageSize();
        // using paper size instead of bounding box size, because viewers show paper
        // size as page size, and you dont see the bounding box rect in a viewer
        old_page_w = page_size.right.x - page_size.left.x;
        old_page_h = page_size.right.y - page_size.left.y;
        // get scale value to fit inside margin of new page
        scale_x = avail_w / old_page_w;
        scale_y = avail_h / old_page_h;
        scale = MIN(scale_x, scale_y);
        // adjust for new margin
        move_x =  bbox.left.x;
        move_y =  bbox.left.y;
        // adjust to fit center
        move_x +=  (avail_w - (scale * old_page_w)) / 2;
        move_y +=  (avail_h - (scale * old_page_h)) / 2;
        // adjust in case, old paper bottom left is not (0,0)
        // as the old page is scaled, dimension is also scaled
        move_x -= scale*page_size.left.x;
        move_y -= scale*page_size.left.y;

        Matrix matrix;
        matrix.scale(scale);
        matrix.translate(move_x, move_y);

        page.transform(matrix);
		page.paper = paper;
        page.bbox_is_cropbox = false;
	}
    return true;
}

bool doc_pages_set_paper_size (PdfDocument &doc, PageRanges &pages, Rect paper)
{
	for (int page_num : pages){
        PdfPage &page = doc.page_list[page_num-1];
        page.paper = paper;
    }
    return true;
}


typedef struct {
   std::string name;
   float width;
   float height;
} PaperSize;

/* list of supported paper sizes.
 all format names must be in lowercase.
 sizes are in points (1/72 inch)
*/
static std::list<PaperSize> paper_sizes({
    { "a0", 2382, 3369 },   // 84.1cm * 118.8cm
    { "a1", 1684, 2382 },   // 59.4cm * 84.1cm
    { "a2", 1191, 1684 },   // 42cm * 59.4cm
    { "a3",  842, 1191 },	// 29.7cm * 42cm
    { "a4",  595,  842 },	// 21cm * 29.7cm
    { "a5",  421,  595 },   // 14.85cm * 21cm
    { "a6",  297,  420 },	// 10.5cm * 14.85 cm
    { "a7",  210,  297 },	// 7.4cm * 10.5cm
    { "a8",  148,  210 },	// 5.2cm * 7.4cm
    { "a9",  105,  148 },	// 3.7cm * 5.2cm
    { "a10",  73,  105 },	// 2.6cm * 3.7cm
    { "b0", 2835, 4008 },	// 100cm * 141.4cm
    { "b1", 2004, 2835 },	// 70.7cm * 100cm
    { "b2", 1417, 2004 },	// 50cm * 70.7cm
    { "b3", 1001, 1417 },	// 35.3cm * 50cm
    { "b4",  709, 1001 },	// 25cm * 35.3cm
    { "b5",  499,  709 },	// 17.6cm * 25cm
    { "b6",  354,  499 },	// 12.5cm * 17.6cm
    { "jisb0", 2920, 4127 },// 103.0cm * 145.6cm
    { "jisb1", 2064, 2920 },// 72.8cm * 103.0cm
    { "jisb2", 1460, 2064 },// 51.5cm * 72.8cm
    { "jisb3", 1032, 1460 },// 36.4cm * 51.5cm
    { "jisb4",  729, 1032 },// 25.7cm * 36.4cm
    { "jisb5",  516,  729 },// 18.2cm * 25.7cm
    { "jisb6",  363,  516 },// 12.8cm * 18.2cm
    { "c0",  2599, 3677 },	// 91.7cm * 129.7cm
    { "c1",  1837, 2599 },	// 64.8cm * 91.7cm
    { "c2",  1298, 1837 },	// 45.8cm * 64.8cm
    { "c3",   918, 1298 },	// 32.4cm * 45.8cm
    { "c4",   649,  918 },	// 22.9cm * 32.4cm
    { "c5",   459,  649 },	// 16.2cm * 22.9cm
    { "c6",   323,  459 },	// 11.4cm * 16.2cm
    { "ledger",   1224,  792 },	// 17in * 11in
    { "tabloid",   792, 1224 },	// 11in * 17in
    { "letter",    612,  792 },	// 8.5in * 11in
    { "halfletter",396,  612 }, // 5.5in * 8.5in
    { "statement", 396,  612 },	// 5.5in * 8.5in
    { "legal",     612, 1008 },	// 8.5in * 14in
    { "executive", 540,  720 },	// 7.6in * 10in
    { "folio",  612, 936 },	    // 8.5in * 13in
    { "quarto", 610, 780 },	    // 8.5in * 10.83in
    { "10x14", 720, 1008 },	    // 10in * 14in
    { "arche", 2592, 3456 },	// 34in * 44in
    { "archd", 1728, 2592 },	// 22in * 34in
    { "archc", 1296, 1728 },	// 17in * 22in
    { "archb",  864, 1296 },	// 11in * 17in
    { "archa",  648,  864 },	// 8.5in * 11in
    { "flsa",   612,  936 },    // 8.5in * 13in (U.S. foolscap)
    { "flse",   612,  936 }     // 8.5in * 13in (European foolscap)
});

// add user defined paper size
bool add_new_paper_size (std::string name, float w, float h)
{
    transform(name.begin(), name.end(), name.begin(), ::tolower);
    PaperSize new_size = {name, w, h};
    paper_sizes.push_front(new_size);
    return true;
}

bool set_paper_from_name(Rect &paper, std::string name, Orientation orientation)
{
    transform(name.begin(), name.end(), name.begin(), ::tolower);

    for (auto &paper_size : paper_sizes) {
        if (paper_size.name == name) {
            paper.left = Point(0, 0);
            paper.right = Point(paper_size.width, paper_size.height);
            paper_set_orientation(paper, orientation);
            return true;
        }
    }
    return false;
}

void paper_set_orientation (Rect &paper, Orientation orientation)
{
    // switch width & height if landscape is required
    if ( (orientation==ORIENT_PORTRAIT && paper.isLandscape())
          or (orientation==ORIENT_LANDSCAPE && (not paper.isLandscape())) ) {
        paper.right = Point(paper.right.y, paper.right.x);
    }
}

void print_paper_sizes()
{
    for (auto &paper_size : paper_sizes) {
        fprintf(stderr, "%s\n", paper_size.name.c_str());
    }
}

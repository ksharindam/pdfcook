#ifndef _VDOC_H_
#define _VDOC_H_

#include "vdocerror.h"
#include "doc_type.h"
#include "page_list.h"
#include "vldoc.h"
int pages_info(page_list_head * p_doc,FILE * f);
int pages_nup(page_list_head * p_doc,int x,int y, dimensions *bbox,double dx, double dy, int  orientation ,int rotate,int order_by_bbox, int frame, int center, int scale);
int pages_flip(page_list_head * p_doc, int mode1, int mode2);
int pages_scale(page_list_head * p_doc, double scale);
int pages_scaleto(page_list_head * p_doc, dimensions * _paper, double top, double right, double bottom, double left);
int set_paper_orient(page_list_head * p_doc, orientation orient);
int set_paper_size(page_list_head * p_doc, dimensions * dim);
int pages_transform(page_list_head * p_doc, transform_matrix * matrix);
int pages_crop(page_list_head * p_doc, dimensions * dim);
int pages_move_xy(page_list_head * p_doc, double x, double y);
int pages_rotate(page_list_head * p_doc, int angle);	
int pages_line(page_list_head * p_doc,int lx, int ly, int hx, int hy, int width);
int pages_number(page_list_head * p_doc,int x, int y, int start,char * text, char * font, int size);
int pages_text(page_list_head * p_doc,int x, int y,char * text, char * font, int size);

transform_matrix *transform_matrix_move_xy(transform_matrix * what, double x, double y);
transform_matrix *transform_matrix_scale(transform_matrix * what, double scale);
transform_matrix *transform_matrix_rotate(transform_matrix * what, double angle_deg);
transform_matrix * transform_matrix_multi(transform_matrix * a, transform_matrix *b);
void transform_dimensions(dimensions * bbox, transform_matrix * matrix);
void transform_matrix_point(coordinate * point,transform_matrix * matrix);
int pages_cmarks(page_list_head * p_doc, int by_bbox);
int pages_norm(page_list_head * p_doc, int center, int scale, int l_bbox, int g_bbox);



#endif

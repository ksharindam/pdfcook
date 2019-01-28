#include "common.h"
#include "page_list.h"
#include "vdoc.h"


int pages_info(page_list_head * p_doc,FILE * f){
	page_list * page;
	fprintf(f,"Pages Info:\n\n");
	for (page=page_next(page_begin(p_doc));page!=page_end(p_doc);page=page_next(page)){
		fprintf(f, "paper: %5d %5d %5d %5d\n", page->page->paper.left.x,  page->page->paper.left.y, page->page->paper.right.x, page->page->paper.right.y);
		fprintf(f, "bbox:  %5d %5d %5d %5d\n", page->page->bbox.left.x,  page->page->bbox.left.y, page->page->bbox.right.x, page->page->bbox.right.y);
	}
	fprintf(f, "\n");
	return 0;

}

int pages_transform(page_list_head * p_doc, transform_matrix * matrix){
	page_list * page;
	for (page=page_next(page_begin(p_doc));page!=page_end(p_doc);page=page_next(page)){
		doc_page_transform(page,matrix);
	}
	return 0;
}

int pages_scale(page_list_head * p_doc, double scale){
	transform_matrix  scale_matrix = {{1,0,0},{0,1,0},{0,0,1}};
	transform_matrix_scale(&scale_matrix,scale);
	pages_transform(p_doc,&scale_matrix);
	return 0;
}

int set_paper_size(page_list_head * p_doc, dimensions * dim){
	page_list * page;
	copy_dimensions(&(p_doc->doc->paper),dim);
	for (page=page_next(page_begin(p_doc));page!=page_end(p_doc);page=page_next(page)){
		copy_dimensions(&(page->page->paper),dim);
	}
	return 0;
}

int set_paper_orient(page_list_head * p_doc, orientation orient){
	page_list * page;
	p_doc->doc->orient = orient;
	for (page=page_next(page_begin(p_doc));page!=page_end(p_doc);page=page_next(page)){
		page->page->orient = orient;
	}
	return 0;
}



int pages_scaleto(page_list_head * p_doc, dimensions * _paper, double top, double right, double bottom, double left){
	transform_matrix  scale_matrix = {{1,0,0},{0,1,0},{0,0,1}};
	double scale, scale_x, scale_y;
	double move_x,move_y;
	double size_x, size_y, psize_x, psize_y;
	dimensions paper;
	page_list * page;

	copy_dimensions(&paper,_paper);
	paper.right.x-=right;
	paper.right.y-=top;
	paper.left.x+=left;
	paper.left.y+=bottom;

	size_x = paper.right.x - paper.left.x;
	size_y = paper.right.y - paper.left.y;
	psize_x = p_doc->doc->bbox.right.x - p_doc->doc->bbox.left.x;
	psize_y = p_doc->doc->bbox.right.y - p_doc->doc->bbox.left.y;

	scale_x = size_x / psize_x;
	scale_y = size_y / psize_y;

	scale = min(scale_x, scale_y);

	move_x =  ((size_x - (scale * psize_x)) / 2.0  +  paper.left.x); 
	move_y =  ((size_y - (scale * psize_y)) / 2.0 +  paper.left.y); 
	transform_matrix_move_xy(&scale_matrix,move_x,move_y);
	


	transform_matrix_scale(&scale_matrix,scale);
	move_x = - p_doc->doc->bbox.left.x; 
	move_y = - p_doc->doc->bbox.left.y; 
	transform_matrix_move_xy(&scale_matrix,move_x,move_y);


	for (page=page_next(page_begin(p_doc));page!=page_end(p_doc);page=page_next(page)){
		doc_page_transform(page,&scale_matrix);
		copy_dimensions( &( page->page->paper),_paper);
	}

	update_global_dimensions(p_doc);
	
	return 0;
}

int pages_rotate(page_list_head * p_doc, int angle){	
	transform_matrix  matrix = {{1,0,0},{0,1,0},{0,0,1}};
#ifdef MOVE_UP
	transform_matrix  post_matrix = {{1,0,0},{0,1,0},{0,0,1}};
#endif
	int x = p_doc->doc->paper.right.x;
	int y = p_doc->doc->paper.right.y;
	update_global_dimensions(p_doc);
	for(;angle<0;angle+=360);
	switch (angle%360){
		case 180:
			transform_matrix_move_xy(&matrix,x,y);
			break;
		case 90:
			transform_matrix_move_xy(&matrix,0,x);
			break;
		case 270:
			transform_matrix_move_xy(&matrix,y,0);
			break;
	}
	transform_matrix_rotate(&matrix,angle);
	pages_transform(p_doc,&matrix);
	update_global_dimensions(p_doc);
#ifdef MOVE_UP
	/*move up rotated page*/
	if ((p_doc->doc->bbox.left.x<0) || (p_doc->doc->bbox.left.y<0)) {
	
		if (p_doc->doc->bbox.left.x<0){
			transform_matrix_move_xy(&post_matrix,-1 * p_doc->doc->bbox.left.x,0);
			
		}
		if (p_doc->doc->bbox.left.y<0){
			transform_matrix_move_xy(&post_matrix, 0,-1 * p_doc->doc->bbox.left.y);
		}
		pages_transform(p_doc,&post_matrix);
	}
#endif
		/*printf("<d>%d %d %d %d\n",p_doc->doc->bbox.left.x,p_doc->doc->bbox.left.y,p_doc->doc->bbox.right.x,p_doc->doc->bbox.right.y);*/
	return 0;
}

int pages_line(page_list_head * p_doc,int lx, int ly, int hx, int hy, int width){
	page_list * page;
	coordinate l,h;
	l.x=lx;
	l.y=ly;
	h.x=hx;
	h.y=hy;
	for (page=page_next(page_begin(p_doc));page!=page_end(p_doc);page=page_next(page)){
		doc_draw_to_page_line(page, &l, &h,width);
	}
	return 0;
}
int pages_text(page_list_head * p_doc,int x, int y,char * text, char * font, int size){
	page_list * page;
	coordinate poz;
	poz.x=x;
	poz.y=y;

	for(page=page_next(page_begin(p_doc));page!=page_end(p_doc);page=page_next(page)){
		doc_draw_to_page_text(page,&poz,text,size,font);
	}
	return 0;
}
int pages_number(page_list_head * p_doc,int x, int y, int start,char * text, char * font, int size){
	page_list * page;
	coordinate poz;
	char _text[]="%d";
	char * out;
	char *p1,*p2;
	if (text==NULL){
		text=_text;
	}
	else{
		p1=strstr(text,"%d");
		p2=strstr(text,"%");
		if (p1==NULL || p1!=p2){
			return -1;
		}
		p2=strstr(p1 + 1,"%");
		if (p2!=NULL){
			return -1;
		}
	}
	if (x==-1 && y==-1){
		poz.x=(p_doc->doc->bbox.right.x-p_doc->doc->bbox.left.x)/2 + p_doc->doc->bbox.left.x;
		poz.y=size+10;
	}
	else{
		poz.x=x;
		poz.y=y;
	}
	for(page=page_next(page_begin(p_doc));page!=page_end(p_doc);page=page_next(page)){
		asprintf(&out,text,start++);
		doc_draw_to_page_text(page,&poz,out,size,font);
		free(out);
	}
	return 0;
}

int pages_flip(page_list_head * p_doc, int mode1, int mode2){
	transform_matrix  matrix = {{1,0,0},{0,1,0},{0,0,1}};
	switch(mode1){
		case DOC_O_LANDSCAPE:
			matrix[2][1]+=p_doc->doc->paper.right.y;
			matrix[1][1]*=-1;
			break;
		case DOC_O_PORTRAIT:
			matrix[2][0]=+p_doc->doc->paper.right.x;
			matrix[0][0]*=-1;
			break;
		default:
			assert(-1);
			return -1;
	}
	pages_transform(p_doc,&matrix);
	return 0;
}

int pages_crop(page_list_head * p_doc, dimensions * dim){
	page_list * page;
	for (page=page_next(page_begin(p_doc));page!=page_end(p_doc);page=page_next(page)){
		doc_page_crop(page,dim);
	}
	copy_dimensions(&p_doc->doc->bbox,dim);
	update_global_dimensions(p_doc);
	return 0;
}


int draw_frame(page_list_head * p_doc, int x, int y, int width, int type, int start_x, int start_y, int off_x, int off_y, int dx, int dy){
	int i,j;
	coordinate begin;
	coordinate end;

	if (width==0){
		return 0;
	}

	begin.y=start_y - (dy + off_y)*y + dy;
	end.y=start_y;
	for (i=1;i<x;++i){
		begin.x=start_x + off_x*i + dx * (i - 1) + dx/2.0; 
		end.x=begin.x;
		doc_draw_to_page_line(page_next(page_begin(p_doc)),&begin,&end,width);
	}
	begin.x=start_x + (dx + off_x)*x - dx; 
	end.x=start_x;
	for (j=1;j<y;++j){
		begin.y=start_y - off_y*j - dy * (j - 1) - dy/2.0;
		end.y=begin.y;
		doc_draw_to_page_line(page_next(page_begin(p_doc)),&begin,&end,width);
	}
	return 0;
}

int pages_move_xy(page_list_head * p_doc, double x, double y){
	transform_matrix  matrix = {{1,0,0},{0,1,0},{0,0,1}};
	page_list * page;
	transform_matrix_move_xy(&matrix,x,y);
	for (page=page_next(page_begin(p_doc));page!=page_end(p_doc);page=page_next(page)){
		doc_page_transform(page,&matrix);
	}
	return 0;

}

int pages_nup(page_list_head * p_doc,int x,int y, dimensions * bbox,
	double dx, double dy, int  orientation ,int rotate,
	int order_by_bbox, int frame, int center,int en_scale){
	double p_size_x, p_size_y;
	double n_p_size_x, n_p_size_y;
	int i,j,k;
	int loops_count, pages_count;
	double scale_x, scale_y,scale;
	double off_x,off_y;
	double start_x,start_y;
	page_list_head * new_list, *selected_pages;;

	/*MJ's wish*/
	if (y==0){
		switch(x){
		case 1:
			y=1;
			x=1;
			break;
		case 2:
			x=1;
			y=2;
			rotate = rotate?rotate:90;
			break;
		case 4:
			x=2;
			y=2;
			break;
		default:
			{
				int n, k;
				/* k*k ==x*/
				for(k=1;k*k<x;++k);

				if (k*k==x){
					x =k;
					y =k;
					break;
				}
				
				k = 0; 
				n = x;
				do{
					if ( (n & 0x1)){
						if (n != 0x1){
							/*isn't n!=2^k*/
							return -1;
						}
						break;
					}
					if (n==0){
						return -1;
					}
					n = n >> 1;
					++k;
				} while (1);

				if (k%2){
					x = 1<<(k/2);
					y = 1<<(k/2 +1);
					rotate = rotate?rotate:90;
				}
				else{
					x = 1<<(k/2);
					y = 1<<(k/2);
				}
			}
			
		}
	}

	pages_count=pages_count(p_doc);
	loops_count=pages_count/(x*y) + ((pages_count%(x*y))?1:0);

	/*rotace vsech stranek v seznamu o  90 stupnu*/
	if (rotate){
		pages_rotate(p_doc,rotate);
	}
		
	if (en_scale == 0) {
		if (order_by_bbox){
			p_size_x=(p_doc->doc->bbox.right.x-p_doc->doc->bbox.left.x);		
			p_size_y=(p_doc->doc->bbox.right.y-p_doc->doc->bbox.left.y);
		}
		else{
			p_size_x=(p_doc->doc->paper.right.x - p_doc->doc->paper.left.x);
			p_size_y=(p_doc->doc->paper.right.y - p_doc->doc->paper.left.y);
	
		}
		bbox->left.x = 0;
		bbox->left.y = 0;
		bbox->right.x =  p_size_x * x + (x-1)*dx;
		bbox->right.y =  p_size_y * y + (y-1)*dy;
	
	}

	n_p_size_x = bbox->right.x-bbox->left.x;
	n_p_size_y = bbox->right.y-bbox->left.y;
 
	p_size_x = (n_p_size_x - (x-1)*dx)/x;
	p_size_y = (n_p_size_y - (y-1)*dy)/y;
	
	if (order_by_bbox){
		if (en_scale){
			scale_x=p_size_x/(double)(p_doc->doc->bbox.right.x-p_doc->doc->bbox.left.x);
			scale_y=p_size_y/(double)(p_doc->doc->bbox.right.y-p_doc->doc->bbox.left.y);
			scale = min(scale_x,scale_y);
		}
		else {
			scale = 1;
		}
		p_size_x=scale * (double)(p_doc->doc->bbox.right.x-p_doc->doc->bbox.left.x);
		p_size_y=scale * (double)(p_doc->doc->bbox.right.y-p_doc->doc->bbox.left.y);
		pages_move_xy(p_doc, -1 * p_doc->doc->bbox.left.x, -1 * p_doc->doc->bbox.left.y);
	}
	else{
		if (en_scale) {
			scale_x=p_size_x/(double)(p_doc->doc->paper.right.x - p_doc->doc->paper.left.x);
			scale_y=p_size_y/(double)(p_doc->doc->paper.right.y - p_doc->doc->paper.left.y);
			scale = min(scale_x,scale_y);
		}
		else {
			scale = 1;
		}
		p_size_x=scale * (double)(p_doc->doc->paper.right.x - p_doc->doc->paper.left.x);
		p_size_y=scale * (double)(p_doc->doc->paper.right.y - p_doc->doc->paper.left.y);
		pages_move_xy(p_doc, -1 * p_doc->doc->paper.left.x, -1 * p_doc->doc->paper.left.y);
	}
	
	if (center==1){
		off_x=(n_p_size_x)/x;
		off_y=(n_p_size_y)/y;
	}
	else{
		off_x=(x==1)?p_size_x:p_size_x + dx;
		off_y=(y==1)?p_size_y:p_size_y + dy;
	}
	pages_scale(p_doc,scale);

	switch (center) {
	case 1:
		pages_move_xy(p_doc,(off_x-p_size_x)/2.0,(off_y-p_size_y)/2.0);
		start_x=(orientation==DOC_O_LANDSCAPE)?(bbox->left.x + off_x * (x - 1) + (n_p_size_x - off_x * x)/2.0):(bbox->left.x + (n_p_size_x - off_x * x)/2.0);
		start_y=bbox->right.y - (n_p_size_y - off_y * y)/2 ;
		break;
	case 2:
		start_x=(orientation==DOC_O_LANDSCAPE)?(bbox->left.x + off_x * (x - 1)  + (n_p_size_x - off_x * x + (x==1?0:dx))/2.0) \
			:(bbox->left.x + (n_p_size_x - off_x * x + (x==1?0:dx))/2.0);
		start_y=bbox->right.y - (n_p_size_y - off_y * y - (y==1?0:dy))/2 ;
		break;
	default:
		start_x=(orientation==DOC_O_LANDSCAPE)?(bbox->left.x + off_x * (x - 1)):bbox->left.x;
		start_y=bbox->left.y + off_y * y + (y==1?0:dy);
		break;
	}
	new_list=pages_list_new(NULL,0);
	for (k=0;k<loops_count;++k){
		selected_pages=pages_list_new(NULL,0);

		if (orientation==DOC_O_LANDSCAPE){
			for (i=0;i<x;++i){
				for (j=1;j<=y;++j){
					transform_matrix  translate_matrix = {{1,0,0},{0,1,0},{0,0,1}};
					page_list * page;
					if (pages_count==0){
						goto out;
					}
					pages_count--;
					page = page_next(page_begin(p_doc));
					pages_list_add_page(selected_pages,page,pg_add_end);
					transform_matrix_move_xy(&translate_matrix,
									start_x - off_x*i, 
									start_y - off_y*j
								    );
					doc_page_transform(page,&translate_matrix);
					copy_dimensions(&page->page->paper,bbox);
					copy_dimensions(&page->page->bbox,bbox);
				}
			}
		}
		else{
			for (j=1;j<=y;++j){
				for (i=0;i<x;++i){
					transform_matrix  translate_matrix = {{1,0,0},{0,1,0},{0,0,1}};
					page_list * page;
					if (pages_count==0){
						goto out;
					}
					pages_count--;
					page = page_next(page_begin(p_doc));
					pages_list_add_page(selected_pages,page,pg_add_end);
					transform_matrix_move_xy(&translate_matrix,
									start_x + off_x*i,
									start_y - off_y*j
								    );
					doc_page_transform(page,&translate_matrix);
					copy_dimensions(&page->page->paper,bbox);
					copy_dimensions(&page->page->bbox,bbox);

				}
			}
		}
out:

		update_global_dimensions(p_doc);
		pages_to_one(selected_pages);
		{
		double _off_x, _off_y, _dx, _dy, _start_y;
		_start_y = start_y;
		if (center==1){
			_off_x = off_x;
			_off_y = off_y;
			_dx=0;
			_dy=0;
		}
		else{
			_off_x = (x==1)?off_x:off_x - dx;
			_off_y = (y==1)?off_y:off_y - dy;
			if (y!=1){
				_start_y -= dy;
			}
			_dx = dx;
			_dy = dy;
		}
		draw_frame(selected_pages,x,y,frame,0,(orientation==DOC_O_LANDSCAPE)?(start_x - off_x * (x - 1)):(start_x),_start_y, _off_x, _off_y,_dx,_dy);
		}
		pages_list_cat(new_list,selected_pages);
	}
	p_doc->doc->paper.right.x=bbox->right.x;
	p_doc->doc->paper.right.y=bbox->right.y;
	pages_list_cat(p_doc,new_list);
	update_global_dimensions(p_doc);
	return 0;
}
		

#define MARK_LEN 30
#define MARK_OFFSET 5
void draw_cmarks(page_list * page, dimensions * dim ,int type){
	coordinate begin, end;
		
	/*left_bottom*/
	begin.x=dim->left.x-MARK_OFFSET;
	begin.y=dim->left.y;
	copy_coordinate(&end,&begin);
	end.x-=MARK_LEN;
	doc_draw_to_page_line(page,&begin,&end,1);
	begin.x=dim->left.x;
	begin.y-=MARK_OFFSET;
	copy_coordinate(&end,&begin);
	end.y-=MARK_LEN;
	doc_draw_to_page_line(page,&begin,&end,1);
	/*right_bottom*/
	begin.x=dim->right.x+MARK_OFFSET;
	begin.y=dim->left.y;
	copy_coordinate(&end,&begin);
	end.x+=MARK_LEN;
	doc_draw_to_page_line(page,&begin,&end,1);
	begin.x=dim->right.x;
	begin.y-=MARK_OFFSET;
	copy_coordinate(&end,&begin);
	end.y-=MARK_LEN;
	doc_draw_to_page_line(page,&begin,&end,1);
	/*left_top*/
	begin.x=dim->left.x-MARK_OFFSET;
	begin.y=dim->right.y;
	copy_coordinate(&end,&begin);
	end.x-=MARK_LEN;
	doc_draw_to_page_line(page,&begin,&end,1);
	begin.x=dim->left.x;
	begin.y+=MARK_OFFSET;
	copy_coordinate(&end,&begin);
	end.y+=MARK_LEN;
	doc_draw_to_page_line(page,&begin,&end,1);
	/*right_top*/
	begin.x=dim->right.x+MARK_OFFSET;
	begin.y=dim->right.y;
	copy_coordinate(&end,&begin);
	end.x+=MARK_LEN;
	doc_draw_to_page_line(page,&begin,&end,1);
	begin.x=dim->right.x;
	begin.y+=MARK_OFFSET;
	copy_coordinate(&end,&begin);
	end.y+=MARK_LEN;
	doc_draw_to_page_line(page,&begin,&end,1);

}


int pages_cmarks(page_list_head * p_doc, int by_bbox){
	page_list * page;
	dimensions * dim;
	transform_matrix  matrix = {{1,0,0},{0,1,0},{0,0,1}};
	int move_x, move_y;

	if (by_bbox){
		dim = &(p_doc->doc->bbox);
	}
	else{
		dim = &(p_doc->doc->paper);
	}

	move_x=(MARK_LEN + MARK_OFFSET - dim->left.x);
	move_y=(MARK_LEN + MARK_OFFSET - dim->left.y);
	transform_matrix_move_xy(&matrix,move_x,move_y);
	transform_dimensions(dim,&matrix);

	for (page=page_next(page_begin(p_doc));page!=page_end(p_doc);page=page_next(page)){
		doc_page_transform(page,&matrix);
		draw_cmarks(page,dim ,0);
	}
	/*
	dim->left.x -= MARK_LEN + MARK_OFFSET; 
	dim->left.y -= MARK_LEN + MARK_OFFSET; 
	dim->right.x += MARK_LEN + MARK_OFFSET; 
	dim->right.y += MARK_LEN + MARK_OFFSET; 
	*/
	update_global_dimensions(p_doc);
	return 0;
}

int pages_norm(page_list_head * p_doc, int center, int scale, int l_bbox, int g_bbox){
	page_list * page;
	double gsize_x, gsize_y;
	dimensions paper, bbox;
	update_global_dimensions(p_doc);

	copy_dimensions(&bbox,&(p_doc->doc->bbox));
	copy_dimensions(&paper,&(p_doc->doc->paper));

	if (g_bbox){
		gsize_x = p_doc->doc->bbox.right.x-p_doc->doc->bbox.left.x;
		gsize_y = p_doc->doc->bbox.right.y-p_doc->doc->bbox.left.y;	
	}
	else{
		gsize_x = p_doc->doc->paper.right.x-p_doc->doc->paper.left.x;
		gsize_y = p_doc->doc->paper.right.y-p_doc->doc->paper.left.y;
	}
	for (page=page_next(page_begin(p_doc));page!=page_end(p_doc);page=page_next(page)){
		{
			double move_x, move_y;
			transform_matrix  matrix = {{1,0,0},{0,1,0},{0,0,1}};
			
			//fix if dimensions are zerro
			if (isdimzero((page->page->paper)) || isdimzero((page->page->bbox))) {
				continue;
			}


			if (g_bbox){
				move_x = p_doc->doc->bbox.left.x;
				move_y = p_doc->doc->bbox.left.y;
			}
			else{
				move_x = p_doc->doc->paper.left.x;
				move_y = p_doc->doc->paper.left.y;
			}
			if (l_bbox){
				move_x -= page->page->bbox.left.x;
			       	move_y -= page->page->bbox.left.y;
			}
			else{
				move_x -= page->page->paper.left.x;
			       	move_y -= page->page->paper.left.y;
			}
			transform_matrix_move_xy(&matrix,move_x,move_y);
			doc_page_transform(page,&matrix);


		}
		if (scale){
			double scale_x, scale_y, scale, size_x, size_y;
			transform_matrix  matrix = {{1,0,0},{0,1,0},{0,0,1}};
			if(l_bbox){
				size_x = page->page->bbox.right.x-page->page->bbox.left.x;
				size_y = page->page->bbox.right.y-page->page->bbox.left.y;
			}
			else{
				size_x = page->page->paper.right.x-page->page->paper.left.x;
				size_y = page->page->paper.right.y-page->page->paper.left.y;

			}
			scale_x=gsize_x/size_x;
			scale_y=gsize_y/size_y;
			scale=min(scale_x,scale_y);
#if 0
			printf("%f\n",scale);
#endif
			transform_matrix_scale(&matrix,scale);
			doc_page_transform(page,&matrix);
		}
		if (center){
			double size_x, size_y;
			transform_matrix  matrix = {{1,0,0},{0,1,0},{0,0,1}};
			if(l_bbox){
				size_x = page->page->bbox.right.x-page->page->bbox.left.x;
				size_y = page->page->bbox.right.y-page->page->bbox.left.y;
			}
			else{
				size_x = page->page->paper.right.x-page->page->paper.left.x;
				size_y = page->page->paper.right.y-page->page->paper.left.y;

			}
			switch (center){
			case 1:
				transform_matrix_move_xy(&matrix,(gsize_x-size_x)/2,(gsize_y-size_y)/2);
				break;
			case 2:
				transform_matrix_move_xy(&matrix,0,(gsize_y-size_y)/2);
				break;
			case 3:
				transform_matrix_move_xy(&matrix,(gsize_x-size_x)/2,gsize_y - size_y);
				break;
			}
			doc_page_transform(page,&matrix);
		}
		copy_dimensions(&(page->page->bbox),&(bbox));
		copy_dimensions(&(page->page->paper),&(paper));
	}
	copy_dimensions(&(p_doc->doc->bbox),&(bbox));
	copy_dimensions(&(p_doc->doc->paper),&(paper));
	return 0;
}

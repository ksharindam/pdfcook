#include "common.h"
#include "page_list.h"
#include "vdoc.h"
#include "magic.h"

page_list_head * pages_list_new(const page_list_head * from, int type){
	page_list_head * pom=(page_list_head *)malloc(sizeof(page_list_head));
	if (pom==NULL){
		assert(0);
		vdoc_errno=VDOC_ERR_LIBC;	
		return pom;
	}
	pom->id=M_ID_DOC_PAGE_LIST_HEAD;;
	pom->next=(page_list *) pom;
	pom->prev=(page_list *) pom;
	if (from){
		pom->doc=doc_handle_new(from->doc,type);
		pom->doc=doc_handle_copy_w(pom->doc);
		pages_count(pom)=0;
	}	
	else{
		pom->doc=doc_handle_new(NULL,type);

	}
	if (pom->doc == NULL){
		free(pom);
		assert(0);
		return NULL;
	}
	return pom;
}
/*nacteni seznamu stranek ze souboru */
page_list_head * pages_list_open(const char * name){
	return doc_open(name);
}
/*ulozeni seznamu stranek do souboru */
int pages_list_save(page_list_head * list,const char * name){
	return doc_save(list,name,NULL);
}
/*zruseni seznamu stranek*/
void pages_list_delete(page_list_head * which){
	assert(is_page_list_head(which));
	pages_list_empty(which);
	doc_handle_delete(which->doc);
	free(which);
}
/*smazani stranek ze seznamu*/
void pages_list_empty(page_list_head * which){
	assert(is_page_list_head(which));
	while(which->next!=(page_list *)which){
		page_delete(which->next);
	}
}

#if 0
/*kopie stranek ze seznamu*/
page_list_head * pages_list_copy(page_list_head * old){
	page_list_head * new;
	page_list * page;
	assert(is_page_list_head(old));
	new = (page_list_head *)malloc(sizeof(page_list_head));
	assert(new!=NULL);
	memcpy(new,old,sizeof(page_list_head));
	for(page=old->next;page!=(page_list *)old;page=page->next){
	}
}
#endif

/*maximum ze souradnic*/
void max_coordinates(coordinate * c1, coordinate * c2){
	c1->x=max(c1->x,c2->x);
	c1->y=max(c1->y,c2->y);
}
/*minimum ze souradnic*/
void min_coordinates(coordinate * c1, coordinate * c2){
	c1->x=min(c1->x,c2->x);
	c1->y=min(c1->y,c2->y);
}

void max_dimensions(dimensions * d1, dimensions *d2){
	if (isdimzero(*d1)) {
		copy_dimensions(d1, d2);
	}

	if (isdimzero(*d2)) {
		return;
	}

	max_coordinates(&(d1->right),&(d2->right));		
	min_coordinates(&(d1->left),&(d2->left));		
}

void update_page_info(doc_handle * doc, page_handle * page){
	max_dimensions(&doc->bbox,&page->bbox);
	max_dimensions(&doc->paper,&page->paper);
	if (isdimzero(page->bbox)){
		copy_dimensions(&page->bbox,&doc->bbox);
	}
	if (isdimzero(page->paper)){
		copy_dimensions(&page->paper,&doc->paper);
	}
	doc->orient=(doc->orient==DOC_O_UNKNOWN)?page->orient:doc->orient;
	page->orient=(page->orient==DOC_O_UNKNOWN)?doc->orient:page->orient;
}

/*pridani stranky do seznamu*/
void pages_list_add_page(page_list_head * where, page_list * what, pg_add_mode how){
	switch(how){
		case pg_add_begin:
			pages_list_insert_page(where, what, (page_list *)where);
			break;
		case pg_add_end:
			pages_list_insert_page(where, what, where->prev);
			break;
	}
}
page_list * page_list_remove_page(page_list * what){
	assert(is_page_list(what));
	if (what->next && what->prev){
		what->next->prev=what->prev;
		what->prev->next=what->next;
		what->next=NULL;
		what->prev=NULL;
		pages_count(what->page)-=1;
	}
	return what;
}
void pages_list_insert_page(page_list_head * where, page_list * what, page_list * behide){
	if (!is_page_list_head(where) || !is_page_list(what) || (!is_page_list(behide) && !is_page_list_head(behide)) ){
		assert(0);
		return;
	}

	if (isdimzero(what->page->paper)){
		copy_dimensions(&what->page->paper,&where->doc->paper);
	}

	if (isdimzero(what->page->bbox)){
		copy_dimensions(&what->page->bbox,&where->doc->bbox);
	}
	
	if (isdimzero(where->doc->paper)){
		copy_dimensions(&where->doc->paper,&what->page->paper);
	}

	if (isdimzero(where->doc->bbox)){
		copy_dimensions(&where->doc->bbox,&what->page->bbox);
	}

	if (isdimzero(what->page->paper)){
			copy_dimensions(&what->page->paper,&what->page->bbox);
		}
		
	if (isdimzero(what->page->bbox)){
		copy_dimensions(&what->page->bbox,&what->page->paper);
	}

	/*vyhozeni stranky ze seznamu*/
	what->page=page_handle_copy_w(what->page);
	what=page_list_remove_page(what);
	if (what->page->doc){
		if (doc_handle_merge(where->doc,what->page->doc)==-1){
			assert(0);
			return;
		}
		doc_handle_delete(what->page->doc);
	}
	what->page->doc=doc_handle_new(where->doc,0);
	
	listinsert(what,behide);
	pages_count(where)+=1;
	update_page_info(where->doc,what->page);
}

/*prevedeni cisla stranky na ukazatel na stranku*/
page_list * page_num_to_ptn(page_list_head * plist, int number){
	page_list * start;
	int i;
	assert(is_page_list_head(plist));
	
	number=(number<0)?(plist->doc->pages_count+1+number):number;

	if (number == pages_count(plist) + 1){
		return page_end(plist);
	}

	if (number<0 || number>pages_count(plist)){
		message(FATAL,"number %d is out of pages count.\n",number);
		printf("%d\n",number);
		assert(0);
		return NULL;
	}

	if (number==0){
		return page_begin(plist);
	}

	for (i=1,start=plist->next;i<number && start!=(page_list *) plist;++i,start=start->next)
		;
	if (start==(page_list*)plist){
		return NULL;
	}
	return start;
}

/*spojeni dvou seznamu stranek do jednoho*/
int pages_list_cat(page_list_head * l1, page_list_head * l2){
	page_list * pom;
	assert(is_page_list_head(l1) && is_page_list_head(l2));

	if (doc_handle_convert_format(l1,l2)==-1){
		/*assert(0);*/
		return -1;
	}

	if (doc_handle_merge(l1->doc,l2->doc)==-1){
		assert(0);
		return -1;
	}
	for(pom=l2->next;pom!=(page_list*)(l2);pom=pom->next){
		doc_handle_delete(pom->page->doc);
		pom->page->doc=doc_handle_new(l1->doc,0);
		update_page_info(l1->doc,pom->page);
	}
	
	pages_count(l1)+=pages_count(l2);
	listcat(page_list,l1,l2);
	doc_handle_delete(l2->doc);
	free(l2);
	return 0;
}

/*vytvori novou stranku, pokud handle ukazuje na platnou stranku, vytvori kopii*/ 
page_list * page_new_ext(page_list * from, int doc_type, doc_handle * doc){
	page_list * pom;
	pom=(page_list *) malloc(sizeof(page_list));
	if (pom==NULL){
		vdoc_errno=VDOC_ERR_LIBC;	
		message(FATAL,"malloc() error");
		assert(0);
		return pom;
	}
	else{
		pom->next=NULL;
		pom->prev=NULL;
		pom->id=M_ID_DOC_PAGE_LIST;
		pom->page=page_handle_new_ext(from?(from->page):NULL,doc_type,doc);
		if (pom->page==NULL){
			free(pom);
			assert(0);
			return NULL;
		}
		else{
			return pom;
		}	
	}
}
page_list * page_new(page_list * from, int doc_type){
	return page_new_ext(from, doc_type, NULL);
}

/* odstrani stranku*/
int page_delete(page_list * which){
	int ret_val;
	which=page_list_remove_page(which);
	ret_val=page_handle_delete(which->page);
	free(which);
	return ret_val;
}

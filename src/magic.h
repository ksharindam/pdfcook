#ifndef _MAGIC_H_
#define _MAGIC_H_
enum magic_id {
	M_ID_UNKNOWN,
	M_ID_DOC_HANDLE,
	M_ID_DOC_PAGE_HANDLE,
	M_ID_DOC_PAGE_LIST,
	M_ID_DOC_PAGE_LIST_HEAD
};

#define test_magic(s_h,m_id)((s_h==NULL)?(vdoc_errno=VDOC_ERR_NULL_POINTER, 0):(s_h->id!=m_id?vdoc_errno=m_id,0:1))

#define is_doc_handle(l) (test_magic(l,M_ID_DOC_HANDLE))
#define is_page_handle(l) (test_magic(l,M_ID_DOC_PAGE_HANDLE))
#define is_page_list(l) (test_magic(l,M_ID_DOC_PAGE_LIST))
#define is_page_list_head(l) (test_magic(l,M_ID_DOC_PAGE_LIST_HEAD))

#endif

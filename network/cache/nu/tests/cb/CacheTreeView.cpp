// CacheTreeView.cpp : implementation file
//

#include "stdafx.h"
#include "cb.h"
#include "CacheTreeView.h"
#include "cbdoc.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CCacheTreeView

IMPLEMENT_DYNCREATE(CCacheTreeView, CTreeView)

CCacheTreeView::CCacheTreeView()
{
	m_pImgList = new CImageList();
}

CCacheTreeView::~CCacheTreeView()
{
	delete m_pImgList;
}


BEGIN_MESSAGE_MAP(CCacheTreeView, CTreeView)
	//{{AFX_MSG_MAP(CCacheTreeView)
	ON_WM_CREATE()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

int CCacheTreeView::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{

	lpCreateStruct->style = TVS_HASLINES | TVS_HASBUTTONS | TVS_LINESATROOT;
	if (CView::OnCreate(lpCreateStruct) == -1)
		return -1;

	GetDocument()->m_pTreeView = this;

	// Create the Image List
	m_pImgList->Create(IDB_CACHEICONS,16,0,RGB(255,0,255));
	m_pImgList->SetBkColor(GetSysColor(COLOR_WINDOW));

	// Attach image list to Tree
	GetTreeCtrl().SetImageList(m_pImgList, TVSIL_NORMAL);
	GetTreeCtrl().SetIndent(10);
	return 0;
}

void CCacheTreeView::OnDraw(CDC* pDC)
{
	CDocument* pDoc = GetDocument();
	// TODO: add draw code here
}


#ifdef _DEBUG
void CCacheTreeView::AssertValid() const
{
	CTreeView::AssertValid();
}

void CCacheTreeView::Dump(CDumpContext& dc) const
{
	CTreeView::Dump(dc);
}

CCbDoc* CCacheTreeView::GetDocument()
{ 
	ASSERT(m_pDocument->IsKindOf(RUNTIME_CLASS(CCbDoc)));
	return (CCbDoc*)m_pDocument; 
}

#endif //_DEBUG

void CCacheTreeView::Populate() {
	AssertValid();
	GetTreeCtrl().DeleteAllItems();

	TV_INSERTSTRUCT insertStruct;
	TV_ITEM itemStruct;

	insertStruct.hParent = TVI_ROOT;
	insertStruct.hInsertAfter = TVI_FIRST;
	insertStruct.item = itemStruct;

	HTREEITEM hti = GetTreeCtrl().InsertItem(&insertStruct);
	GetTreeCtrl().SetItemText(hti, "Your Cache");

	insertStruct.hParent = hti;
	insertStruct.hInsertAfter = hti;
	insertStruct.item = itemStruct;

	HTREEITEM htiDisk = GetTreeCtrl().InsertItem(&insertStruct);
	GetTreeCtrl().SetItemText(htiDisk, "Disk");
	HTREEITEM htiMem = GetTreeCtrl().InsertItem(&insertStruct);
	GetTreeCtrl().SetItemText(htiMem, "Memory");
}
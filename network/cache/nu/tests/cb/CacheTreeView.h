// CacheTreeView.h : header file
//
#ifndef _CacheTreeView_h_
#define _CacheTreeView_h_

#include <afxcview.h>
class CImageList;
class CCbDoc;
class CCacheTreeView : public CTreeView
{
protected:
	CCacheTreeView();           // protected constructor used by dynamic creation
	DECLARE_DYNCREATE(CCacheTreeView)

// Attributes
public:
	CCbDoc* GetDocument();
	void Populate();
// Operations
public:

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CCacheTreeView)
	protected:
	virtual void OnDraw(CDC* pDC);      // overridden to draw this view
	//}}AFX_VIRTUAL

protected:
	virtual ~CCacheTreeView();
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

	// Generated message map functions
protected:
	//{{AFX_MSG(CCacheTreeView)
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	//}}AFX_MSG

	DECLARE_MESSAGE_MAP()

	CImageList* m_pImgList;
};

#ifndef _DEBUG  // debug version in AnimalView.cpp
inline CCbDoc* CCacheTreeView::GetDocument()
   { return (CCbDoc*)m_pDocument; }
#endif

#endif



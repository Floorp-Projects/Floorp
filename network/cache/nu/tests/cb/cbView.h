// cbView.h : interface of the CCbView class
//
/////////////////////////////////////////////////////////////////////////////
class CCbView : public CView
{
protected: // create from serialization only
	CCbView();
	DECLARE_DYNCREATE(CCbView)

// Attributes
public:
	CCbDoc* GetDocument();

// Operations
public:

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CCbView)
	public:
	virtual void OnDraw(CDC* pDC);  // overridden to draw this view
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
	virtual void OnInitialUpdate();
	protected:
	virtual BOOL OnPreparePrinting(CPrintInfo* pInfo);
	virtual void OnBeginPrinting(CDC* pDC, CPrintInfo* pInfo);
	virtual void OnEndPrinting(CDC* pDC, CPrintInfo* pInfo);
	//}}AFX_VIRTUAL

// Implementation
public:
	virtual ~CCbView();
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

protected:

// Generated message map functions
protected:
	//{{AFX_MSG(CCbView)
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

	CString m_Mesg;
};

#ifndef _DEBUG  // debug version in cbView.cpp
inline CCbDoc* CCbView::GetDocument()
   { return (CCbDoc*)m_pDocument; }
#endif

/////////////////////////////////////////////////////////////////////////////

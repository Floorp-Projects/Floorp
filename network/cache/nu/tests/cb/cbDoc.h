// cbDoc.h : interface of the CCbDoc class
//
/////////////////////////////////////////////////////////////////////////////
class CCacheTreeView;
class CCbDoc : public CDocument
{
protected: // create from serialization only
	CCbDoc();
	DECLARE_DYNCREATE(CCbDoc)

// Attributes
public:
	CCacheTreeView* m_pTreeView;
// Operations
public:

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CCbDoc)
	public:
	virtual BOOL OnNewDocument();
	virtual void Serialize(CArchive& ar);
	//}}AFX_VIRTUAL

// Implementation
public:
	virtual ~CCbDoc();
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

protected:

// Generated message map functions
protected:
	//{{AFX_MSG(CCbDoc)
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

};

/////////////////////////////////////////////////////////////////////////////

// cbDoc.cpp : implementation of the CCbDoc class
//

#include "stdafx.h"
#include "cb.h"

#include "cbDoc.h"
#include "CacheTreeView.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CCbDoc

IMPLEMENT_DYNCREATE(CCbDoc, CDocument)

BEGIN_MESSAGE_MAP(CCbDoc, CDocument)
	//{{AFX_MSG_MAP(CCbDoc)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CCbDoc construction/destruction

CCbDoc::CCbDoc()
{
}

CCbDoc::~CCbDoc()
{
}

BOOL CCbDoc::OnNewDocument()
{
	if (!CDocument::OnNewDocument())
		return FALSE;
	if (m_pTreeView)
		m_pTreeView->Populate();
	return TRUE;
}

/////////////////////////////////////////////////////////////////////////////
// CCbDoc serialization

void CCbDoc::Serialize(CArchive& ar)
{
	if (ar.IsStoring())
	{
	}
	else
	{
	}
}

/////////////////////////////////////////////////////////////////////////////
// CCbDoc diagnostics

#ifdef _DEBUG
void CCbDoc::AssertValid() const
{
	CDocument::AssertValid();
}

void CCbDoc::Dump(CDumpContext& dc) const
{
	CDocument::Dump(dc);
}
#endif //_DEBUG

/////////////////////////////////////////////////////////////////////////////
// CCbDoc commands

// cbView.cpp : implementation of the CCbView class
//

#include "stdafx.h"
#include "cb.h"

#include "cbDoc.h"
#include "cbView.h"
#include <afxcmn.h>
#include "MainFrm.h"

#include "nsCacheManager.h"
#include "nsMemModule.h"
#include "nsDiskModule.h"
#include "nsCacheObject.h"
#include "nsTimeIt.h"
#include "nsCacheBkgThd.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

#define ID_TREECTRL (WM_USER +1)

#define STATUS(s) ((CMainFrame*)GetParentFrame())->Status(s)

IMPLEMENT_DYNCREATE(CCbView, CView)

BEGIN_MESSAGE_MAP(CCbView, CView)
    //{{AFX_MSG_MAP(CCbView)
    //}}AFX_MSG_MAP
    // Standard printing commands
    ON_COMMAND(ID_FILE_PRINT, CView::OnFilePrint)
    ON_COMMAND(ID_FILE_PRINT_DIRECT, CView::OnFilePrint)
    ON_COMMAND(ID_FILE_PRINT_PREVIEW, CView::OnFilePrintPreview)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CCbView construction/destruction

CCbView::CCbView()
{
}

CCbView::~CCbView()
{
}

BOOL CCbView::PreCreateWindow(CREATESTRUCT& cs)
{
	return CView::PreCreateWindow(cs);
}

/////////////////////////////////////////////////////////////////////////////
// CCbView drawing

void CCbView::OnDraw(CDC* pDC)
{
    CRect rect;
    GetClientRect(&rect);

    pDC->SetBkColor(RGB(192,192,192));
    pDC->FillSolidRect(rect, RGB(192,192,192));

    rect.DeflateRect(10,10);
    pDC->DrawText(m_Mesg.GetBuffer(m_Mesg.GetLength()), -1, rect, DT_WORDBREAK | DT_EXPANDTABS);
}

/////////////////////////////////////////////////////////////////////////////
// CCbView printing

BOOL CCbView::OnPreparePrinting(CPrintInfo* pInfo)
{
    // default preparation
    return DoPreparePrinting(pInfo);
}

void CCbView::OnBeginPrinting(CDC* /*pDC*/, CPrintInfo* /*pInfo*/)
{
}

void CCbView::OnEndPrinting(CDC* /*pDC*/, CPrintInfo* /*pInfo*/)
{
}

/////////////////////////////////////////////////////////////////////////////
// CCbView diagnostics

#ifdef _DEBUG
void CCbView::AssertValid() const
{
	CView::AssertValid();
}

void CCbView::Dump(CDumpContext& dc) const
{
	CView::Dump(dc);
}

CCbDoc* CCbView::GetDocument() // non-debug version is inline
{
	ASSERT(m_pDocument->IsKindOf(RUNTIME_CLASS(CCbDoc)));
	return (CCbDoc*)m_pDocument;
}
#endif //_DEBUG

/////////////////////////////////////////////////////////////////////////////
// CCbView message handlers

void CCbView::OnInitialUpdate() 
{
    //STATUS("Loading lib...");

    nsCacheManager* pCM = nsCacheManager::GetInstance();
    nsMemModule* pMM = pCM->GetMemModule();
    nsDiskModule* pDM = new nsDiskModule();
    pCM->AddModule(pDM);
    
    nsCacheObject* pCO = new nsCacheObject("http://www.netscape.com/");
    pCO->Etag("Etag-testing");
    pCO->Size(2250);
    pCO->LastModified(time(0));
    pDM->AddObject(pCO);

//    if (!pDM->Contains("http://www.netscape.com/"))
//        pDM->AddObject(pCO);
//    else 
    {
        nsCacheObject* pTemp = new nsCacheObject("http://www.netscape.com/");
        if (pDM->Contains(pTemp))
        {
            m_Mesg += pTemp->Trace();
        }
    }
    int j = 1000;
    char tmpBuff[10];
    while (j--)
    {
        pCO = new nsCacheObject(itoa(j,tmpBuff,10));
        VERIFY(pDM->AddObject(pCO));
    }

    m_Mesg += itoa(pDM->Entries(), tmpBuff, 10);
    m_Mesg += " nsCacheObject(s) added.\n";

    char* traceBuff =(char*) pCM->Trace();
    m_Mesg += "-----------------\n";
    m_Mesg += traceBuff;
    delete[] traceBuff;
    traceBuff = (char*) pMM->Trace();
    m_Mesg += traceBuff;
    delete[] traceBuff;

    m_Mesg += "-----------------\n";


    char buffer[10];
    m_Mesg += "Worst case time= ";
    m_Mesg += itoa(pCM->WorstCaseTime(), buffer, 10);
    m_Mesg += " microsec.\n";

    PRUint32 t;
    {
        nsTimeIt tmp(t);
        if (pCM->Contains("999"))
            m_Mesg += "!";
    }
    m_Mesg += "-----------------\n";
    m_Mesg += "An item found in ";
    m_Mesg += itoa(t, buffer, 10);
    m_Mesg += " microsec.\n";

    CView::OnInitialUpdate();
}



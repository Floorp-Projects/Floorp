/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code,
 * released March 31, 1998.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 *
 * Contributors:
 *     Troy Chevalier <troy@netscape.com>
 *     Sean Su <ssu@netscape.com>
 */

// nszipvw.cpp : implementation of the CNsZipView class
//

#include "stdafx.h"
#include "nszip.h"

#include "nszipdoc.h"
#include "nszipvw.h"

#ifdef _DEBUG
#undef THIS_FILE
static char BASED_CODE THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CNsZipView

IMPLEMENT_DYNCREATE(CNsZipView, CView)

BEGIN_MESSAGE_MAP(CNsZipView, CView)
	//{{AFX_MSG_MAP(CNsZipView)
	ON_COMMAND(ID_ACTIONS_DELETE, OnActionsDelete)
	ON_UPDATE_COMMAND_UI(ID_ACTIONS_DELETE, OnUpdateActionsDelete)
	ON_COMMAND(ID_ACTIONS_ADD, OnActionsAdd)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CNsZipView construction/destruction

CNsZipView::CNsZipView()
{
	// TODO: add construction code here

}

CNsZipView::~CNsZipView()
{
}

/////////////////////////////////////////////////////////////////////////////
// CNsZipView drawing

void CNsZipView::OnDraw(CDC* pDC)
{
	CNsZipDoc* pDoc = GetDocument();
	ASSERT_VALID(pDoc);

	// TODO: add draw code for native data here
}

/////////////////////////////////////////////////////////////////////////////
// CNsZipView diagnostics

#ifdef _DEBUG
void CNsZipView::AssertValid() const
{
	CView::AssertValid();
}

void CNsZipView::Dump(CDumpContext& dc) const
{
	CView::Dump(dc);
}

CNsZipDoc* CNsZipView::GetDocument() // non-debug version is inline
{
	ASSERT(m_pDocument->IsKindOf(RUNTIME_CLASS(CNsZipDoc)));
	return (CNsZipDoc*)m_pDocument;
}
#endif //_DEBUG

/////////////////////////////////////////////////////////////////////////////
// CNsZipView message handlers

void CNsZipView::OnActionsDelete() 
{
	// TODO: Add your command handler code here
	
}

void CNsZipView::OnUpdateActionsDelete(CCmdUI* pCmdUI) 
{
	// TODO: Add your command update UI handler code here
	
}

void CNsZipView::OnActionsAdd() 
{
	// TODO: Add your command handler code here
	
}

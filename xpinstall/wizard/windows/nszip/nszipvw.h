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

// nszipvw.h : interface of the CNsZipView class
//
/////////////////////////////////////////////////////////////////////////////

class CNsZipView : public CView
{
protected: // create from serialization only
	CNsZipView();
	DECLARE_DYNCREATE(CNsZipView)

// Attributes
public:
	CNsZipDoc* GetDocument();

// Operations
public:

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CNsZipView)
	public:
	virtual void OnDraw(CDC* pDC);  // overridden to draw this view
	protected:
	//}}AFX_VIRTUAL

// Implementation
public:
	virtual ~CNsZipView();
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

protected:

// Generated message map functions
protected:
	//{{AFX_MSG(CNsZipView)
	afx_msg void OnActionsDelete();
	afx_msg void OnUpdateActionsDelete(CCmdUI* pCmdUI);
	afx_msg void OnActionsAdd();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

#ifndef _DEBUG  // debug version in nszipvw.cpp
inline CNsZipDoc* CNsZipView::GetDocument()
   { return (CNsZipDoc*)m_pDocument; }
#endif

/////////////////////////////////////////////////////////////////////////////

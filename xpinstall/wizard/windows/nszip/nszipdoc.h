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

// nszipdoc.h : interface of the CNsZipDoc class
//
/////////////////////////////////////////////////////////////////////////////

class CNsZipDoc : public CDocument
{
protected: // create from serialization only
	CNsZipDoc();
	DECLARE_DYNCREATE(CNsZipDoc)

// Attributes
public:

// Operations
public:
	// Create an archive with the specified file name. Wrapper around
	// the MFC OnNewDocument member
	BOOL	OnNewDocument(LPCTSTR lpszPathName);

	// Add a single file to the archive
	BOOL	AddFile(LPCTSTR lpszFile);

	// Add one or more files to the archive (lpszFiles can be a wildcard)
	void	AddFiles(LPCTSTR lpszFiles);

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CNsZipDoc)
	public:
	virtual void OnCloseDocument();
	virtual BOOL OnOpenDocument(LPCTSTR lpszPathName);
	virtual void DeleteContents();
	virtual BOOL OnSaveDocument(LPCTSTR lpszPathName);
	//}}AFX_VIRTUAL

// Implementation
public:
	virtual ~CNsZipDoc();
	virtual void Serialize(CArchive& ar);   // overridden for document i/o
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

protected:
	HANDLE	m_hUpdateFile;

// Generated message map functions
protected:
	//{{AFX_MSG(CNsZipDoc)
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////

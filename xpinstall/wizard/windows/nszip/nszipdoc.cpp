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

// nszipdoc.cpp : implementation of the CNsZipDoc class
//

#include "stdafx.h"
#include "nszip.h"

#include "nszipdoc.h"

#ifdef _DEBUG
#undef THIS_FILE
static char BASED_CODE THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CNsZipDoc

IMPLEMENT_DYNCREATE(CNsZipDoc, CDocument)

BEGIN_MESSAGE_MAP(CNsZipDoc, CDocument)
	//{{AFX_MSG_MAP(CNsZipDoc)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CNsZipDoc construction/destruction

CNsZipDoc::CNsZipDoc()
{
	m_hUpdateFile = NULL;
}

CNsZipDoc::~CNsZipDoc()
{
}

BOOL CNsZipDoc::OnNewDocument(LPCTSTR lpszPathName)
{
	char	szStub[MAX_PATH];

	// If we were doing a resource update, discard any changes
	if (m_hUpdateFile) {
		VERIFY(::EndUpdateResource(m_hUpdateFile, TRUE));

		// Delete the archive
		DeleteFile(m_strPathName);
	}

	if (!CDocument::OnNewDocument())
		return FALSE;

	// Set the pathname
	SetPathName(lpszPathName);

	// Copy the stub executable and make it the basis of the archive
	::GetModuleFileName(NULL, szStub, sizeof(szStub));
	strcpy(strrchr(szStub, '\\') + 1, "Nsinstall.exe");
	::CopyFile(szStub, m_strPathName, FALSE);  // overwrite an existing file

	// Get a handle we can use with UpdateResource()
	VERIFY(m_hUpdateFile = ::BeginUpdateResource(m_strPathName, FALSE));
	return m_hUpdateFile != NULL;
}

/////////////////////////////////////////////////////////////////////////////
// CNsZipDoc serialization

void CNsZipDoc::Serialize(CArchive& ar)
{
	if (ar.IsStoring())
	{
		// TODO: add storing code here
	}
	else
	{
		// TODO: add loading code here
	}
}

/////////////////////////////////////////////////////////////////////////////
// CNsZipDoc diagnostics

#ifdef _DEBUG
void CNsZipDoc::AssertValid() const
{
	CDocument::AssertValid();
}

void CNsZipDoc::Dump(CDumpContext& dc) const
{
	CDocument::Dump(dc);
}
#endif //_DEBUG

/////////////////////////////////////////////////////////////////////////////
// CNsZipDoc commands

void CNsZipDoc::OnCloseDocument() 
{
	// If we were doing a resource update, discard any changes
	if (m_hUpdateFile) {
		VERIFY(::EndUpdateResource(m_hUpdateFile, TRUE));
		m_hUpdateFile = NULL;

		// Delete the archive
		DeleteFile(m_strPathName);
	}
	
	CDocument::OnCloseDocument();
}

BOOL CNsZipDoc::OnOpenDocument(LPCTSTR lpszPathName) 
{
	// If we were doing a resource update, discard any changes
	if (m_hUpdateFile) {
		VERIFY(::EndUpdateResource(m_hUpdateFile, TRUE));

		// Delete the archive
		DeleteFile(m_strPathName);
	}

	DeleteContents();

	// ZZZ: Get a list of all the FILE resources
	ASSERT(FALSE);	
	return TRUE;
}

void CNsZipDoc::DeleteContents() 
{
	m_hUpdateFile = NULL;
	CDocument::DeleteContents();
}

BOOL CNsZipDoc::OnSaveDocument(LPCTSTR lpszPathName) 
{
	// If we were doing a resource update, save any changes
	if (m_hUpdateFile) {
		BOOL	bRet = ::EndUpdateResource(m_hUpdateFile, FALSE);

		ASSERT(bRet);
		m_hUpdateFile = NULL;
		return bRet;
	}

	return TRUE;
}

// Add a single file to the archive
BOOL CNsZipDoc::AddFile(LPCTSTR lpszFile)
{
	HANDLE	hFile;
	LPBYTE	lpBuf;
	LPBYTE	lpBufCmp;
	DWORD	dwBytesRead;
	DWORD	dwFileSize;
	DWORD	dwFileSizeCmp;

	// Check if we are trying to add the archive file itself
	// (this could happen if the user passes *.* as a file spec)
	if (m_strPathName.CompareNoCase(lpszFile) == 0)
		return FALSE;

	// Open the file
	hFile = ::CreateFile(lpszFile, GENERIC_READ, FILE_SHARE_READ, NULL,
		OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

	if (hFile == INVALID_HANDLE_VALUE)
		return FALSE;

	// Figure out how big the file is
	dwFileSize = GetFileSize(hFile, NULL);

	// Allocate enough space for the file contents and a DWORD header that
	// contains the size of the file
	lpBuf         = (LPBYTE)malloc(dwFileSize);
	lpBufCmp      = (LPBYTE)malloc(dwFileSize + (sizeof(DWORD) * 2));
  dwFileSizeCmp = dwFileSize;

  if (lpBuf) {
		CString	strResourceName = strrchr(lpszFile, '\\') + 1;

		// It's really important that the resource name be UPPERCASE
		strResourceName.MakeUpper();

//		*(LPDWORD)lpBuf = dwFileSize;
		::ReadFile(hFile, lpBuf, dwFileSize, &dwBytesRead, NULL);
		ASSERT(dwBytesRead == dwFileSize);

    if(compress((lpBufCmp + (sizeof(DWORD) * 2)), &dwFileSizeCmp, (const Bytef*)lpBuf, dwFileSize) != Z_OK)
      return(FALSE);

    // The first DWORD holds the total size of the file to be stored in the
    // resource (in this case, it's the compressed size)
    // The second DWORD holds the original uncompressed size of the file.
    *(LPDWORD)lpBufCmp = dwFileSizeCmp;
    *(LPDWORD)(lpBufCmp + sizeof(DWORD)) = dwFileSize;
    
    VERIFY(::UpdateResource(m_hUpdateFile, "FILE", strResourceName,
			MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US), lpBufCmp, dwFileSizeCmp + (sizeof(DWORD) * 2)));
	}

	free(lpBuf);
	::CloseHandle(hFile);
	return TRUE;
}

// Add one or more files to the archive (lpszFiles can be a wildcard)
void CNsZipDoc::AddFiles(LPCTSTR lpszFiles)
{
	ASSERT(m_hUpdateFile);
	ASSERT(lpszFiles);

	if (lpszFiles) {
		HANDLE			hFindFile;
		WIN32_FIND_DATA	findFileData;
		char			szPath[MAX_PATH];

		// Get a full pathname to the files
		::GetFullPathName(lpszFiles, sizeof(szPath), szPath, NULL);

		// Get the first file that matches
		hFindFile = ::FindFirstFile(szPath, &findFileData);

		if (hFindFile == INVALID_HANDLE_VALUE)
			return;

		do {
			char	szFile[MAX_PATH];

			// We need to pass to AddFile() whatever kind of path we were passed,
			// e.g. simple, relative, full
			strcpy(szFile, szPath);
			strcpy(strrchr(szFile, '\\') + 1, findFileData.cFileName);
			AddFile(szFile);
		} while (::FindNextFile(hFindFile, &findFileData));

		::FindClose(hFindFile);
	}
}

/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
#ifndef TESTS_H
#define TESTS_H

#include "CBrowse_i.h"

class CBrowseDlg;
struct Test;

class BrowserInfo
{
public:
	Test *pTest;
	TestResult nResult;
	CBrowserCtlSiteInstance *pControlSite;
	IUnknown *pIUnknown;
	CLSID clsid;
	CBrowseDlg *pBrowseDlg;
	CString szTestURL;
	CString szTestCGI;

	void OutputString(const TCHAR *szMessage, ...);
	HRESULT GetWebBrowser(IWebBrowserApp **pWebBrowser);
	HRESULT GetDocument(IHTMLDocument2 **pDocument);
};


typedef TestResult (__cdecl *TestProc)(BrowserInfo &cInfo);

struct Test
{
	TCHAR szName[256];
	TCHAR szDesc[256];
	TestProc pfn;
	TestResult nLastResult;
};

struct TestSet;
typedef void (__cdecl *SetPopulatorProc)(TestSet *pTestSet);

struct TestSet
{
	TCHAR *szName;
	TCHAR *szDesc;
	int    nTests;
	Test  *aTests;
	SetPopulatorProc pfnPopulator;
};

extern TestSet aTestSets[];
extern int nTestSets;

#endif
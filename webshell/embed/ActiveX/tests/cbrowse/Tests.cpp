/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#include "stdafx.h"

TestResult __cdecl tstDocument(BrowserInfo *pInfo)
{
	CIPtr(IHTMLDocument2) cpDocElement;
	pInfo->GetDocument(&cpDocElement);
	if (cpDocElement == NULL)
	{
		return trFailed;
	}

	return trPassed;
}

TestResult __cdecl tstTesters(BrowserInfo *pInfo)
{
	AfxMessageBox("Test architecture is reasonably sane!");
	return trPassed;
}

TestResult __cdecl tstControlActive(BrowserInfo *pInfo)
{
	CControlSiteInstance *pControlSite = pInfo->pControlSite;
	if (pControlSite == NULL || !pControlSite->IsInPlaceActive())
	{
		return trFailed;
	}
	return trPassed;
}

TestResult __cdecl tstIWebBrowser(BrowserInfo *pInfo)
{
	if (pInfo->pIUnknown == NULL)
	{
		return trFailed;
	}
	IWebBrowser *pIWebBrowser = NULL;
	pInfo->pIUnknown->QueryInterface(IID_IWebBrowser, (void **) &pIWebBrowser);
	if (pIWebBrowser)
	{
		pIWebBrowser->Release();
		return trPassed;
	}

	return trFailed;
}
TestResult __cdecl tstIWebBrowser2(BrowserInfo *pInfo)
{
	if (pInfo->pIUnknown == NULL)
	{
		return trFailed;
	}
	IWebBrowser2 *pIWebBrowser = NULL;
	pInfo->pIUnknown->QueryInterface(IID_IWebBrowser2, (void **) &pIWebBrowser);
	if (pIWebBrowser)
	{
		pIWebBrowser->Release();
		return trPassed;
	}

	return trFailed;
}


TestResult __cdecl tstIWebBrowserApp(BrowserInfo *pInfo)
{
	if (pInfo->pIUnknown == NULL)
	{
		return trFailed;
	}
	IWebBrowser2 *pIWebBrowser = NULL;
	pInfo->pIUnknown->QueryInterface(IID_IWebBrowserApp, (void **) &pIWebBrowser);
	if (pIWebBrowser)
	{
		pIWebBrowser->Release();
		return trPassed;
	}

	return trFailed;
}

Test aBasic[] =
{
	{ _T("Test Tester"), _T("Ensure that the testing architecture is working"), tstTesters },
	{ _T("Control basics"), _T("Ensure that the browser control is active"), tstControlActive },
	{ _T("IWebBrowser"), _T("Test if control has an IWebBrowser interface"), tstIWebBrowser },
	{ _T("IWebBrowser2"), _T("Test if control has an IWebBrowser2 interface"), tstIWebBrowser2 },
	{ _T("IWebBrowserApp"), _T("Test if control has an IWebBrowserApp interface"), tstIWebBrowserApp }
};

Test aDHTML[] =
{
	{ _T("get_Document"), _T("Test if browser has a top level element"), tstDocument }
};

TestSet aTestSets[] =
{
	{ _T("Basic"), _T("Basic sanity tests"), 5, aBasic },
	{ _T("Browsing"), _T("Browsing and navigation tests"), 0, NULL },
	{ _T("DHTML"), _T("Test the DOM"), 1, aDHTML }
};

int nTestSets = sizeof(aTestSets) / sizeof(aTestSets[0]);
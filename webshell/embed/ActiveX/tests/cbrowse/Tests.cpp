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


void tstDrillerLevel(BrowserInfo *pInfo, IHTMLElementCollection *pCollection, int nLevel)
{
	if (pCollection == NULL)
	{
		return;
	}

	LONG celem = 0;;
	HRESULT hr = pCollection->get_length( &celem );
	for ( int i=0; i< celem; i++ )
	{
		VARIANT varIndex;
		varIndex.vt = VT_UINT;
		varIndex.lVal = i;
		VARIANT var2;
		VariantInit( &var2 );
		CIPtr(IDispatch) cpDisp; 

		hr = pCollection->item( varIndex, var2, &cpDisp );
		if ( hr != S_OK )
		{
			continue;
		}

		CIPtr(IHTMLElement) cpElem;

		hr = cpDisp->QueryInterface( IID_IHTMLElement, (void **)&cpElem );
		if ( hr == S_OK )
		{

			BSTR bstr;
			hr = cpElem->get_tagName(&bstr);
			
			USES_CONVERSION;
			char szLevel[256];
			memset(szLevel, 0, sizeof(szLevel));
			memset(szLevel, ' ', nLevel);

			CString strTag = bstr;
			pInfo->pfnOutputString(_T("Parsing element: %s%s"), A2T(szLevel), strTag);

			CIPtr(IHTMLImgElement) cpImgElem;
			hr = cpDisp->QueryInterface( IID_IHTMLImgElement, (void **)&cpImgElem );
			if ( hr == S_OK )
			{
				cpImgElem->get_href(&bstr);
			}
			else
			{
				CIPtr(IHTMLAnchorElement) cpAnchElem;
				hr = cpDisp->QueryInterface( IID_IHTMLAnchorElement, (void **)&cpAnchElem );
				if ( hr == S_OK )
				{
					cpAnchElem->get_href(&bstr);
				}
			}

			CIPtr(IDispatch) cpDispColl;
			cpElem->get_all(&cpDispColl);
			if (hr == S_OK)
			{
				CIPtr(IHTMLElementCollection) cpColl = cpDispColl;
				tstDrillerLevel(pInfo, cpColl, nLevel + 1);
			}
		}
	}
}

TestResult __cdecl tstDriller(BrowserInfo *pInfo)
{
	CIPtr(IHTMLDocument2) cpDocElement;
	pInfo->GetDocument(&cpDocElement);
	if (cpDocElement == NULL)
	{
		return trFailed;
	}
	
	CIPtr(IHTMLElementCollection) cpColl;
	HRESULT hr = cpDocElement->get_all( &cpColl );
	if (hr == S_OK)
	{
		tstDrillerLevel(pInfo, cpColl, 0);
	}

	return trPassed;
}

TestResult __cdecl tstTesters(BrowserInfo *pInfo)
{
	pInfo->pfnOutputString("Test architecture is reasonably sane!");
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
	{ _T("Test Tester"), _T("Ensure that the testing architecture is working"), tstTesters, trNotRun },
	{ _T("Control basics"), _T("Ensure that the browser control is active"), tstControlActive, trNotRun },
	{ _T("IWebBrowser"), _T("Test if control has an IWebBrowser interface"), tstIWebBrowser, trNotRun },
	{ _T("IWebBrowser2"), _T("Test if control has an IWebBrowser2 interface"), tstIWebBrowser2, trNotRun },
	{ _T("IWebBrowserApp"), _T("Test if control has an IWebBrowserApp interface"), tstIWebBrowserApp, trNotRun }
};

Test aDHTML[] =
{
	{ _T("get_Document"), _T("Test if browser has a top level element"), tstDocument },
	{ _T("parse DOM"), _T("Parse the document DOM"), tstDriller }
};

TestSet aTestSets[] =
{
	{ _T("Basic"), _T("Basic sanity tests"), 5, aBasic },
	{ _T("Browsing"), _T("Browsing and navigation tests"), 0, NULL },
	{ _T("DHTML"), _T("Test the DOM"), 2, aDHTML }
};

int nTestSets = sizeof(aTestSets) / sizeof(aTestSets[0]);
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

TestResult __cdecl tstDocument(BrowserInfo &cInfo)
{
	CIPtr(IHTMLDocument2) cpDocElement;
	cInfo.GetDocument(&cpDocElement);
	if (cpDocElement == NULL)
	{
		return trFailed;
	}

	return trPassed;
}


void tstDrillerLevel(BrowserInfo &cInfo, IHTMLElementCollection *pCollection, int nLevel)
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

			BSTR bstrTagName = NULL;
			hr = cpElem->get_tagName(&bstrTagName);
			CString szTagName = bstrTagName;
			SysFreeString(bstrTagName);

			BSTR bstrID = NULL;
			hr = cpElem->get_id(&bstrID);
			CString szID = bstrID;
			SysFreeString(bstrID);

			BSTR bstrClassName = NULL;
			hr = cpElem->get_className(&bstrClassName);
			CString szClassName = bstrClassName;
			SysFreeString(bstrClassName);
			
			USES_CONVERSION;
			char szLevel[256];
			memset(szLevel, 0, sizeof(szLevel));
			memset(szLevel, ' ', nLevel);

			cInfo.pfnOutputString(_T("%sElement %s"), A2T(szLevel), szTagName);
			cInfo.pfnOutputString(_T("%s  id=%s"), A2T(szLevel), szID);
			cInfo.pfnOutputString(_T("%s  classname=%s"), A2T(szLevel), szClassName);

			CIPtr(IHTMLImgElement) cpImgElem;
			hr = cpDisp->QueryInterface( IID_IHTMLImgElement, (void **)&cpImgElem );
			if ( hr == S_OK )
			{
//				cpImgElem->get_href(&bstr);
			}
			else
			{
				CIPtr(IHTMLAnchorElement) cpAnchElem;
				hr = cpDisp->QueryInterface( IID_IHTMLAnchorElement, (void **)&cpAnchElem );
				if ( hr == S_OK )
				{
//					cpAnchElem->get_href(&bstr);
				}
			}

			CIPtr(IDispatch) cpDispColl;
			hr = cpElem->get_children(&cpDispColl);
			if (hr == S_OK)
			{
				CIPtr(IHTMLElementCollection) cpColl = cpDispColl;
				tstDrillerLevel(cInfo, cpColl, nLevel + 1);
			}
		}
	}
}

TestResult __cdecl tstDriller(BrowserInfo &cInfo)
{
	CIPtr(IHTMLDocument2) cpDocElement;
	cInfo.GetDocument(&cpDocElement);
	if (cpDocElement == NULL)
	{
		return trFailed;
	}
	
	CIPtr(IHTMLElementCollection) cpColl;
	HRESULT hr = cpDocElement->get_all( &cpColl );
	if (hr == S_OK)
	{
		tstDrillerLevel(cInfo, cpColl, 0);
	}

	return trPassed;
}

TestResult __cdecl tstTesters(BrowserInfo &cInfo)
{
	cInfo.pfnOutputString("Test architecture is reasonably sane!");
	return trPassed;
}

TestResult __cdecl tstControlActive(BrowserInfo &cInfo)
{
	CControlSiteInstance *pControlSite = cInfo.pControlSite;
	if (pControlSite == NULL || !pControlSite->IsInPlaceActive())
	{
		return trFailed;
	}
	return trPassed;
}

TestResult __cdecl tstIWebBrowser(BrowserInfo &cInfo)
{
	if (cInfo.pIUnknown == NULL)
	{
		return trFailed;
	}

	CIPtr(IWebBrowser) cpIWebBrowser = cInfo.pIUnknown;
	if (cpIWebBrowser)
	{
		return trPassed;
	}

	return trFailed;
}

TestResult __cdecl tstIWebBrowser2(BrowserInfo &cInfo)
{
	if (cInfo.pIUnknown == NULL)
	{
		return trFailed;
	}
	CIPtr(IWebBrowser2) cpIWebBrowser = cInfo.pIUnknown;
	if (cpIWebBrowser)
	{
		return trPassed;
	}

	return trFailed;
}


TestResult __cdecl tstIWebBrowserApp(BrowserInfo &cInfo)
{
	if (cInfo.pIUnknown == NULL)
	{
		return trFailed;
	}

	CIPtr(IWebBrowserApp) cpIWebBrowser = cInfo.pIUnknown;
	if (cpIWebBrowser)
	{
		return trPassed;
	}

	return trFailed;
}

TestResult __cdecl tstNavigate2(BrowserInfo &cInfo)
{
	return trFailed;
}

///////////////////////////////////////////////////////////////////////////////

Test aBasic[] =
{
	{ _T("Test Tester"), _T("Ensure that the testing architecture is working"), tstTesters, trNotRun },
	{ _T("Control basics"), _T("Ensure that the browser control is active"), tstControlActive, trNotRun },
	{ _T("IWebBrowser"), _T("Test if control has an IWebBrowser interface"), tstIWebBrowser, trNotRun },
	{ _T("IWebBrowser2"), _T("Test if control has an IWebBrowser2 interface"), tstIWebBrowser2, trNotRun },
	{ _T("IWebBrowserApp"), _T("Test if control has an IWebBrowserApp interface"), tstIWebBrowserApp, trNotRun }
};

Test aBrowsing[] =
{
	{ _T("Navigate2"), _T("Test if browser can navigate to the test URL"), NULL }
};

Test aOther[] =
{
	{ _T("Print Page"), _T("Print the test URL page"), NULL }
};

Test aDHTML[] =
{
	{ _T("get_Document"), _T("Test if browser has a top level element"), tstDocument },
	{ _T("parse DOM"), _T("Parse the document DOM"), tstDriller }
};

TestSet aTestSets[] =
{
	{ _T("Basic"), _T("Basic sanity tests"), 5, aBasic },
	{ _T("Browsing"), _T("Browsing and navigation tests"), 1, aBrowsing },
	{ _T("Other"), _T("Other tests"), 1, aOther },
	{ _T("DHTML"), _T("Test the DOM"), 2, aDHTML }
};

int nTestSets = sizeof(aTestSets) / sizeof(aTestSets[0]);
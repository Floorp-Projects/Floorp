// stdafx.h : include file for standard system include files,
//  or project specific include files that are used frequently, but
//      are changed infrequently
//

#if !defined(AFX_STDAFX_H__5121F5E8_5324_11D2_93E1_000000000000__INCLUDED_)
#define AFX_STDAFX_H__5121F5E8_5324_11D2_93E1_000000000000__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#define VC_EXTRALEAN		// Exclude rarely-used stuff from Windows headers

#include <afxwin.h>         // MFC core and standard components
#include <afxext.h>         // MFC extensions
#include <afxdisp.h>        // MFC OLE automation classes
#ifndef _AFX_NO_AFXCMN_SUPPORT
#include <afxcmn.h>			// MFC support for Windows Common Controls
#endif // _AFX_NO_AFXCMN_SUPPORT

#define _ATL_APARTMENT_THREADED
// ATL headers
#include <atlbase.h>

class CBrowseModule : public CComModule
{
public:
	DWORD dwThreadID;

	LPCTSTR FindOneOf(LPCTSTR p1, LPCTSTR p2);
	virtual LONG Unlock();
	virtual LONG Lock();
};
	
extern CBrowseModule _Module;

#include <atlcom.h>
#include <atlctl.h>

#include <exdisp.h>
#include <mshtml.h>
#include <activscp.h>

#include <string>
#include <vector>
#include <map>
#include <list>

#include "..\..\ActiveXTypes.h"
#include "..\..\BrowserDiagnostics.h"
#include "..\..\PropertyList.h"
#include "..\..\PropertyBag.h"
#include "..\..\ControlSiteIPFrame.h"
#include "..\..\ControlSite.h"
#include "..\..\ActiveScriptSite.h"

#include "Tests.h"

#define SECTION_TEST             _T("Test")
#define KEY_TESTURL              _T("TestURL")
#define KEY_TESTURL_DEFAULTVALUE _T("http://www.mozilla.org")



//{{AFX_INSERT_LOCATION}}
// Microsoft Developer Studio will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_STDAFX_H__5121F5E8_5324_11D2_93E1_000000000000__INCLUDED_)

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

// stdafx.h : include file for standard system include files,
//      or project specific include files that are used frequently,
//      but are changed infrequently

#if !defined(AFX_STDAFX_H__1339B542_3453_11D2_93B9_000000000000__INCLUDED_)
#define AFX_STDAFX_H__1339B542_3453_11D2_93B9_000000000000__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#define STRICT

#define _WIN32_WINNT 0x0400
#define _ATL_APARTMENT_THREADED
#define _ATL_STATIC_REGISTRY

// Comment these out as appropriate
//#define USE_NGPREF

#ifdef MOZ_ACTIVEX_PLUGIN_SUPPORT
#define USE_PLUGIN
#endif

// ATL headers
#include <atlbase.h>
//You may derive a class from CComModule and use it if you want to override
//something, but do not change the name of _Module
extern CComModule _Module;
#include <atlcom.h>
#include <atlctl.h>
#include <mshtml.h>
#include <winsock2.h>

#ifdef USE_PLUGIN
#include <activscp.h>
#endif

// STL headers
#include <vector>
#include <list>
#include <string>

// New winsock2.h doesn't define this anymore
typedef long int32;

#include "jstypes.h"
#include "prtypes.h"

// Mozilla headers
#ifdef USE_NGPREF
#include "nsIPref.h"
#endif


#include "xp_core.h"
#include "jscompat.h"

#include "prthread.h"
#include "prprf.h"
#include "plevent.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsIEventQueueService.h"
#include "nsXPComCIID.h"
#include "nsWidgetsCID.h"
#include "nsGfxCIID.h"
#include "nsViewsCID.h"
#include "nsString.h"

#include "nsIURL.h"
#include "nsIWebShell.h"
#include "nsIContentViewer.h"
#include "nsIPresContext.h"
#include "nsIDocument.h"
#include "nsIDocumentObserver.h"
#include "nsIDocumentLoaderObserver.h" 
#include "nsIStreamListener.h"
#include "nsUnitConversion.h"
#include "nsVoidArray.h"
#include "nsCRT.h"

#include "nsIDocumentViewer.h"
#include "nsIDOMNode.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMDocument.h"
#include "nsIDOMElement.h"

// Mozilla control headers
#include "resource.h"

#include "ActiveXTypes.h"
#include "BrowserDiagnostics.h"
#include "PropertyList.h"
#include "MozillaControl.h"
#include "MozillaBrowser.h"
#include "WebShellContainer.h"
#include "PropertyBag.h"
#include "ControlSite.h"
#include "ControlSiteIPFrame.h"

#ifdef USE_PLUGIN
#include "npapi.h"
#include "nsIFactory.h"
#include "nsIPlugin.h"
#include "nsIPluginInstance.h"
#include "ActiveXPlugin.h"
#include "ActiveXPluginInstance.h"
#endif

#include "guids.h"

//{{AFX_INSERT_LOCATION}}
// Microsoft Developer Studio will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_STDAFX_H__1339B542_3453_11D2_93B9_000000000000__INCLUDED)

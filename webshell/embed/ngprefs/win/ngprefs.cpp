/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#define INITGUID
#include <windows.h>
#ifndef _WIN32
#include <ole2ui.h>
#include <dispatch.h>
#endif
#include "xp_mem.h"
#include "prprf.h"
#include "ngprefs.h"
#include <olectl.h>
#include "winprefs/isppageo.h"
#include "winprefs/brprefid.h"
#include "winprefs/wprefid.h"
#include "nsIDefaultBrowser.h"
#include "winprefs/intlfont.h"
#include "winprefs/ibrprefs.h"
#include "winprefs/prefuiid.h"
#include "winprefs/prefui.h"

#include "prefapi.h"
#include "globals.h"

extern "C" {
#include "xpgetstr.h"
};

BOOL	g_bReloadAllWindows;
BOOL    g_bReloadChangeColor;

char *FE_GetProgramDirectory(char *buffer, int length)
{
    ::GetModuleFileName(g_hInst, buffer, length);
    char *tmp = strrchr(buffer, '\\');
    if (tmp) {
        *tmp = '\0';
    }
    return buffer;
}

//  Use this to create some COM objects, as we need to switch to the
//      directory where the .EXE sits if the current directory isn't
//      the same, as the pref COM DLLs have relative paths in the
//      registry.
HRESULT FEU_CoCreateInstance(REFCLSID rclsid, LPUNKNOWN pUnkOuter, DWORD dwClsContext, REFIID riid, LPVOID *ppv)
{
    HRESULT hRetval = NULL;

    char aOrigDir[MAX_PATH + 1];
    DWORD dwCheck = GetCurrentDirectory(sizeof(aOrigDir), aOrigDir);
    PR_ASSERT(dwCheck);

    char aProgramDir[MAX_PATH + 1];
    FE_GetProgramDirectory(aProgramDir, sizeof(aProgramDir));

    BOOL bCheck = SetCurrentDirectory(aProgramDir);
    PR_ASSERT(bCheck);

    hRetval = CoCreateInstance(rclsid, pUnkOuter, dwClsContext, riid, ppv);

    BOOL bRestoreCheck = SetCurrentDirectory(aOrigDir);
    PR_ASSERT(bRestoreCheck);

    return(hRetval);
}

/////////////////////////////////////////////////////////////////////////////
// Helper functions

#ifndef _WIN32
static LPVOID
CoTaskMemAlloc(ULONG cb)
{
	LPMALLOC	pMalloc;
	
	if (SUCCEEDED(CoGetMalloc(MEMCTX_TASK, &pMalloc))) {
		LPVOID	pv = pMalloc->Alloc(cb);

		pMalloc->Release();
		return pv;
	}

	return NULL;
}
#endif

// Convert an ANSI string to an OLE string (UNICODE string)
static LPOLESTR NEAR
AllocTaskOleString(LPCSTR lpszString)
{
	LPOLESTR	lpszResult;
	UINT		nLen;

	if (lpszString == NULL)
		return NULL;

	// Convert from ANSI to UNICODE
	nLen = lstrlen(lpszString) + 1;
	lpszResult = (LPOLESTR)CoTaskMemAlloc(nLen * sizeof(OLECHAR));
	if (lpszResult)	{
#ifdef _WIN32
		MultiByteToWideChar(CP_ACP, 0, lpszString, -1, lpszResult, nLen);
#else
		lstrcpy(lpszResult, lpszString);  // Win 16 doesn't use any UNICODE
#endif
	}

	return lpszResult;
}

#ifdef XXX

/////////////////////////////////////////////////////////////////////////////
// CEnumHelpers

// Helper class to encapsulate enumeration of the helper apps
class CEnumHelpers : public IEnumHelpers {
	public:
		CEnumbHelpers();
		
		// IUnknown methods
		STDMETHODIMP QueryInterface(REFIID riid, LPVOID FAR* ppvObj);
		STDMETHODIMP_(ULONG) AddRef();
		STDMETHODIMP_(ULONG) Release();

		// IEnumHelpers methods
		STDMETHODIMP		 Next(NET_cdataStruct **ppcdata);
		STDMETHODIMP		 Reset();

	private:
		ULONG		m_uRef;
		XP_List	   *m_pInfoList;
};

CEnumHelpers::CEnumHelpers()
{
	m_uRef = 0;
	m_pInfoList = cinfo_MasterListPointer();
}

STDMETHODIMP
CEnumHelpers::QueryInterface(REFIID riid, LPVOID FAR* ppvObj)
{
	*ppvObj = NULL;

	if (riid == IID_IUnknown || riid == IID_IEnumHelpers) {
		*ppvObj = (LPVOID)this;
		AddRef();
		return NOERROR;
	}

	return ResultFromScode(E_NOINTERFACE);
}

STDMETHODIMP_(ULONG)
CEnumHelpers::AddRef()
{
	return ++m_uRef;
}

STDMETHODIMP_(ULONG)
CEnumHelpers::Release()
{
	if (--m_uRef == 0) {
		delete this;
		return 0;
	}

	return m_uRef;
}

STDMETHODIMP
CEnumHelpers::Next(NET_cdataStruct **ppcdata)
{
	CHelperApp	*pHelperApp;

  while ((*ppcdata = (NET_cdataStruct *)XP_ListNextObject(m_pInfoList))) {
		// Ignore items that don't have a MIME type
		if (!(*ppcdata)->ci.type)
			continue;

		// Don't give the user an opportunity to change application/octet-stream
		// or proxy auto-config
		if (strcmp((*ppcdata)->ci.type, APPLICATION_OCTET_STREAM) == 0 ||
			strcmp((*ppcdata)->ci.type, APPLICATION_NS_PROXY_AUTOCONFIG) == 0) {
			continue;
		}

		// Ignore items that don't have a description
		if (!(*ppcdata)->ci.desc) {
#ifdef DEBUG
			TRACE1("PREFS: Ignoring MIME type %s\n", (*ppcdata)->ci.type);
#endif
			continue;
		}
		
		// Make sure there's a CHelperApp associated with the item
		pHelperApp = (CHelperApp *)(*ppcdata)->ci.fe_data;

		if (!pHelperApp) {
#ifdef DEBUG
			TRACE1("PREFS: Ignoring MIME type %s\n", (*ppcdata)->ci.type);
#endif
			continue;
		}

		// Ignore items that have HANDLE_UNKNOWN or HANDLE_MOREINFO as how to handle
		if (pHelperApp->how_handle == HANDLE_UNKNOWN || pHelperApp->how_handle == HANDLE_MOREINFO) {
#ifdef DEBUG
			TRACE1("PREFS: Ignoring MIME type %s\n", (*ppcdata)->ci.type);
#endif
			continue;
		}

		return NOERROR;
	}

	return ResultFromScode(S_FALSE);
}

STDMETHODIMP
CEnumHelpers::Reset()
{
	m_pInfoList = cinfo_MasterListPointer();
	return NOERROR;
}

#endif /* XXX */

/////////////////////////////////////////////////////////////////////////////
// CBrowserPrefs

class CBrowserPrefs : public IBrowserPrefs {
	public:
		CBrowserPrefs(LPCSTR lpszCurrentPage);

		// IUnknown methods
		STDMETHODIMP QueryInterface(REFIID riid, LPVOID FAR* ppvObj);
		STDMETHODIMP_(ULONG) AddRef();
		STDMETHODIMP_(ULONG) Release();
		
		// IBrowserPrefs methods
		STDMETHODIMP GetCurrentPage(LPOLESTR *lpoleStr);

		STDMETHODIMP EnumHelpers(LPENUMHELPERS *ppenumHelpers);
		STDMETHODIMP GetHelperInfo(NET_cdataStruct *, LPHELPERINFO);
		STDMETHODIMP SetHelperInfo(NET_cdataStruct *, LPHELPERINFO);
		STDMETHODIMP NewFileType(LPCSTR lpszDescription,
								 LPCSTR lpszExtension,
								 LPCSTR lpszMimeType,
								 LPCSTR lpszOpenCmd,
								 NET_cdataStruct **ppcdata);
		STDMETHODIMP RemoveFileType(NET_cdataStruct *);
		// Initialization routine to create contained and aggregated objects
		HRESULT		 Init();

	private:
		ULONG	  	m_uRef;
		LPUNKNOWN 	m_pCategory;  // inner object supporting ISpecifyPropertyPageObjects
		LPCSTR		m_lpszCurrentPage;
};

CBrowserPrefs::CBrowserPrefs(LPCSTR lpszCurrentPage)
{
	m_uRef = 0;
	m_pCategory = NULL;
	m_lpszCurrentPage = lpszCurrentPage ? strdup(lpszCurrentPage) : NULL;
}

HRESULT
CBrowserPrefs::Init()
{
	// Create the object as part of an aggregate
	return FEU_CoCreateInstance(CLSID_BrowserPrefs, (LPUNKNOWN)this,
		CLSCTX_INPROC_SERVER, IID_IUnknown, (LPVOID *)&m_pCategory);
}

STDMETHODIMP
CBrowserPrefs::QueryInterface(REFIID riid, LPVOID FAR* ppvObj)
{
	*ppvObj = NULL;

	if (riid == IID_IUnknown || riid == IID_IBrowserPrefs) {
		*ppvObj = (LPVOID)this;
		AddRef();
		return NOERROR;

	} else if (riid == IID_ISpecifyPropertyPageObjects) {
		assert(m_pCategory);
		return m_pCategory->QueryInterface(riid, ppvObj);
	}

	return ResultFromScode(E_NOINTERFACE);
}

STDMETHODIMP_(ULONG)
CBrowserPrefs::AddRef()
{
	return ++m_uRef;
}

STDMETHODIMP_(ULONG)
CBrowserPrefs::Release()
{
	if (--m_uRef == 0) {
		if (m_pCategory)
			m_pCategory->Release();
		if (m_lpszCurrentPage)
			XP_FREE((void *)m_lpszCurrentPage);
		delete this;
		return 0;
	}

	return m_uRef;
}

STDMETHODIMP
CBrowserPrefs::GetCurrentPage(LPOLESTR *lpoleStr)
{
	if (!lpoleStr)
		return ResultFromScode(E_INVALIDARG);
	
	*lpoleStr = 0;

	if (m_lpszCurrentPage) {
		*lpoleStr = AllocTaskOleString(m_lpszCurrentPage);
		
		if (!*lpoleStr)
			return ResultFromScode(E_OUTOFMEMORY);
	}

	return NOERROR;
}

STDMETHODIMP
CBrowserPrefs::EnumHelpers(LPENUMHELPERS *ppEnumHelpers)
{
#ifdef XXX
	CEnumHelpers   *pEnumHelpers = new CEnumHelpers;
	HRESULT			hres;
	
	if (!pEnumHelpers)
		return ResultFromScode(E_OUTOFMEMORY);

	pEnumHelpers->AddRef();
	hres = pEnumHelpers->QueryInterface(IID_IEnumHelpers, (LPVOID *)ppEnumHelpers);
	pEnumHelpers->Release();
	return hres;
#else
    return E_NOTIMPL;
#endif
}


// Returns the command string value (path and filename for the application plus any
// command line options) associated with the given file extension
static BOOL
GetOpenCommandForExt(LPCSTR lpszExt, LPSTR lpszCmdString, DWORD cbCmdString)
{
	char    szBuf[_MAX_PATH + 32];
	char	szFileClass[60];
	LONG	lResult;
	LONG	lcb;
#ifdef _WIN32
	DWORD	cbData;
	DWORD	dwType;
#else
	LONG	cbData;
#endif

	*lpszCmdString = '\0';
	
	// Look up the file association key which maps a file extension
	// to an application identifier (also called the file type class
	// identifier or just file class)
	PR_snprintf(szBuf, sizeof(szBuf), ".%s", lpszExt);
	lcb = sizeof(szFileClass);
	lResult = RegQueryValue(HKEY_CLASSES_ROOT, szBuf, szFileClass, &lcb);

#ifdef _WIN32
	PR_ASSERT(lResult != ERROR_MORE_DATA);
#endif
	if (lResult != ERROR_SUCCESS)
		return FALSE;

	// Get the key for shell\open\command
	HKEY	hKey;
	
#ifdef _WIN32
	PR_snprintf(szBuf, sizeof(szBuf), "%s\\shell\\open\\command", szFileClass);
	lResult = RegOpenKeyEx(HKEY_CLASSES_ROOT, szBuf, 0, KEY_QUERY_VALUE, &hKey);
#else
	PR_snprintf(szBuf, sizeof(szBuf), "%s\\shell\\open\\command", szFileClass);
	lResult = RegOpenKey(HKEY_CLASSES_ROOT, szBuf, &hKey);
#endif
	if (lResult != ERROR_SUCCESS)
		return FALSE;

	// Get the value of the key
	cbData = sizeof(szBuf);
#ifdef _WIN32
	lResult = RegQueryValueEx(hKey, NULL, NULL, &dwType, (LPBYTE)szBuf, &cbData);
#else
	lResult = RegQueryValue(hKey, NULL, (LPSTR)szBuf, &cbData);
#endif
	RegCloseKey(hKey);
	if (lResult != ERROR_SUCCESS)
		return FALSE;

#ifdef _WIN32
	// Win32 doesn't expand automatically environment variables (for value
	// data of type REG_EXPAND_SZ). We need this for things like %SystemRoot%
	if (dwType == REG_EXPAND_SZ)
		ExpandEnvironmentStrings(szBuf, lpszCmdString, cbCmdString);
	else
		lstrcpy(lpszCmdString, szBuf);
#else
	lstrcpy(lpszCmdString, szBuf);
#endif

    return TRUE;
}

#ifdef XXX
// Returns the path of the helper application associated with the
// given pcdata
static BOOL
GetApplication(NET_cdataStruct *pcdata, LPSTR lpszApp)
{
	CHelperApp *pHelperApp = (CHelperApp *)pcdata->ci.fe_data;
	char   	   *lpszFile;
	char		szExtension[_MAX_EXT];
	LPSTR		lpszStrip;

	if (!pHelperApp)
		return FALSE;

	// How we do this depends on how it's handled
	switch (pHelperApp->how_handle) {
		case HANDLE_EXTERNAL:
			lstrcpy(lpszApp, (LPCSTR)pHelperApp->csCmd);

			// XXX - this is what the code in display.cpp does, but it's really
			// wrong...
			lpszStrip = strrchr(lpszApp, '%');

			if (lpszStrip)
				*lpszStrip = '\0';
			return TRUE;

		case HANDLE_SHELLEXECUTE:
		case HANDLE_BY_OLE:
			// Have FindExecutable() tell us what the executable name is
			wsprintf(szExtension, ".%s", pcdata->exts[0]);
			lpszFile = WH_TempFileName(xpTemporary, "M", szExtension);
			if (lpszFile) {
				BOOL	bResult = FEU_FindExecutable(lpszFile, lpszApp, TRUE);

				XP_FREE(lpszFile);
				return bResult;
			}
			return FALSE;

		default:
			ASSERT(FALSE);
			return FALSE;
	}
}

#endif /* XXX */

//Begin CRN_MIME
static BOOL
IsMimeTypeLocked(const char *pPrefix)
{
	BOOL bRet = FALSE;

	//???????Any holes here??????
	//I'm looking at just the ".type" field to see if a pref is locked. 
	char* setup_buf = PR_smprintf("%s.%s", pPrefix, "mimetype");

	bRet = PREF_PrefIsLocked(setup_buf);

	if(setup_buf && setup_buf[0])
		XP_FREEIF(setup_buf);

	return bRet;
}

#ifdef XXX
static int
GetPrefLoadAction(int how_handle)
{
	switch(how_handle) {
	case HANDLE_SAVE:
			return 1;

		case HANDLE_SHELLEXECUTE: 
			return 2;

		case HANDLE_UNKNOWN:
			return 3;

		case HANDLE_VIA_PLUGIN:
			return 4;

		default:
			return 2;
	}
}
//End CRN_MIME

#endif
STDMETHODIMP
CBrowserPrefs::GetHelperInfo(NET_cdataStruct *pcdata, LPHELPERINFO lpInfo)
{
#ifdef XXX
	CHelperApp	*pHelperApp;
    char         szApp[_MAX_PATH];
	
	if (!pcdata || !lpInfo)
		return ResultFromScode(E_INVALIDARG);

	pHelperApp = (CHelperApp *)pcdata->ci.fe_data;
	if (!pHelperApp)
		return ResultFromScode(E_UNEXPECTED);

	//Begin CRN_MIME
	if(! pHelperApp->csMimePrefPrefix.IsEmpty())
	{
		//This is a mime type associcated with a helper for a type specified thru' prefs.
		//Use the csMimePrefPrefix to generate the pref and see if it's locked.
		lpInfo->bIsLocked = IsMimeTypeLocked((LPCSTR)pHelperApp->csMimePrefPrefix);
		
	}
	else
		lpInfo->bIsLocked = FALSE;
	//End CRN_MIME

	lpInfo->nHowToHandle = pHelperApp->how_handle;
	lpInfo->szOpenCmd[0] = '\0';

	switch (pHelperApp->how_handle) {
		case HANDLE_EXTERNAL:
			lstrcpy(lpInfo->szOpenCmd, (LPCSTR)pHelperApp->csCmd);
            if (GetApplication(pcdata, szApp))
			    lpInfo->bAskBeforeOpening = theApp.m_pSpawn->PromptBeforeOpening(szApp);
			break;

		case HANDLE_SHELLEXECUTE:
		case HANDLE_BY_OLE:
			ASSERT(pcdata->num_exts > 0);
			if (pcdata->num_exts > 0) {
				GetOpenCommandForExt(pcdata->exts[0], lpInfo->szOpenCmd, sizeof(lpInfo->szOpenCmd));
                if (GetApplication(pcdata, szApp))
				    lpInfo->bAskBeforeOpening = theApp.m_pSpawn->PromptBeforeOpening(szApp);
			}
			break;

		case HANDLE_SAVE:
			if (pcdata->num_exts > 0) {
				// Even though it's marked as Save to Disk get the shell open command anyway. This
				// way if the user changes the association to launch the application they'll already
				// have it
				GetOpenCommandForExt(pcdata->exts[0], lpInfo->szOpenCmd, sizeof(lpInfo->szOpenCmd));
				lpInfo->bAskBeforeOpening = TRUE;
			}
			break;
//~~~
		case HANDLE_VIA_PLUGIN:
		case HANDLE_VIA_PLUGINAPPLET:
			if (pcdata->num_exts > 0) {
				GetOpenCommandForExt(pcdata->exts[0], lpInfo->szOpenCmd, sizeof(lpInfo->szOpenCmd));
				lpInfo->bAskBeforeOpening = FALSE;
			}
			break;
		default:
			break;
	}
	
	return NOERROR;
#else
    return E_NOTIMPL;
#endif
}

STDMETHODIMP
CBrowserPrefs::SetHelperInfo(NET_cdataStruct *pcdata, LPHELPERINFO lpInfo)
{
#ifdef XXX
    fe_ChangeFileType(pcdata, lpInfo->lpszMimeType, lpInfo->nHowToHandle, lpInfo->szOpenCmd);

//Begin CRN_MIME
	//Set user pref values only for mime types setup from the pref file
	CHelperApp	*pHelperApp;
	pHelperApp = (CHelperApp *)pcdata->ci.fe_data;
	if(pHelperApp)
	{
		if(! pHelperApp->csMimePrefPrefix.IsEmpty())
		{
			char* setup_buf = PR_smprintf("%s.%s", (LPCSTR)pHelperApp->csMimePrefPrefix, "mimetype");
			PREF_SetCharPref(setup_buf, lpInfo->lpszMimeType);
			if(setup_buf && setup_buf[0])
				XP_FREEIF(setup_buf);

			setup_buf = PR_smprintf("%s.%s", (LPCSTR)pHelperApp->csMimePrefPrefix, "win_appname");
			PREF_SetCharPref(setup_buf, lpInfo->szOpenCmd);
			if(setup_buf && setup_buf[0])
				XP_FREEIF(setup_buf);

			setup_buf = PR_smprintf("%s.%s", (LPCSTR)pHelperApp->csMimePrefPrefix, "load_action");
			PREF_SetIntPref(setup_buf, GetPrefLoadAction(lpInfo->nHowToHandle));
			if(setup_buf && setup_buf[0])
				XP_FREEIF(setup_buf);
		}
	}

//End CRN_MIME

	if (lpInfo->nHowToHandle == HANDLE_EXTERNAL ||
		lpInfo->nHowToHandle == HANDLE_SHELLEXECUTE ||
		lpInfo->nHowToHandle == HANDLE_BY_OLE) {

		// Update whether we should prompt before opening the file
        char    szApp[_MAX_PATH];

        if (GetApplication(pcdata, szApp))
		    theApp.m_pSpawn->SetPromptBeforeOpening(szApp, lpInfo->bAskBeforeOpening);
	}
	return NOERROR;
#else
    return E_NOTIMPL;
#endif
}

STDMETHODIMP
CBrowserPrefs::NewFileType(LPCSTR lpszDescription,
						   LPCSTR lpszExtension,
						   LPCSTR lpszMimeType,
						   LPCSTR lpszOpenCmd,
						   NET_cdataStruct **ppcdata)
{
#ifdef XXX
	*ppcdata = fe_NewFileType(lpszDescription, lpszExtension, lpszMimeType, lpszOpenCmd);
	return *ppcdata ? NOERROR : ResultFromScode(S_FALSE);
#else
    return E_NOTIMPL;
#endif
}

STDMETHODIMP
CBrowserPrefs::RemoveFileType(NET_cdataStruct *pcdata)
{
#ifdef XXX
//Begin CRN_MIME
	//Clear user pref values only for mime types setup from the pref file
	CHelperApp	*pHelperApp;
	pHelperApp = (CHelperApp *)pcdata->ci.fe_data;
	if(pHelperApp)
	{
		if(! pHelperApp->csMimePrefPrefix.IsEmpty())
		{
			char* setup_buf = PR_smprintf("%s.%s", (LPCSTR)pHelperApp->csMimePrefPrefix, "mimetype");
			PREF_ClearUserPref(setup_buf);
			if(setup_buf && setup_buf[0])
				XP_FREEIF(setup_buf);

			setup_buf = PR_smprintf("%s.%s", (LPCSTR)pHelperApp->csMimePrefPrefix, "win_appname");
			PREF_ClearUserPref(setup_buf);
			if(setup_buf && setup_buf[0])
				XP_FREEIF(setup_buf);

			setup_buf = PR_smprintf("%s.%s", (LPCSTR)pHelperApp->csMimePrefPrefix, "load_action");
			PREF_ClearUserPref(setup_buf);
			if(setup_buf && setup_buf[0])
				XP_FREEIF(setup_buf);

			setup_buf = PR_smprintf("%s.%s", (LPCSTR)pHelperApp->csMimePrefPrefix, "extension");
			PREF_ClearUserPref(setup_buf);
			if(setup_buf && setup_buf[0])
				XP_FREEIF(setup_buf);

			setup_buf = PR_smprintf("%s.%s", (LPCSTR)pHelperApp->csMimePrefPrefix, "description");
			PREF_ClearUserPref(setup_buf);
			if(setup_buf && setup_buf[0])
				XP_FREEIF(setup_buf);
		}
	}
//End CRN_MIME

	return fe_RemoveFileType(pcdata) ? NOERROR : ResultFromScode(S_FALSE);
#else
    return E_NOTIMPL;
#endif
}

/////////////////////////////////////////////////////////////////////////////
// CAppearancePreferences

class CAppearancePrefs : public IIntlFont {
	public:
		CAppearancePrefs(int nCharsetId);

		// IUnknown methods
		STDMETHODIMP QueryInterface(REFIID riid, LPVOID FAR* ppvObj);
		STDMETHODIMP_(ULONG) AddRef();
		STDMETHODIMP_(ULONG) Release();
		
		// IIntlFont methods
		STDMETHODIMP GetNumEncodings(LPDWORD pdwEncodings);
		STDMETHODIMP GetEncodingName(DWORD dwCharsetNum, LPOLESTR *pszName);
		STDMETHODIMP GetEncodingInfo(DWORD dwCharsetNum, LPENCODINGINFO lpInfo);
		STDMETHODIMP SetEncodingFonts(DWORD dwCharsetNum, LPENCODINGINFO lpInfo);
		STDMETHODIMP GetCurrentCharset(LPDWORD pdwCharsetNum);

		// Initialization routine to create contained and aggregated objects
		HRESULT		 Init();

	private:
		ULONG	  m_uRef;
		int		  m_nCharsetId;
		LPUNKNOWN m_pCategory;  // inner object supporting ISpecifyPropertyPageObjects
};

CAppearancePrefs::CAppearancePrefs(int nCharsetId)
{
	m_uRef = 0;
	m_nCharsetId = nCharsetId;
	m_pCategory = NULL;
}

HRESULT
CAppearancePrefs::Init()
{
	// Create the object as part of an aggregate
	return FEU_CoCreateInstance(CLSID_AppearancePrefs, (LPUNKNOWN)this,
		CLSCTX_INPROC_SERVER, IID_IUnknown, (LPVOID *)&m_pCategory);
}

STDMETHODIMP
CAppearancePrefs::QueryInterface(REFIID riid, LPVOID FAR* ppvObj)
{
	*ppvObj = NULL;

	if (riid == IID_IUnknown || riid == IID_IIntlFont) {
		*ppvObj = (LPVOID)this;
		AddRef();
		return NOERROR;

	} else if (riid == IID_ISpecifyPropertyPageObjects) {
		assert(m_pCategory);
		return m_pCategory->QueryInterface(riid, ppvObj);
	}

	return ResultFromScode(E_NOINTERFACE);
}

STDMETHODIMP_(ULONG)
CAppearancePrefs::AddRef()
{
	return ++m_uRef;
}

STDMETHODIMP_(ULONG)
CAppearancePrefs::Release()
{
	if (--m_uRef == 0) {
		if (m_pCategory)
			m_pCategory->Release();
		delete this;
		return 0;
	}

	return m_uRef;
}

STDMETHODIMP
CAppearancePrefs::GetNumEncodings(LPDWORD pdwEncodings)
{
	if (!pdwEncodings)
		return ResultFromScode(E_INVALIDARG);

#ifdef XXX	
	*pdwEncodings = (DWORD)MAXLANGNUM;
#else
	*pdwEncodings = (DWORD)0;
#endif
	return NOERROR;
}

STDMETHODIMP
CAppearancePrefs::GetEncodingName(DWORD dwCharsetNum, LPOLESTR *lpoleName)
{
#ifdef XXX
	if (dwCharsetNum >= (DWORD)MAXLANGNUM || !lpoleName)
		return ResultFromScode(E_INVALIDARG);

	LPCSTR	lpszName = theApp.m_pIntlFont->GetEncodingName((int)dwCharsetNum);

	if (lpszName) {
		*lpoleName = AllocTaskOleString(lpszName);
		return *lpoleName ? NOERROR : ResultFromScode(E_OUTOFMEMORY);
	}
#endif
	
	return ResultFromScode(E_UNEXPECTED);
}

STDMETHODIMP
CAppearancePrefs::GetEncodingInfo(DWORD dwCharsetNum, LPENCODINGINFO lpInfo)
{
#ifdef XXX
	EncodingInfo	*lpEncoding;

	if (dwCharsetNum >= (DWORD)MAXLANGNUM || !lpInfo)
		return ResultFromScode(E_INVALIDARG);
	
	lpEncoding = theApp.m_pIntlFont->GetEncodingInfo((int)dwCharsetNum);
	lpInfo->bIgnorePitch = ( CIntlWin::FontSelectIgnorePitch(lpEncoding->iCSID) );
	lstrcpy(lpInfo->szVariableWidthFont, lpEncoding->szPropName);
	lpInfo->nVariableWidthSize = lpEncoding->iPropSize;
	lstrcpy(lpInfo->szFixedWidthFont, lpEncoding->szFixName);
	lpInfo->nFixedWidthSize = lpEncoding->iFixSize;
#endif
	return NOERROR;
}

STDMETHODIMP
CAppearancePrefs::SetEncodingFonts(DWORD dwCharsetNum, LPENCODINGINFO lpInfo)
{	
#ifdef XXX	
    EncodingInfo   *lpEncoding;
	BOOL			bFixedWidthChanged;
	BOOL			bVariableWidthChanged;

	if (dwCharsetNum >= (DWORD)MAXLANGNUM || !lpInfo)
		return ResultFromScode(E_INVALIDARG);

	lpEncoding = theApp.m_pIntlFont->GetEncodingInfo((int)dwCharsetNum);

	// See if anything has changed. We don't want to reset everything
	// if nothing has changed
	bFixedWidthChanged = lstrcmp(lpInfo->szFixedWidthFont, lpEncoding->szFixName) != 0 ||
		lpInfo->nFixedWidthSize != lpEncoding->iFixSize;
	bVariableWidthChanged = lstrcmp(lpInfo->szVariableWidthFont, lpEncoding->szPropName) != 0 ||
		lpInfo->nVariableWidthSize != lpEncoding->iPropSize;

	if (bFixedWidthChanged || bVariableWidthChanged) {
		lstrcpy(lpEncoding->szFixName, lpInfo->szFixedWidthFont);
		lpEncoding->iFixSize = lpInfo->nFixedWidthSize;
		lstrcpy(lpEncoding->szPropName, lpInfo->szVariableWidthFont);
		lpEncoding->iPropSize = lpInfo->nVariableWidthSize;
    	
		// Reset the assorted font caches...
		CDCCX::ClearAllFontCaches();
#ifdef MOZ_NGLAYOUT
  XP_ASSERT(0);
#else
		CVirtualFontFontCache::Reset();
#endif /* MOZ_NGLAYOUT */
		theApp.m_pIntlFont->WriteToIniFile();

		// Indicate we need to reload all of the windows
		g_bReloadAllWindows = TRUE;
	}
#endif	
	return NOERROR;
}

STDMETHODIMP
CAppearancePrefs::GetCurrentCharset(LPDWORD pdwCharsetNum)
{
	if (!pdwCharsetNum)
		return ResultFromScode(E_INVALIDARG);

	*pdwCharsetNum = (DWORD)m_nCharsetId;
	return NOERROR;
}

typedef HRESULT (*DllServerFunction)(void);

static BOOL
RegisterCLSIDForDll(char * dllName)
{
	BOOL 	bRetval = FALSE;
	char    szLib[_MAX_PATH + 32];
	FE_GetProgramDirectory( szLib, _MAX_PATH + 32 );
	if ( *szLib ) {
		strcat( szLib, dllName );

		if(szLib)  {
			HINSTANCE hLibInstance = hLibInstance = LoadLibrary(szLib);
#ifdef WIN32
			if(hLibInstance)
#else
			if(hLibInstance > (HINSTANCE)HINSTANCE_ERROR)
#endif
			{
				FARPROC RegistryFunc = NULL;

				RegistryFunc = ::GetProcAddress(hLibInstance, "DllRegisterServer");
							
				if(RegistryFunc)   {
					HRESULT hResult = (RegistryFunc)();

					if(GetScode(hResult) == S_OK)   {
						bRetval = TRUE;
					}

					RegistryFunc = NULL;
				}
				else    {
					/* If the DLL doesn't have those functions then it just doesn't support
					 * self-registration. We don't consider that to be an error
					 *
					 * We should consider checking for the "OleSelfRegister" string in the
					 * StringFileInfo section of the version information resource. If the DLL
					 * has "OleSelfRegister" but doesn't have the self-registration functions
					 * then that would be an error
					 */
					bRetval = TRUE;
				}

				FreeLibrary(hLibInstance);
				hLibInstance = NULL;
			}
		}
	}
			
	return bRetval;
}

#ifdef OLD_MOZ_MAIL_NEWS
static BOOL
CreateMailNewsCategory(MWContext *pContext, LPSPECIFYPROPERTYPAGEOBJECTS *pCategory)
{
	BOOL	bResult = FALSE;

	// Create the property page providers
	CMailNewsPreferences *pMailNews = new CMailNewsPreferences(pContext);
	pMailNews->AddRef();

	// Initialize the mail news object. This allows it to load any objects that are
	// contained or aggregated
	if (SUCCEEDED(pMailNews->Init())) {
		// Get the interface pointer for ISpecifyPropertyPageObjects
		if (SUCCEEDED(pMailNews->QueryInterface(IID_ISpecifyPropertyPageObjects, (LPVOID *)pCategory)))
			bResult = TRUE;
	}
	else {
		// Try to register the dll and initialize again
#ifdef _WIN32
		bResult = RegisterCLSIDForDll("mnpref32.dll");
#else
		bResult = RegisterCLSIDForDll("mnpref16.dll");
#endif
		if (bResult) {
			if (SUCCEEDED(pMailNews->Init())) {
				// Get the interface pointer for ISpecifyPropertyPageObjects
				if (SUCCEEDED(pMailNews->QueryInterface(IID_ISpecifyPropertyPageObjects, (LPVOID *)pCategory)))
					bResult = TRUE;
			}
			else
				bResult = FALSE;
		}
	}
	
	// We're all done with the object
	pMailNews->Release();
	return bResult;
}
#endif /* OLD_MOZ_MAIL_NEWS */

#ifdef MOZ_LOC_INDEP
/////////////////////////////////////////////////////////////////////////////
// CLIPreference

class CLIPreference : public ILIPrefs {
	public:
		CLIPreference();

		// IUnknown methods
		STDMETHODIMP QueryInterface(REFIID riid, LPVOID FAR* ppvObj);
		STDMETHODIMP_(ULONG) AddRef();
		STDMETHODIMP_(ULONG) Release();
		
		// Initialization routine to create contained and aggregated objects
		HRESULT		 Init();

	private:

		ULONG	  m_uRef;
		LPUNKNOWN m_pCategory;  // inner object supporting ISpecifyPropertyPageObjects
};

/////////////////////////////////////////////////////////////////////////////
// CLIPreference

CLIPreference::CLIPreference()
{
	m_uRef = 0;
	m_pCategory = NULL;
}

HRESULT
CLIPreference::Init()
{
	// Create the object as part of an aggregate
	return FEU_CoCreateInstance(CLSID_LIPrefs, (LPUNKNOWN)this,
		CLSCTX_INPROC_SERVER, IID_IUnknown, (LPVOID *)&m_pCategory);
}

STDMETHODIMP
CLIPreference::QueryInterface(REFIID riid, LPVOID FAR* ppvObj)
{
	*ppvObj = NULL;

	if (riid == IID_IUnknown || riid == IID_ILIlogin) {
		*ppvObj = (LPVOID)this;
		AddRef();
		return NOERROR;

	} else if (riid == IID_ISpecifyPropertyPageObjects) {
		assert(m_pCategory);
		return m_pCategory->QueryInterface(riid, ppvObj);
	}

	return ResultFromScode(E_NOINTERFACE);
}

STDMETHODIMP_(ULONG)
CLIPreference::AddRef()
{
	return ++m_uRef;
}

STDMETHODIMP_(ULONG)
CLIPreference::Release()
{
	if (--m_uRef == 0) {
		if (m_pCategory)
			m_pCategory->Release();
		delete this;
		return 0;
	}

	return m_uRef;
}
#endif /* MOZ_LOC_INDEP */

#ifdef MOZ_SMARTUPDATE
/////////////////////////////////////////////////////////////////////////////
// CSmartUpdatePreference

class CSmartUpdatePrefs : public ISmartUpdatePrefs {
	public:
		CSmartUpdatePrefs();

		// IUnknown methods
		STDMETHODIMP QueryInterface(REFIID riid, LPVOID FAR* ppvObj);
		STDMETHODIMP_(ULONG) AddRef();
		STDMETHODIMP_(ULONG) Release();
		
        // ISmartUpdatePrefs methods
        STDMETHODIMP_(LONG) Uninstall(char* regPackageName);
        STDMETHODIMP_(LONG) EnumUninstall(void** context, char* packageName,
                                    LONG len1, char*regPackageName, LONG len2);

	private:

		ULONG	  m_uRef;
};

/////////////////////////////////////////////////////////////////////////////
// CSmartUpdatePrefs
CSmartUpdatePrefs::CSmartUpdatePrefs()
{
	m_uRef = 0;
}

STDMETHODIMP
CSmartUpdatePrefs::QueryInterface(REFIID riid, LPVOID FAR* ppvObj)
{
	*ppvObj = NULL;

	if (riid == IID_IUnknown || riid == IID_ISmartUpdatePrefs) {
		*ppvObj = (LPVOID)this;
		AddRef();
		return NOERROR;

	} 

	return ResultFromScode(E_NOINTERFACE);
}

STDMETHODIMP_(ULONG)
CSmartUpdatePrefs::AddRef()
{
	return ++m_uRef;
}

STDMETHODIMP_(ULONG)
CSmartUpdatePrefs::Release()
{
	if (--m_uRef == 0) {
		delete this;
		return 0;
	}

	return m_uRef;
}

STDMETHODIMP_(LONG)
CSmartUpdatePrefs::Uninstall(char* regPackageName)
{
	return SU_Uninstall(regPackageName);
}

STDMETHODIMP_(LONG)
CSmartUpdatePrefs::EnumUninstall(void** context, char* packageName,
                                 LONG len1, char*regPackageName, LONG len2)
{
	return SU_EnumUninstall(context, packageName,len1, regPackageName,len2);
}

#endif /* MOZ_SMARTUPDATE */


/////////////////////////////////////////////////////////////////////////////
// CAdvancedPrefs

class CAdvancedPrefs : public IAdvancedPrefs {
	public:
		CAdvancedPrefs();

		// IUnknown methods
		STDMETHODIMP QueryInterface(REFIID riid, LPVOID FAR* ppvObj);
		STDMETHODIMP_(ULONG) AddRef();
		STDMETHODIMP_(ULONG) Release();
		
       	// Initialization routine to create contained and aggregated objects
		HRESULT		 Init();

	private:

		ULONG	  m_uRef;
		LPUNKNOWN m_pCategory;  // inner object supporting ISpecifyPropertyPageObjects
#ifdef MOZ_SMARTUPDATE
        CSmartUpdatePrefs *m_pSmartUpdatePrefs;
#endif /* MOZ_SMARTUPDATE */
};

/////////////////////////////////////////////////////////////////////////////
// CAdvancedPrefs
CAdvancedPrefs::CAdvancedPrefs()
{
	m_uRef = 0;
	m_pCategory = NULL;
#ifdef MOZ_SMARTUPDATE
    m_pSmartUpdatePrefs = new CSmartUpdatePrefs;
    m_pSmartUpdatePrefs->AddRef();
#endif /* MOZ_SMARTUPDATE */
 }

HRESULT
CAdvancedPrefs::Init()
{
   	// Create the object as part of an aggregate
  	return FEU_CoCreateInstance(CLSID_AdvancedPrefs, (LPUNKNOWN)this,
		CLSCTX_INPROC_SERVER, IID_IUnknown, (LPVOID *)&m_pCategory);
}

STDMETHODIMP
CAdvancedPrefs::QueryInterface(REFIID riid, LPVOID FAR* ppvObj)
{
	*ppvObj = NULL;

	if (riid == IID_IUnknown || riid == IID_IAdvancedPrefs) {
		*ppvObj = (LPVOID)this;
		AddRef();
		return NOERROR;
#ifdef MOZ_SMARTUPDATE
	} else if (riid == IID_ISmartUpdatePrefs) {
		assert(m_pSmartUpdatePrefs);
		return m_pSmartUpdatePrefs->QueryInterface(riid, ppvObj);
#endif /* MOZ_SMARTUPDATE */
	} else if (riid == IID_ISpecifyPropertyPageObjects) {
		assert(m_pCategory);
		return m_pCategory->QueryInterface(riid, ppvObj);
	}

	return ResultFromScode(E_NOINTERFACE);
}

STDMETHODIMP_(ULONG)
CAdvancedPrefs::AddRef()
{
	return ++m_uRef;
}

STDMETHODIMP_(ULONG)
CAdvancedPrefs::Release()
{
	if (--m_uRef == 0) {
		if (m_pCategory)
			m_pCategory->Release();
#ifdef MOZ_SMARTUPDATE
        if (m_pSmartUpdatePrefs)
			m_pSmartUpdatePrefs->Release();
#endif /* MOZ_SMARTUPDATE */
		delete this;
		return 0;
	}

	return m_uRef;
}

void wfe_ReloadAllWindows()
{
    g_bReloadChangeColor = FALSE;
    g_bReloadAllWindows = FALSE;
}

static BOOL
CreateAppearancesCategory(HWND pFrame, LPSPECIFYPROPERTYPAGEOBJECTS *pCategory)
{
    int                 nCharsetId = 0;
	CAppearancePrefs   *pAppearance = new CAppearancePrefs(nCharsetId);
	BOOL		   		bResult = FALSE;
	
	pAppearance->AddRef();

	// Initialize the appearances object. This allows it to load any objects that are
	// contained or aggregated
	if (SUCCEEDED(pAppearance->Init())) {
		// Get the interface pointer for ISpecifyPropertyPageObjects
		if (SUCCEEDED(pAppearance->QueryInterface(IID_ISpecifyPropertyPageObjects, (LPVOID *)pCategory)))
			bResult = TRUE;
	}
	else {
		// Try to register the dll and initialize again
#ifdef _WIN32
		bResult = RegisterCLSIDForDll("brpref32.dll");
#else
		bResult = RegisterCLSIDForDll("brpref16.dll");
#endif
		if (bResult) {
			if (SUCCEEDED(pAppearance->Init())) {
				// Get the interface pointer for ISpecifyPropertyPageObjects
				if (SUCCEEDED(pAppearance->QueryInterface(IID_ISpecifyPropertyPageObjects, (LPVOID *)pCategory)))
					bResult = TRUE;
			}
			else
				bResult = FALSE;
		}
	}

	// We're all done with the object
	pAppearance->Release();
	return bResult;
}

static BOOL
CreateBrowserCategory(LPSPECIFYPROPERTYPAGEOBJECTS *pCategory)
{
	CBrowserPrefs *pBrowser;
	BOOL		   bResult = FALSE;

	// If this is the browser then use the URL for the current page
	pBrowser = new CBrowserPrefs(NULL);
	pBrowser->AddRef();
	
	// Initialize the browser pref object. This allows it to load any objects that are
	// contained or aggregated
	if (SUCCEEDED(pBrowser->Init())) {
		// Get the interface pointer for ISpecifyPropertyPageObjects
		if (SUCCEEDED(pBrowser->QueryInterface(IID_ISpecifyPropertyPageObjects, (LPVOID *)pCategory)))
			bResult = TRUE;
	}
	else {
		// Try to register the dll and initialize again
#ifdef _WIN32
		bResult = RegisterCLSIDForDll("brpref32.dll");
#else
		bResult = RegisterCLSIDForDll("brpref16.dll");
#endif
		if (bResult) {
			if (SUCCEEDED(pBrowser->Init())) {
				// Get the interface pointer for ISpecifyPropertyPageObjects
				if (SUCCEEDED(pBrowser->QueryInterface(IID_ISpecifyPropertyPageObjects, (LPVOID *)pCategory)))
					bResult = TRUE;
			}
			else
				bResult = FALSE;
		}
	}

	// We're all done with the object
	pBrowser->Release();
	return bResult;
}

static BOOL
CreateAdvancedCategory(LPSPECIFYPROPERTYPAGEOBJECTS *pCategory)
{
	CAdvancedPrefs *pAdvanced;
	BOOL		   bResult = FALSE;

	
	pAdvanced = new CAdvancedPrefs();
	pAdvanced->AddRef();
	
	// Initialize the browser pref object. This allows it to load any objects that are
	// contained or aggregated
	if (SUCCEEDED(pAdvanced->Init())) {
		// Get the interface pointer for ISpecifyPropertyPageObjects
		if (SUCCEEDED(pAdvanced->QueryInterface(IID_ISpecifyPropertyPageObjects, (LPVOID *)pCategory)))
			bResult = TRUE;
	} 
	
	// We're all done with the object
	pAdvanced->Release();
	return bResult;
}

typedef HRESULT (STDAPICALLTYPE *PFNPREFS)(HWND, int, int, LPCSTR, ULONG, LPSPECIFYPROPERTYPAGEOBJECTS *, ULONG, NETHELPFUNC);

// Callback for prefs to display a NetHelp window
void CALLBACK
#ifndef _WIN32
__export
#endif
NetHelpWrapper(LPCSTR lpszHelpTopic)
{
}

extern "C" void
DisplayPreferences(HWND pFrame)
{
	HINSTANCE					 	hPrefDll;
	LPSPECIFYPROPERTYPAGEOBJECTS	categories[7];
	PFNPREFS					 	pfnCreatePropertyFrame;
	ULONG						 	nCategories = 0;
	ULONG						 	nInitialCategory;
    ULONG                           nDesktopCategory = -1;
	static HWND		                pCurrentFrame = NULL;

	// We only allow one pref UI window up at a time
	if (pCurrentFrame) {
		HWND	hwnd = pCurrentFrame;

#ifdef _WIN32
		::SetForegroundWindow(hwnd);
#else
		::BringWindowToTop(hwnd);
#endif
		return;
	}

	// Load the preferences UI DLL
#ifdef _WIN32
	hPrefDll = LoadLibrary("prefui32.dll");
	if (hPrefDll) {
#else
	hPrefDll = LoadLibrary("prefui16.dll");
	if ((UINT)hPrefDll > 32) {
#endif

		// Get the address of the routine to create the property frame
		pfnCreatePropertyFrame = (PFNPREFS)GetProcAddress(hPrefDll, "NS_CreatePropertyFrame");
		if (!pfnCreatePropertyFrame) {
#ifdef _DEBUG
            const char * const pText = "Unable to get proc address for NS_CreatePropertyFrame";
            ::MessageBox( pFrame, pText, NULL, MB_OK | MB_ICONEXCLAMATION);
#endif
			FreeLibrary(hPrefDll);
			return;
		}
		
		// Remember that this window is displaying the pref UI
		pCurrentFrame = pFrame;

		// Appearances category
		if (CreateAppearancesCategory(pFrame, &categories[nCategories]))
			nCategories++;

		// Browser category
		if (CreateBrowserCategory(&categories[nCategories]))
			nCategories++;

		// Advanced category
        if (CreateAdvancedCategory(&categories[nCategories])) {
			nCategories++;
		}
             
        // Make sure we have at least one category
		if (nCategories == 0) {
            ::MessageBox(pFrame, "Can't Load Prefs", NULL, MB_OK | MB_ICONEXCLAMATION);
			FreeLibrary(hPrefDll);
			return;
		}
		
        // Hack to indicate whether we need to reload the windows because something
		// like a color or a font changed
		//
		// We do have code that observes the XP prefs, but if more than one preference
		// changes we'll get several callbacks (we don't have a way to do batch changes)
		// and we'll reload the windows multiple times which is very sloppy...
		g_bReloadAllWindows = FALSE;
		g_bReloadChangeColor = FALSE;

		// Figure out what the initial category we display should be
        nInitialCategory = 1;
		
		// Display the preferences
		pfnCreatePropertyFrame(pFrame, -1, -1, "Preferences", nCategories, categories,
            nInitialCategory, NetHelpWrapper);
		pCurrentFrame = NULL;

		// Release the interface pointers for the preference categories
		for (ULONG i = 0; i < nCategories; i++)
			categories[i]->Release();

		// Unload the library
		FreeLibrary(hPrefDll);

		// See if we need to reload the windows
		if (g_bReloadAllWindows) {
			wfe_ReloadAllWindows();
			g_bReloadAllWindows = FALSE;
		}

        // Save out the preferences
		PREF_SavePrefFile();

	} else {
#ifdef _DEBUG
		MessageBox(NULL, "Unable to load NSPREFUI.DLL", "Navigator",
			MB_OK | MB_ICONEXCLAMATION);
#endif
	}
}


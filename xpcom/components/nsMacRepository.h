/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
#ifdef XP_MAC
#ifdef MOZ_NGLAYOUT

#error "nsMacRepository.h became obsolete when the shared lib conversion was completed."

// The Mac NGLayout is not based on shared libraries yet.
// All the DLLs are built as static libraries and we present them as
// shared libraries by redefining PR_LoadLibrary(), PR_UnloadLibrary()
// and PR_FindSymbol() below.
//
// If you add or remove shared libraries on other platforms, you must
//	- Add the library name to the defines below.
//	- Rename the "NSGetFactory" and "NSCanUnload" procs for the Mac:
//    just append the library name to the function name.
//	- Add the library and its procs to the static list below.


typedef struct MacLibrary
{
	char *			name;
	nsFactoryProc	factoryProc;
	nsCanUnloadProc	unloadProc;
} MacLibrary;

// library names
#define WIDGET_DLL		"WIDGET_DLL"
#define GFXWIN_DLL		"GFXWIN_DLL"
#define VIEW_DLL		"VIEW_DLL"
#define WEB_DLL			"WEB_DLL"
#define PLUGIN_DLL		"PLUGIN_DLL"
#define PREF_DLL		"PREF_DLL"
#define PARSER_DLL		"PARSER_DLL"
#define DOM_DLL    		"DOM_DLL"
#define LAYOUT_DLL		"LAYOUT_DLL"
#define NETLIB_DLL		"NETLIB_DLL"
#define EDITOR_DLL		"EDITOR_DLL"

#ifdef IMPL_MAC_REPOSITORY

extern "C" nsresult		NSGetFactory_WIDGET_DLL(nsISupports* serviceMgr,
                                           const nsCID &aClass,
                                           const char *aClassName,
                                           const char *aContractID,
                                           nsIFactory **aFactory);
extern "C" nsresult		NSGetFactory_GFXWIN_DLL(nsISupports* serviceMgr,
                                           const nsCID &aClass,
                                           const char *aClassName,
                                           const char *aContractID,
                                           nsIFactory **aFactory);
extern "C" nsresult		NSGetFactory_VIEW_DLL(nsISupports* serviceMgr,
                                           const nsCID &aClass,
                                           const char *aClassName,
                                           const char *aContractID,
                                           nsIFactory **aFactory);
extern "C" nsresult		NSGetFactory_WEB_DLL(nsISupports* serviceMgr,
                                           const nsCID &aClass,
                                           const char *aClassName,
                                           const char *aContractID,
                                           nsIFactory **aFactory);
#if 0
extern "C" nsresult		NSGetFactory_PLUGIN_DLL(nsISupports* serviceMgr,
                                           const nsCID &aClass,
                                           const char *aClassName,
                                           const char *aContractID,
                                           nsIFactory **aFactory);
#endif
extern "C" nsresult		NSGetFactory_PREF_DLL(nsISupports* serviceMgr,
                                           const nsCID &aClass,
                                           const char *aClassName,
                                           const char *aContractID,
                                           nsIFactory **aFactory);
extern "C" nsresult		NSGetFactory_PARSER_DLL(nsISupports* serviceMgr,
                                           const nsCID &aClass,
                                           const char *aClassName,
                                           const char *aContractID,
                                           nsIFactory **aFactory);
extern "C" nsresult		NSGetFactory_DOM_DLL(nsISupports* serviceMgr,
                                           const nsCID &aClass,
                                           const char *aClassName,
                                           const char *aContractID,
                                           nsIFactory **aFactory);
extern "C" nsresult		NSGetFactory_LAYOUT_DLL(nsISupports* serviceMgr,
                                           const nsCID &aClass,
                                           const char *aClassName,
                                           const char *aContractID,
                                           nsIFactory **aFactory);
extern "C" nsresult		NSGetFactory_NETLIB_DLL(nsISupports* serviceMgr,
                                           const nsCID &aClass,
                                           const char *aClassName,
                                           const char *aContractID,
                                           nsIFactory **aFactory);
extern "C" nsresult		NSGetFactory_EDITOR_DLL(nsISupports* serviceMgr,
                                           const nsCID &aClass,
                                           const char *aClassName,
                                           const char *aContractID,
                                           nsIFactory **aFactory);

extern "C" PRBool		NSCanUnload_PREF_DLL(void);

// library list
static MacLibrary	libraries[] = {
#if 0
	WIDGET_DLL,		NSGetFactory_WIDGET_DLL,	NULL,
	GFXWIN_DLL,		NSGetFactory_GFXWIN_DLL,	NULL,
	VIEW_DLL,		NSGetFactory_VIEW_DLL,		NULL,
	WEB_DLL,		NSGetFactory_WEB_DLL,		NULL,
	//PLUGIN_DLL,		NSGetFactory_PLUGIN_DLL,	NULL,
	PREF_DLL,		NSGetFactory_PREF_DLL,		NSCanUnload_PREF_DLL,
	PARSER_DLL,		NSGetFactory_PARSER_DLL,	NULL,
	DOM_DLL,		NSGetFactory_DOM_DLL,		NULL,
	LAYOUT_DLL,		NSGetFactory_LAYOUT_DLL,	NULL,
	NETLIB_DLL,		NSGetFactory_NETLIB_DLL,	NULL,
	//EDITOR_DLL,		NSGetFactory_EDITOR_DLL,	NULL,		// FIX ME
#endif
	NULL
};

static void* FindMacSymbol(char* libName, const char *symbolName)
{
	MacLibrary * macLib;

	for (macLib = libraries; ; macLib ++)
	{
		if (macLib->name == NULL)
			return NULL;

		if (PL_strcmp(macLib->name, libName) == 0)
			break;
	}

	if (PL_strcmp(symbolName, "NSGetFactory") == 0) {
		return macLib->factoryProc;
	}
	else if (PL_strcmp(symbolName, "NSCanUnload") == 0) {
		return macLib->unloadProc;
	}
	return NULL;
}

#define PR_LoadLibrary(libName)			(PRLibrary *)libName
#define PR_UnloadLibrary(lib)			lib = NULL
#define PR_FindSymbol(lib, symbolName)	FindMacSymbol((char*)lib, symbolName)

#endif // IMPL_MAC_REPOSITORY

#endif // MOZ_NGLAYOUT
#endif // XP_MAC

/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

/* Dll
 *
 * Programmatic representation of a dll. Stores modifiedTime and size for
 * easy detection of change in dll.
 *
 * dp Suresh <dp@netscape.com>
 */

#include "prio.h"
#include "prlink.h"
#include "nsISupports.h"

#ifndef xcDll_h__
#define xcDll_h__

class nsIFileSpec;
class nsIModule;
class nsIServiceManager;

typedef enum nsDllStatus
{
	DLL_OK = 0,
	DLL_NO_MEM = 1,
	DLL_STAT_ERROR = 2,
	DLL_NOT_FILE = 3,
	DLL_INVALID_PARAM = 4
} nsDllStatus;

class nsDll
{
private:
    char *m_dllName;			// Stores the dllName to load.

    nsIFileSpec *m_dllSpec;	// Filespec representing the component
	PRUint32 m_modDate;		// last modified time at creation of this object
	PRUint32 m_size;		// size of the dynamic library at creation of this object

	PRLibrary *m_instance;	// Load instance
	nsDllStatus m_status;	// holds current status
    nsIModule *m_moduleObject;

    // Cache frequent queries
    char *m_persistentDescriptor;
    char *m_nativePath;

    PRBool m_markForUnload;
    char *m_registryLocation;

    void Init(nsIFileSpec *dllSpec);
    void Init(const char *persistentDescriptor);

public:
 
	nsDll(nsIFileSpec *dllSpec, const char *registryLocation);
	nsDll(nsIFileSpec *dllSpec, const char *registryLocation, PRUint32 modDate, PRUint32 fileSize);
	nsDll(const char *persistentDescriptor);
	nsDll(const char *persistentDescriptor, PRUint32 modDate, PRUint32 fileSize);
    nsDll(const char *dll, int type /* dummy */);

	~nsDll(void);

    // Sync : Causes update of file information
    nsresult Sync(void);

	// Status checking on operations completed
	nsDllStatus GetStatus(void) { return (m_status); }

	// Dll Loading
	PRBool Load(void);
	PRBool Unload(void);
	PRBool IsLoaded(void)
	{
		return ((m_instance != 0) ? PR_TRUE : PR_FALSE);
	}
    void MarkForUnload(PRBool mark) { m_markForUnload = mark; }
    PRBool IsMarkedForUnload(void) { return m_markForUnload; }

    // Shutdown the dll. This will call any on unload hook for the dll.
    // This wont unload the dll. Unload() implicitly calls Shutdown().
    nsresult Shutdown(void);

	void *FindSymbol(const char *symbol);
	
    PRBool HasChanged(void);

    // WARNING: DONT FREE string returned.
	const char *GetNativePath(void);
    // WARNING: DONT FREE string returned.
    const char *GetPersistentDescriptorString(void);
    // WARNING: DONT FREE string returned.
    const char *GetRegistryLocation(void) { return m_registryLocation; }
	PRUint32 GetLastModifiedTime(void) { return(m_modDate); }
	PRUint32 GetSize(void) { return(m_size); }
	PRLibrary *GetInstance(void) { return (m_instance); }

    // NS_RELEASE() is required to be done on objects returned
    nsresult GetDllSpec(nsIFileSpec **dllSpec);
    nsresult GetModule(nsISupports *servMgr, nsIModule **mobj);
};

#endif /* xcDll_h__ */

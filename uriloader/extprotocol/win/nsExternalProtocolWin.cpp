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
 * 
 * Contributor(s): 
 *   
 */


#include "nscore.h"
#include "nsCRT.h"
#include "nsString.h"
#include "nsIURI.h"
#include "nsExternalProtocol.h"
#include "nsXPIDLString.h"

#include <windows.h>

#ifdef NS_DEBUG
#define DEBUG_LOG0( x) printf( x)
#define DEBUG_LOG1( x, y) printf( x, y)
#else
#define DEBUG_LOG0( x)
#define DEBUG_LOG1( x, y)
#endif


////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////
// ::RegOpenKeyEx( HKEY_CURRENT_USER, "Software\\Accounts", 0, KEY_QUERY_VALUE | KEY_ENUMERATE_SUB_KEYS, &sKey) == ERROR_SUCCESS)

BYTE * GetValueBytes( HKEY hKey, const char *pValueName)
{
	LONG	err;
	DWORD	bufSz;
	LPBYTE	pBytes = NULL;

	err = ::RegQueryValueEx( hKey, pValueName, NULL, NULL, NULL, &bufSz); 
	if (err == ERROR_SUCCESS) {
		pBytes = new BYTE[bufSz];
		err = ::RegQueryValueEx( hKey, pValueName, NULL, NULL, pBytes, &bufSz);
		if (err != ERROR_SUCCESS) {
			delete [] pBytes;
			pBytes = NULL;
		}
	}

	return( pBytes);
}


PRBool GetCommandString( const char *pProtocol, nsCString& command)
{
	// HKEY_CLASSES_ROOT
	//	<protocol>/shell/open/command
	
	PRBool	result = PR_FALSE;
	nsCString	reg(pProtocol);
	reg.Append( "\\shell\\open\\command");
	HKEY	hKey;
	LONG err = ::RegOpenKeyEx( HKEY_CLASSES_ROOT, reg, 0, KEY_QUERY_VALUE, &hKey);
	if (err == ERROR_SUCCESS) {
		LPBYTE pBytes = GetValueBytes( hKey, NULL);
		if (pBytes) {
			command = (char *)pBytes;
			delete [] pBytes;
			result = PR_TRUE;
		}
	}

	return( result);
}

////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////

nsresult nsExternalProtocol::DefaultLaunch( nsIURI *pUri)
{
	nsresult rv = NS_OK;

	// 1. Find the default app for this protocol
	// 2. Set up the command line
	// 3. Launch the app.

	// For now, we'll just cheat essentially, check for the command line
	// then just call ShellExecute()!
	nsCString	cmd;
	if (GetCommandString( m_schemeName, cmd)) {
		nsXPIDLCString	uriStr;
		if (pUri) {
			pUri->GetSpec( getter_Copies( uriStr));
		}
		else {
			uriStr = m_schemeName;
		}
		LONG r = (LONG) ::ShellExecute( NULL, "open", uriStr, NULL, NULL, SW_SHOWNORMAL);
		if (r < 32) {
			DEBUG_LOG1( "ShellExecute failed: %d\n", (int)r);
			rv = NS_ERROR_FAILURE;
		}
		else
			rv = NS_ERROR_EXTERNAL_LAUNCH;
		
	}
	else
		rv = NS_ERROR_FAILURE;

    return( rv);
}


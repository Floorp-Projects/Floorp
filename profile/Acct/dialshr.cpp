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
//#define VC_EXTRALEAN		// Exclude rarely-used stuff from Windows headers

//#include <afxwin.h>         // MFC core and standard components
//#include <afxext.h>         // MFC extensions
// Adding WINVER = 0x401 for compiling purposes
#ifdef WINVER
#undef WINVER
#endif
#define WINVER 0x0401

#include "dialshr.h"
//include <windows.h>
#include <string.h>
#include <stdio.h>
#include <time.h>   
#include <dos.h>
#include <winbase.h>
#include <raserror.h>
//#include <shlobj.h>
#include <regstr.h>
#include <tapi.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <winsock.h>


#include "resource.h"
//nclude "xp_mem.h"
//nclude "prefapi.h"
//#include "prmem.h"
//#include "nsString.h"   
 
#define trace

// Increased from 20 to 2000 since usage is wierd in createDialerShortcut SWE 2/1/99
#define MAX_ENTRIES     2000

// Navigator quit message (see QuitNavigator)
#define ID_APP_SUPER_EXIT 34593

// The number of times we try to dial
#define NUM_ATTEMPTS    3
#define IDDISCONNECTED  31

//THe array for storing data for account setup
#define _MAX_LENGTH			256
#define _MAX_NUM_ACCOUNTS	50

static char gNewAccountData[_MAX_NUM_ACCOUNTS][_MAX_LENGTH] = {{'\0'}};
static int	g_Count = 0;

enum CallState
{
	StateIdle,
	StateConnecting,
	StateConnected,
	StateDisconnecting
};


CallState					gCallState = StateIdle;

UINT						gDialAttempts = 0;		// keeps the total number of dialing count
HRASCONN					gRasConn;				// handle to the current ras connection
RASCONNSTATE				gRASstate;				// current connection's ras state
RASDIALPARAMS				gDialParams;				// keeps the current connection info
HWND						gHwndStatus = NULL;		// handle to our connection status window
BOOL						gCancelled = FALSE;		// assume connection will be there unless user cancels
BOOL						gLineDrop = FALSE;
BOOL						gDeviceErr = FALSE; 		// assume no hardware err
HWND						gHwndParent = NULL;
HANDLE						gRasMon = NULL;		// process handle to RasMon on WinNT

HINSTANCE					m_hRasInst = NULL;

// NT/95 entrypoints
RASDIAL						m_lpfnRasDial;
RASDELETEENTRY				m_lpfnRasDeleteEntry;
RASENUMCONNECTIONS			m_lpfnRasEnumConnections;
RASENUMDEVICES				m_lpfnRasEnumDevices;
RASENUMENTRIES				m_lpfnRasEnumEntries;
RASGETCONNECTSTATUS			m_lpfnRasGetConnectStatus;
RASGETCOUNTRYINFO			m_lpfnRasGetCountryInfo;
RASGETENTRYPROPERTIES		m_lpfnRasGetEntryProperties;
RASGETERRORSTRING			m_lpfnRasGetErrorString;
RASHANGUP					m_lpfnRasHangUp;
RASSETENTRYDIALPARAMS		m_lpfnRasSetEntryDialParams;
RASSETENTRYPROPERTIES		m_lpfnRasSetEntryProperties;
RASVALIDATEENTRYNAME		m_lpfnRasValidateEntryName;

// NT entrypoints
RASSETAUTODIALENABLE		m_lpfnRasSetAutodialEnable;
RASSETAUTODIALADDRESS		m_lpfnRasSetAutodialAddress;
RASGETAUTODIALADDRESS		m_lpfnRasGetAutodialAddress;
RASSETAUTODIALPARAM			m_lpfnRasSetAutodialParam;
RASENUMAUTODIALADDRESSES	m_lpfnRasEnumAutodialAddresses;
RASSETCREDENTIALS			m_lpfnRasSetCredentials;

// Keep track of whether the functions are loaded
BOOL						g_lpfn_loaded = FALSE;

size_t						stRASENTRY ;
size_t						stRASCONN;
size_t						stRASCTRYINFO;
size_t						stRASDIALPARAMS;
size_t						stRASDEVINFO;
size_t						stRASENTRYNAME;

//********************************************************************************
// LoadRasFunctions()
//********************************************************************************
BOOL LoadRasFunctions( LPCSTR lpszLibrary )
{
	//    ASSERT(!m_hRasInst);
	m_hRasInst = ::LoadLibrary( lpszLibrary );
	if ( (UINT)m_hRasInst > 32 ) 
	{
		m_lpfnRasDial = (RASDIAL)::GetProcAddress( m_hRasInst, "RasDialA" );
		m_lpfnRasDeleteEntry = (RASDELETEENTRY)::GetProcAddress( m_hRasInst, "RasDeleteEntry" );
		m_lpfnRasEnumConnections = (RASENUMCONNECTIONS)::GetProcAddress( m_hRasInst, "RasEnumConnectionsA" );
		m_lpfnRasEnumEntries = (RASENUMENTRIES)::GetProcAddress( m_hRasInst, "RasEnumEntriesA" );
		m_lpfnRasEnumDevices = (RASENUMDEVICES)::GetProcAddress( m_hRasInst, "RasEnumDevices" );
		m_lpfnRasGetConnectStatus = (RASGETCONNECTSTATUS)::GetProcAddress( m_hRasInst, "RasGetConnectStatus" );
		m_lpfnRasGetCountryInfo = (RASGETCOUNTRYINFO)::GetProcAddress( m_hRasInst, "RasGetCountryInfo" );
		m_lpfnRasGetEntryProperties = (RASGETENTRYPROPERTIES)::GetProcAddress( m_hRasInst, "RasGetEntryProperties");
		m_lpfnRasHangUp = (RASHANGUP)::GetProcAddress( m_hRasInst, "RasHangUpA" );
		m_lpfnRasSetEntryDialParams = (RASSETENTRYDIALPARAMS)::GetProcAddress( m_hRasInst, "RasSetEntryDialParamsA" );
		m_lpfnRasSetEntryProperties = (RASSETENTRYPROPERTIES)::GetProcAddress( m_hRasInst, "RasSetEntryProperties" );
		m_lpfnRasValidateEntryName = (RASVALIDATEENTRYNAME)::GetProcAddress( m_hRasInst,"RasValidateEntryName" );
		
		SizeofRAS95();		
		return TRUE;
	} 
	else 
	{
		MessageBox( NULL, "Please install Dial-up Networking", "Netscape", MB_ICONSTOP );     
	
		::FreeLibrary( m_hRasInst );
		m_hRasInst = NULL;
		return FALSE; 
	}
}

//********************************************************************************
// LoadRasFunctionsNT()
//********************************************************************************
BOOL LoadRasFunctionsNT( LPCSTR lpszLibrary )
{
	m_hRasInst = ::LoadLibrary( lpszLibrary );
	
    if ( (UINT)m_hRasInst > 32 ) 
	{
		m_lpfnRasDial = (RASDIAL)::GetProcAddress( m_hRasInst, "RasDialA" );
		m_lpfnRasDeleteEntry = (RASDELETEENTRY)::GetProcAddress( m_hRasInst, "RasDeleteEntryA" );
		m_lpfnRasEnumConnections = (RASENUMCONNECTIONS)::GetProcAddress( m_hRasInst, "RasEnumConnectionsA" );
		m_lpfnRasEnumDevices = (RASENUMDEVICES)::GetProcAddress( m_hRasInst, "RasEnumDevicesA" );
		m_lpfnRasEnumEntries = (RASENUMENTRIES)::GetProcAddress( m_hRasInst, "RasEnumEntriesA" );
		m_lpfnRasGetConnectStatus = (RASGETCONNECTSTATUS)::GetProcAddress( m_hRasInst, "RasGetConnectStatus" );
		m_lpfnRasGetCountryInfo = (RASGETCOUNTRYINFO)::GetProcAddress( m_hRasInst, "RasGetCountryInfoA" );
		m_lpfnRasGetEntryProperties = (RASGETENTRYPROPERTIES)::GetProcAddress( m_hRasInst, "RasGetEntryPropertiesA" );
		m_lpfnRasHangUp = (RASHANGUP)::GetProcAddress( m_hRasInst, "RasHangUpA" );
		m_lpfnRasSetEntryDialParams = (RASSETENTRYDIALPARAMS)::GetProcAddress(m_hRasInst, "RasSetEntryDialParamsA" );
		m_lpfnRasSetEntryProperties = (RASSETENTRYPROPERTIES)::GetProcAddress( m_hRasInst, "RasSetEntryPropertiesA" );		
		m_lpfnRasValidateEntryName = (RASVALIDATEENTRYNAME)::GetProcAddress( m_hRasInst, "RasValidateEntryNameA" );
		
		// AUTODIAL
		m_lpfnRasEnumAutodialAddresses = (RASENUMAUTODIALADDRESSES)::GetProcAddress( m_hRasInst, "RasEnumAutodialAddressesA" ); 
		m_lpfnRasGetAutodialAddress = (RASGETAUTODIALADDRESS)::GetProcAddress( m_hRasInst, "RasGetAutodialAddressA" );
		m_lpfnRasSetAutodialAddress = (RASSETAUTODIALADDRESS)::GetProcAddress( m_hRasInst, "RasSetAutodialAddressA" );
		m_lpfnRasSetAutodialEnable = (RASSETAUTODIALENABLE)::GetProcAddress( m_hRasInst, "RasSetAutodialEnableA" );
		m_lpfnRasSetAutodialParam = (RASSETAUTODIALPARAM)::GetProcAddress( m_hRasInst, "RasSetAutodialParamA" );
		m_lpfnRasSetCredentials = (RASSETCREDENTIALS)::GetProcAddress( m_hRasInst, "RasSetCredentialsA" );
		
		SizeofRASNT40();
		return TRUE;       
	}                
   	else 
	{
		MessageBox( NULL, "Please install Dial-up Networking", "Netscape", MB_ICONSTOP );
		::FreeLibrary( m_hRasInst );
		m_hRasInst = NULL;
	    return FALSE;       
	}
}
    
//********************************************************************************
// FreeRasFunctions()
//********************************************************************************
void FreeRasFunctions()
{
	if ( (UINT)m_hRasInst > 32 )
		FreeLibrary( m_hRasInst );
}



//********************************************************************************
// GetModemList (s/b GetModems)
// Returns list of modems available for use ('installed' by the user). For Win95
// this list come from the OS, and each entry contains 2 strings - the first is
// the Modem Name, and the second is the device type (both are needed to select
// the device to use to make a Dial-up connection).
//********************************************************************************
BOOL GetModemList( char*** resultModemList, int* numDevices )
{
	DWORD           dwBytes = 0, dwDevices;
    LPRASDEVINFO    lpRnaDevInfo;
	unsigned short	i;
    
    // First find out how much memory to allocate
    (*m_lpfnRasEnumDevices)( NULL, &dwBytes, &dwDevices );
    lpRnaDevInfo = (LPRASDEVINFO)malloc( dwBytes );
    
	if ( !lpRnaDevInfo )
		return (FALSE);
    
    lpRnaDevInfo->dwSize = stRASDEVINFO;
	(*m_lpfnRasEnumDevices)( lpRnaDevInfo, &dwBytes, &dwDevices );
    
    // copy all entries to the char array
    *resultModemList = new char* [dwDevices+1];
    if ( *resultModemList == NULL )
	{
		free(lpRnaDevInfo);
		return FALSE;
	}
    
    *numDevices = dwDevices;
    for ( i=0; i < dwDevices; i++ ) 
    {
		(*resultModemList)[i] = new char[ strlen( lpRnaDevInfo[ i ].szDeviceName) + 1 ];
		if ( ( *resultModemList)[ i ] == NULL )
			goto fail;
		strcpy( ( *resultModemList)[ i ], lpRnaDevInfo[ i ].szDeviceName );
    }
	free(lpRnaDevInfo);
    return TRUE;

fail:
	for (i--; i>= 0; i--)
		delete [](*resultModemList)[i];
	delete [](*resultModemList);
	*resultModemList = NULL;
	free(lpRnaDevInfo);
	return FALSE;
}
    
    
//********************************************************************************
// GetModemType
// Returns the type for the selected modem.
//********************************************************************************
BOOL GetModemType( char *strModemName, char *strModemType)
{
    DWORD           dwBytes = 0, dwDevices;
    LPRASDEVINFO    lpRnaDevInfo;
	BOOL			found = FALSE;
      
	// First get Modem (okay - Device) info from Win95
	// find out how much memory to allocate
	(*m_lpfnRasEnumDevices)( NULL, &dwBytes, &dwDevices );
	lpRnaDevInfo = (LPRASDEVINFO)malloc( dwBytes );
	
	if ( !lpRnaDevInfo )
		return NULL;
	
	lpRnaDevInfo->dwSize = stRASDEVINFO;
	(*m_lpfnRasEnumDevices)( lpRnaDevInfo, &dwBytes, &dwDevices );
	
	// If match the modem given from JS then return the associated Type
	for ( unsigned short i = 0; !found && i < dwDevices; i++ ) 
	{
		if ( 0 == strcmp(strModemName, lpRnaDevInfo[i].szDeviceName ) ) 
		{
			strModemType = new char[ strlen( lpRnaDevInfo[ i ].szDeviceType ) + 1 ];
			strcpy( strModemType, lpRnaDevInfo[ i ].szDeviceType );
			found = TRUE;
		}                
	}
	free(lpRnaDevInfo);
	return found;
}

BOOL SetDisconnectRegistry(HKEY topkey, char *keyname, DWORD disconnectTime)
{
	HKEY    hKey;
	DWORD   dwDisposition;
	long	result;
	DWORD	dwValue;
	BOOL	rtn = TRUE;

	if (RegCreateKeyEx(topkey, keyname, NULL, NULL, NULL, 
						KEY_ALL_ACCESS, NULL, &hKey, &dwDisposition) != ERROR_SUCCESS)
	   return FALSE;

	// Use REG_DWORD instead of REG_BINARY to avoid byte order problems

	// try setting autodisconnect here
	dwValue = 1;
	result = RegSetValueEx(hKey, "EnableAutoDisconnect", NULL, REG_DWORD, (LPBYTE)&dwValue, sizeof(DWORD));
	if (result != ERROR_SUCCESS)
		rtn = FALSE;

	// default auto-disconnect after 5 minutes
	dwValue = (disconnectTime < 5) ? 5 : disconnectTime;
	result = RegSetValueEx(hKey, "DisconnectIdleTime", NULL, REG_DWORD, (LPBYTE)&dwValue, sizeof(DWORD));
	if (result != ERROR_SUCCESS)
		rtn = FALSE;

	RegCloseKey(hKey);

	return rtn;
}

//********************************************************************************
// SetAutoDisconnect
// Sets the autodisconnect time if idle
// the parameter "disconnectTime" is specified as MINUTES, convert it to SECONDS
// as necessary
//********************************************************************************
void SetAutoDisconnect(DWORD disconnectTime)
{
	HKEY    hKey;
//	DWORD   dwDisposition;
	long    result;
	DWORD	dwValue;

	// if it's win95
	if ( gPlatformOS == VER_PLATFORM_WIN32_WINDOWS )
	{

		SetDisconnectRegistry(HKEY_CURRENT_USER,
			"Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings", disconnectTime);
		SetDisconnectRegistry(HKEY_CURRENT_USER,
			"Software\\Microsoft\\Windows\\Internet Settings", disconnectTime);
		SetDisconnectRegistry(HKEY_USERS,
			".Default\\Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings", disconnectTime);
		SetDisconnectRegistry(HKEY_USERS,
			".Default\\Software\\Microsoft\\Windows\\Internet Settings", disconnectTime);

	} else { // NT40

		// we need to convert disconnectTime to # of seconds for NT40
		dwValue = (disconnectTime * 60);

		result = RegOpenKeyEx(HKEY_USERS, ".DEFAULT\\Software\\Microsoft\\RAS Phonebook", NULL, KEY_ALL_ACCESS, &hKey);

		if (result != ERROR_SUCCESS)
			return;

		// now set the auto disconnect seconds
		result = RegSetValueEx(hKey, "IdleHangupSeconds", NULL, REG_DWORD, (LPBYTE)&dwValue, sizeof(DWORD));

		RegCloseKey(hKey);


	} 

	return;
}

//********************************************************************************
// EnableDialOnDemand  (win95)
// Set the magic keys in the registry to enable dial on demand
//********************************************************************************
void EnableDialOnDemand95( LPSTR lpProfileName, BOOL flag )
{
	HKEY    hKey;
	DWORD   dwDisposition;
	long    result;
	char*	szData;
	
	// Do this first to ensure it happens.  Using spruced up function since it sets more keys.  SWE 1/5/99
	SetAutoDisconnect(5);

	//--------------------------------------------------------------------------------------------------------------
	// We need to tell windows about dialing on demand
	result = RegCreateKeyEx( HKEY_LOCAL_MACHINE, "System\\CurrentControlSet\\Services\\Winsock\\Autodial",
							NULL, NULL, NULL, KEY_ALL_ACCESS, NULL, &hKey, &dwDisposition );
	
	// err, oops
	if ( result != ERROR_SUCCESS )
		return;
	
	szData = "url.dll";
	result = RegSetValueEx( hKey, "AutodialDllName32", NULL, REG_SZ, (LPBYTE)szData, strlen( szData ) );
	
	szData = "AutodialHookCallback";
	result = RegSetValueEx( hKey, "AutodialFcnName32", NULL, REG_SZ, (LPBYTE)szData, strlen( szData ) );
	
	RegCloseKey( hKey );
	
	//--------------------------------------------------------------------------------------------------------------
	// set the autodial flag first
	result = RegCreateKeyEx( HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings",
							NULL, NULL, NULL, KEY_ALL_ACCESS, NULL, &hKey, &dwDisposition );
	
	// err, oops
	if ( result != ERROR_SUCCESS )
		return;
	
	// set the autodial and idle-time disconnect
	DWORD		dwValue = flag;
	result = RegSetValueEx( hKey, "EnableAutodial", NULL, REG_DWORD, (LPBYTE)&dwValue, sizeof( DWORD ) );
	
#if 0
// Removed in favor of SetAutoDisconnect()
	// try setting autodisconnect here
	dwValue = 1;
	result = RegSetValueEx( hKey, "EnableAutoDisconnect", NULL, REG_DWORD, (LPBYTE)&dwValue, sizeof( DWORD ) );
	
	// default auto-disconnect after 5 minutes
	dwValue = 5;
	result = RegSetValueEx( hKey, "DisconnectIdleTime", NULL, REG_DWORD, (LPBYTE)&dwValue, sizeof( DWORD ) );
#endif
	
	RegCloseKey( hKey );
	
#if 0
// Removed in favor of SetAutoDisconnect()
	// set the autodisconnect flags here too
	result = RegCreateKeyEx( HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\Internet Settings",
						NULL, NULL, NULL, KEY_ALL_ACCESS, NULL, &hKey, &dwDisposition );
	
	// err, oops
	if ( result != ERROR_SUCCESS )
		return;
	
	dwValue = 1;
	result = RegSetValueEx( hKey, "EnableAutoDisconnect", NULL, REG_DWORD, (LPBYTE)&dwValue, sizeof( DWORD ) );
	
	// default auto-disconnect after 5 minutes
	dwValue = 5;
	result = RegSetValueEx( hKey, "DisconnectIdleTime", NULL, REG_DWORD, (LPBYTE)&dwValue, sizeof( DWORD ) );
	
	RegCloseKey( hKey );
#endif
	
	//--------------------------------------------------------------------------------------------------------------
	// OK, let's tell it which profile to autodial
	result = RegCreateKeyEx( HKEY_CURRENT_USER, "RemoteAccess", NULL, NULL, NULL,
						KEY_ALL_ACCESS, NULL, &hKey, &dwDisposition );
	
	// err, oops
	if ( result != ERROR_SUCCESS )
		return;
	
	if ( flag )	// enable dod
	{
		result = RegSetValueEx( hKey, "InternetProfile", NULL, REG_SZ, (LPBYTE)lpProfileName, strlen( lpProfileName ) );
		result = RegSetValueEx( hKey, "Default", NULL, REG_SZ, (LPBYTE)lpProfileName, strlen( lpProfileName ) );
	}
	else			// disable dod
	{
		result = RegSetValueEx( hKey, "InternetProfile", NULL, REG_SZ, NULL, strlen( lpProfileName ) );
		result = RegSetValueEx( hKey, "Default", NULL, REG_SZ, NULL, strlen( lpProfileName ) );
	}
	RegCloseKey( hKey );
}

    
//********************************************************************************
//
// lineCallbackFuncNT  (winNT40)
//
// Sets the RAS structure for Dial on Demand, NT40 doesn't use registry like win95
//********************************************************************************
void FAR PASCAL
lineCallbackFuncNT(DWORD /* hDevice */, DWORD /* dwMsg */, DWORD /* dwCallbackInstance */,
	DWORD /* dwParam1 */, DWORD /* dwParam2 */, DWORD /* dwParam3 */)
{
	// Cathleen believes this is empty because it doesn't _have_ to do any work...
}
    
void EnableDialOnDemandNT(LPSTR lpProfileName, BOOL flag)
{
	RASAUTODIALENTRY        rasAutodialEntry;
	DWORD                   dwBytes = 0;
	DWORD                   dwNumDevs;
	HLINEAPP                lineApp;
	DWORD                   dwApiVersion;
	LINEINITIALIZEEXPARAMS  lineInitExParams;
	LINETRANSLATECAPS       lineTranslateCaps;
	int                     rtn;

	// This wasn't being done previously - SWE 1/5/99
	SetAutoDisconnect(5);
	
	// Initialize TAPI. We need to do this in order to get the dialable
	// number and to bring up the location dialog
	
	dwApiVersion = 0x00020000;
	lineInitExParams.dwOptions = LINEINITIALIZEEXOPTION_USEEVENT;
	lineInitExParams.dwTotalSize = sizeof( LINEINITIALIZEEXPARAMS );
	lineInitExParams.dwNeededSize = sizeof (LINEINITIALIZEEXPARAMS );
	
	rtn = lineInitializeEx( &lineApp, gDLL, lineCallbackFuncNT, 
		     NULL, &dwNumDevs, &dwApiVersion, &lineInitExParams );

	if ( rtn == 0)
	{
		lineTranslateCaps.dwTotalSize = sizeof( LINETRANSLATECAPS );
		lineTranslateCaps.dwNeededSize = sizeof( LINETRANSLATECAPS );
		rtn = lineGetTranslateCaps( lineApp, dwApiVersion, &lineTranslateCaps );
	}               
	
	rasAutodialEntry.dwFlags = 0;
	rasAutodialEntry.dwDialingLocation = lineTranslateCaps.dwCurrentLocationID;
	strcpy( rasAutodialEntry.szEntry, lpProfileName ); 
	rasAutodialEntry.dwSize = sizeof( RASAUTODIALENTRY );
	
	// set auto dial params
	int     val = flag;
	rtn = ( *m_lpfnRasSetAutodialParam )( RASADP_DisableConnectionQuery, &val, sizeof( int ) );

	if ( rtn == ERROR_INVALID_PARAMETER )
	{
		trace( "dialer.cpp : EnableDialOnDemandNT - Invalid Parameter. Can't set Autodial Parameters. (r)" );
		return;
	}	 
	else if ( rtn == ERROR_INVALID_SIZE )
	{
		trace( "dialer.cpp : EnableDialOnDemandNT - Invalid size. Can't set Autodial Parameters. (r)" );
		return;
	}	 
	else if ( rtn )
	{
		trace( "dialer.cpp : EnableDialOnDemandNT - Can't set Autodial Parameters. Error %d. (r)", rtn );
		return;
	}	 

	
	if ( flag )        // set dod entry if the flag is enabled
	{
		rtn = ( *m_lpfnRasSetAutodialAddress )( "www.netscape.com", 0, &rasAutodialEntry, sizeof(RASAUTODIALENTRY), 1);

		if ( rtn == ERROR_INVALID_PARAMETER )
		{
			trace( "dialer.cpp : EnableDialOnDemandNT - Invalid Parameter. Can't set Autodial Address. (r)" );
			return;
		}
		else if ( rtn == ERROR_INVALID_SIZE )
		{
			trace( "dialer.cpp : EnableDialOnDemandNT - Invalid size. Can't set Autodial Address. (r)" );
			return;
		}
		else if ( rtn )
		{
			trace( "dialer.cpp : EnableDialOnDemandNT - Can't set Autodial Address. Error %d. (r)", rtn );
			return;
		}
	}	 

	rtn = ( *m_lpfnRasSetAutodialEnable )( rasAutodialEntry.dwDialingLocation, flag );

	if ( rtn )
	{
		trace( "dialer.cpp : EnableDialOnDemandNT - Can't set Autodial Enable. Error %d. (r)", rtn );
		return;
	}	 

	return;
}


//********************************************************************************
// utility function
// sameStrings
// checks for string equality between a STRRET and a LPCTSTR
//********************************************************************************
static BOOL sameStrings( LPITEMIDLIST pidl, STRRET& lpStr1, LPCTSTR lpStr2 )
{
	char    buf[ MAX_PATH ];
	char*	mystr;                   // ??? what is the purpose of this variable's usage ???
	int		cch; 
    
	switch ( lpStr1.uType )
	{
		case STRRET_WSTR:
			cch = WideCharToMultiByte( CP_ACP, WC_SEPCHARS | WC_COMPOSITECHECK,
					lpStr1.pOleStr, -1, buf, MAX_PATH, NULL, NULL );
    
			cch = GetLastError();
			return strcmp( lpStr2, buf ) == 0;
    
		case STRRET_OFFSET:
			mystr = ((char*)pidl) + lpStr1.uOffset;
			return strcmp( lpStr2, ((char*)pidl) + lpStr1.uOffset ) == 0;
    
		case STRRET_CSTR:
			mystr = lpStr1.cStr;
			return strcmp( lpStr2, lpStr1.cStr ) == 0;
	}
    
	return FALSE;
}
    
//********************************************************************************
// utility function
// procStrings
//********************************************************************************
static BOOL procStrings( LPITEMIDLIST pidl, STRRET& lpStr1, char* lpStr2 )
{
	char*		mystr;                   // ??? what is the purpose of this variable's usage ???
	int			cch; 
    
	switch ( lpStr1.uType )
	{
		case STRRET_WSTR:
			cch = WideCharToMultiByte( CP_ACP, WC_SEPCHARS | WC_COMPOSITECHECK,
					lpStr1.pOleStr, -1, lpStr2, MAX_PATH, NULL, NULL );
			return TRUE;
						
		case STRRET_OFFSET:
			mystr = ((char*)pidl) + lpStr1.uOffset;
			strcpy( lpStr2, ((char*)pidl) + lpStr1.uOffset );
			return TRUE;
    
		case STRRET_CSTR:
			mystr = lpStr1.cStr;
			strcpy( lpStr2, lpStr1.cStr );
			return TRUE;
	}
    
	return FALSE;
}
    
    
//********************************************************************************
// utility function
// next
//********************************************************************************
static LPITEMIDLIST next( LPCITEMIDLIST pidl )
{
	LPSTR lpMem=(LPSTR)pidl;
	
	lpMem += pidl->mkid.cb;
	return (LPITEMIDLIST)lpMem;
}
    
    
//********************************************************************************
// utility function
// getSize
//********************************************************************************
static UINT getSize(LPCITEMIDLIST pidl)
{
	UINT cbTotal = 0;
    
	if ( pidl )
	{
		cbTotal += sizeof( pidl->mkid.cb );
		while ( pidl->mkid.cb )
		{
			cbTotal += pidl->mkid.cb;
			pidl = next( pidl );
		}
	}
    
	return cbTotal;
}
    
    
//********************************************************************************
// utility function
// create
//********************************************************************************
static LPITEMIDLIST create( UINT cbSize )
{
	IMalloc*     pMalloc;
	LPITEMIDLIST pidl = 0;
    
	if ( FAILED( SHGetMalloc( &pMalloc ) ) )
		return 0;
    
	pidl = (LPITEMIDLIST)pMalloc->Alloc( cbSize );
    
	if ( pidl )
		memset( pidl, 0, cbSize );
     
	pMalloc->Release();
	
	return pidl;
}
    
    
    
//********************************************************************************
// utility function
// ConcatPidls
//********************************************************************************
static LPITEMIDLIST concatPidls( LPCITEMIDLIST pidl1, LPCITEMIDLIST pidl2 )
{
	UINT         cb1 = getSize( pidl1 ) - sizeof( pidl1->mkid.cb );
	UINT         cb2 = getSize( pidl2 );
	LPITEMIDLIST pidlNew = create( cb1 + cb2 );
    
	if ( pidlNew )
	{
		memcpy( pidlNew, pidl1, cb1 );
		memcpy( ((LPSTR)pidlNew ) + cb1, pidl2, cb2 );
	}
    
	return pidlNew;
}
    
    
//********************************************************************************
// GetMyComputerFolder
// This routine returns the ISHellFolder for the virtual My Computer folder,
// and also returns the PIDL.
//********************************************************************************
HRESULT GetMyComputerFolder( LPSHELLFOLDER* ppshf, LPITEMIDLIST* ppidl )
{
	IMalloc*        pMalloc;
	HRESULT         hres;
    
	hres = SHGetMalloc( &pMalloc );
	if ( FAILED( hres ) )
		return hres;
    
	// Get the PIDL for "My Computer"
	hres = SHGetSpecialFolderLocation( /*pWndOwner->m_hWnd*/NULL, CSIDL_DRIVES, ppidl );
	if ( SUCCEEDED( hres ) )
	{
		IShellFolder*   pshf;
    
		hres = SHGetDesktopFolder( &pshf );
		if ( SUCCEEDED( hres ) )
		{
			// Get the shell folder for "My Computer"
			hres = pshf->BindToObject( *ppidl, NULL, IID_IShellFolder, (LPVOID *)ppshf );
			pshf->Release();
		}
	}
    
	pMalloc->Release();
    
	return hres;
}
    
    
//********************************************************************************
// GetDialupNetworkingFolder
// This routine returns the ISHellFolder for the virtual Dial-up Networking
// folder, and also returns the PIDL.
//********************************************************************************
static HRESULT getDialUpNetworkingFolder(LPSHELLFOLDER *ppshf, LPITEMIDLIST *ppidl)
{
	HRESULT         hres;
    
	IMalloc*        pMalloc = NULL;
	IShellFolder*   pmcf = NULL;
	LPITEMIDLIST    pidlmc;
    
    
	char szDialupName[ 256 ];
	HKEY hKey;
	DWORD cbData;
    
	// Poke around in the registry to find out what the Dial-Up Networking
	//   folder is called on this machine

	szDialupName[ 0 ] = '\0';
	if ( ERROR_SUCCESS == RegOpenKeyEx(HKEY_CLASSES_ROOT, 
				     "CLSID\\{992CFFA0-F557-101A-88EC-00DD010CCC48}",  // Chouck got this from MS tech support
				     NULL, 
				     KEY_QUERY_VALUE, 
				     &hKey ) )
	{
    	cbData = sizeof( szDialupName );
		RegQueryValueEx( hKey, "", NULL, NULL, (LPBYTE)szDialupName, &cbData );
		RegCloseKey( hKey );
	}
    
	// if we didn't get anything just use the default
	if( szDialupName[ 0 ] == '\0' )
	{
		char*	strText;
		strText = "Dial-Up Networking";
		//CString strText;
		//strText.LoadString(IDS_DIAL_UP_NW);
		strcpy( szDialupName, (LPCSTR)strText );
	}
    
	// OK, now look for that folder
    
	hres = SHGetMalloc( &pMalloc );
	if ( FAILED( hres ) )
		return hres;
    
	// Get the virtual folder for My Computer
	hres = GetMyComputerFolder( &pmcf, &pidlmc );
	if ( SUCCEEDED( hres ) )
	{
		IEnumIDList*    pEnumIDList;
    
		// Now we need to find the "Dial-Up Networking" folder
		hres = pmcf->EnumObjects( NULL, SHCONTF_FOLDERS | SHCONTF_NONFOLDERS, &pEnumIDList );
		if ( SUCCEEDED( hres ) )
		{
			LPITEMIDLIST    pidl;
    
			int flag = 1;
			STRRET  name;
			// moved "flag" check to front to benefit from partial eval and avoid extra fn call
			while ( flag && ( NOERROR == ( hres = pEnumIDList->Next( 1, &pidl, NULL ) ) ) ) 
			{
			    memset( &name, 0, sizeof( STRRET ) );
    
				name.uType = STRRET_CSTR;  // preferred choice
				hres = pmcf->GetDisplayNameOf( pidl, SHGDN_INFOLDER, &name );
				if ( FAILED( hres ) )
				{
					flag = 0;
				}
				else if ( sameStrings( pidl, name, szDialupName ) )
				{
					*ppidl = concatPidls(pidlmc, pidl);
					hres = pmcf->BindToObject( pidl, NULL, IID_IShellFolder, (LPVOID*)ppshf );
					int rtn = GetLastError();
					flag = 0;
				}
				
				pMalloc->Free( pidl );
			}
    
			pEnumIDList->Release();
		}
    
		pmcf->Release();
		pMalloc->Free( pidlmc );
	}
    
	pMalloc->Release();
	return hres;
}

//********************************************************************************
// GetDialUpConnection
//********************************************************************************
BOOL GetDialUpConnection95( CONNECTIONPARAMS** connectionNames, int* numNames )
{
	HRESULT			hres;
    
	IMalloc*		pMalloc = NULL;
	IShellFolder*	pshf = NULL;
	LPITEMIDLIST	pidldun;
	LPITEMIDLIST	pidl;
	STRRET			name;
	char			temp[ MAX_PATH ];
	int				flag = 1;
	int				i =0;
    
    
	// Initialize out parameter
	hres = SHGetMalloc( &pMalloc );
	if ( FAILED( hres ) )
		return FALSE;
    
	// First get the Dial-Up Networking virtual folder
	hres = getDialUpNetworkingFolder( &pshf, &pidldun );
	if ( SUCCEEDED( hres ) && ( pshf != NULL ) ) 
	{
		IEnumIDList*    pEnumIDList;
    
		// Enumerate the files looking for the desired connection
		hres = pshf->EnumObjects( NULL, SHCONTF_FOLDERS | SHCONTF_NONFOLDERS, &pEnumIDList );
    
		if ( SUCCEEDED( hres ) ) 
		{
			*numNames = 0;
			while ( NOERROR == ( hres = pEnumIDList->Next( 1, &pidl, NULL ) ) )
				(*numNames)++;
    
			pEnumIDList->Reset();
    
			*connectionNames = new CONNECTIONPARAMS[ *numNames ];
			if( *connectionNames == NULL )
				return FALSE;
    
			// moved "flag" check to front to benefit from partial eval and avoid extra fn call
			while ( flag && ( NOERROR == ( hres = pEnumIDList->Next( 1, &pidl, NULL ) ) ) ) 
			{
				name.uType = STRRET_CSTR;  // preferred choice
				hres = pshf->GetDisplayNameOf( pidl, SHGDN_INFOLDER, &name );
				if ( FAILED( hres ) ) 
				{
					flag = 0;
				}
				else
				{
					procStrings( pidl, name, temp );
					if( strcmp( temp, "Make New Connection" ) !=0 )
					{
						strcpy( (*connectionNames)[ i ].szEntryName, temp );
						(*connectionNames)[ i ].pidl = concatPidls( pidldun, pidl );
						i++;
					}
				}

				pMalloc->Free( pidl );
			}
    
			pEnumIDList->Release();
		}
    
		pshf->Release();
		pMalloc->Free( pidldun );
	}
    
	pMalloc->Release();
	*numNames = i;
    
	return TRUE;
}

//********************************************************************************
// GetDialUpConnection
//********************************************************************************
BOOL GetDialUpConnectionNT( CONNECTIONPARAMS** connectionNames, int* numNames )
{
	RASENTRYNAME*	rasEntryName;
	DWORD           cb;
	DWORD           cEntries;
	int             i;
	char*			szPhoneBook;
	BOOL			rval = FALSE;
	
	szPhoneBook = NULL;
	rasEntryName = new RASENTRYNAME[ MAX_ENTRIES ]; 
	
	if( rasEntryName == NULL )
		return FALSE;
	
	memset( rasEntryName, 0, MAX_ENTRIES );
	rasEntryName[ 0 ].dwSize = sizeof( RASENTRYNAME );
	cb = sizeof( RASENTRYNAME ) * MAX_ENTRIES;
	
	int rtn = (*m_lpfnRasEnumEntries)( NULL, szPhoneBook, rasEntryName, &cb, &cEntries );
	if ( rtn !=0 )
		goto fail;
	
	*numNames = cEntries;
	
	*connectionNames = new CONNECTIONPARAMS[ *numNames + 1 ]; 
	if (*connectionNames == NULL)
		goto fail;
	
	for( i = 0; i < *numNames; i++ )
		strcpy ( (*connectionNames)[ i ].szEntryName, rasEntryName[ i ].szEntryName );
	
	rval = TRUE;

fail:
	delete []rasEntryName;
	return rval;
}

//********************************************************************************
// getDialupConnectionPIDL
//********************************************************************************
static HRESULT getDialUpConnectionPIDL( LPCTSTR lpConnectionName, LPITEMIDLIST* ppidl )
{
	HRESULT			hres;
	
	IMalloc*		pMalloc = NULL;
	IShellFolder*	pshf = NULL;
	LPITEMIDLIST	pidldun;
	
	// Initialize out parameter
	*ppidl = NULL;
	
	hres = SHGetMalloc( &pMalloc );
	if ( FAILED( hres ) )
		return hres;
	
	// First get the Dial-Up Networking virtual folder
	hres = getDialUpNetworkingFolder( &pshf, &pidldun );
	if ( SUCCEEDED( hres ) && ( pshf != NULL ) )
	{
		IEnumIDList*    pEnumIDList;
	
		// Enumerate the files looking for the desired connection
		hres = pshf->EnumObjects( /*pWndOwner->m_hWnd*/NULL, SHCONTF_FOLDERS | SHCONTF_NONFOLDERS, &pEnumIDList );
		if ( SUCCEEDED( hres ) )
		{
			LPITEMIDLIST    pidl;
	
			int flag = 1;
			while ( ( NOERROR == ( hres = pEnumIDList->Next( 1, &pidl, NULL ) ) ) && flag )
			{
				STRRET  name;
	
				name.uType = STRRET_CSTR;  // preferred choice
				hres = pshf->GetDisplayNameOf( pidl, SHGDN_INFOLDER, &name );
				if ( FAILED( hres ) )
				{
					flag = 0;
				}
				else if ( sameStrings( pidl, name, lpConnectionName ) )
				{
					*ppidl = concatPidls( pidldun, pidl );
					flag = 0;
				}
	
				pMalloc->Free(pidl);
			}
	
			pEnumIDList->Release();
		}
	
		pshf->Release();
		pMalloc->Free( pidldun );
	}
	
	pMalloc->Release();
	return hres;
}

//********************************************************************************
// getNetscapePIDL
//********************************************************************************
static void getNetscapePidl( LPITEMIDLIST* ppidl )
{
	MessageBox(NULL, "Need to get Netscape install directory from registry or somesuch", "NSCP", MB_ICONSTOP);

	char				szPath[ MAX_PATH ], *p;
	OLECHAR				olePath[ MAX_PATH ];
	IShellFolder*		pshf;
	
	GetModuleFileName( gDLL, szPath, sizeof( szPath ) );
	//GetModuleFileName(AfxGetInstanceHandle(), szPath, sizeof(szPath));
	
	//we need to take off \plugins\npasw.dll from the path
	p = strrchr( szPath, '\\' );
	if ( p ) 
		*p = '\0';
	p = strrchr( szPath, '\\' );
	if ( p ) 
		*p = '\0';
	strcat( szPath, "\\netscape.exe" );
	
	HRESULT hres = SHGetDesktopFolder( &pshf );
	if ( SUCCEEDED( hres ) )
	{
		MultiByteToWideChar( CP_ACP, MB_PRECOMPOSED, (LPCSTR)szPath, -1, (LPWSTR)olePath, sizeof( olePath ) );
	
		ULONG lEaten;
		pshf->ParseDisplayName( NULL, NULL, (LPOLESTR)olePath, &lEaten, ppidl, NULL );
	}
	
	return;
}


//********************************************************************************
// getMsgString
// loads a Message String from the string table
//********************************************************************************
static BOOL getMsgString( char* buf, UINT uID )
{
	if ( !buf )
		return FALSE;

	// returns zero when successful
	int ret = LoadString( gDLL, uID, buf, 255 );

	return (ret == 0);
}

//********************************************************************************
// getSystemDirectory_1()
// intended to be a more fail-safe version of GetSystemDirectory
// returns:
//	NULL if the path cannot be obtained for any reason
//	otherwise, the "GetSystemDirectory" path
//********************************************************************************
static char* getSystemDirectory_1()
{
	UINT			startLength = MAX_PATH;
	UINT			sysPathLength = MAX_PATH;
	char*			sysPath;
	
	// Why aren't we using realloc for this???

start:
	sysPath = (char*)malloc( sizeof(char) * startLength );
	if ( !sysPath ) 
		return NULL;
	sysPathLength = ::GetSystemDirectory( sysPath, startLength );
	if ( sysPathLength == 0 )  // 0 indicates failure of the call
		return NULL;
	if ( sysPathLength > startLength )
	{
		free( sysPath );
		startLength += sysPathLength;  // make it arbitrarily big
		goto start;
	}
	return sysPath;
}

//********************************************************************************
// getPhoneBookNT()
// returns the path to the NT Phone Book file (rasphone.pbk)
// returns:
//	NULL if the path cannot be obtained for any reason
//	otherwise, the phone book file path
//********************************************************************************
static char* getPhoneBookNT()
{
	char*			sysPath;
	char*			pbPath;
	
	sysPath = getSystemDirectory_1();
	if ( !sysPath )
		return FALSE;
		
	pbPath = (char*)malloc( sizeof(char) * strlen( sysPath ) + 30 );
	if ( !pbPath )
	{
		free( sysPath );
		return NULL;
	}
	
	strcpy( pbPath, sysPath );
	strcat( pbPath, "\\ras\\rasphone.pbk" );
	strcat( pbPath, "\0" );
	free( sysPath );
	return pbPath;	
}

//********************************************************************************
// startRasMonNT()
// starts the "rasmon" process (rasmon.exe)
// returns:
//	FALSE if the process cannot be started for any reason
//	TRUE otherwise
//********************************************************************************
static BOOL startRasMonNT()
{
	// starts up RASMON process
	PROCESS_INFORMATION pi;
	BOOL                ret;
	STARTUPINFO         sti;
	UINT                err = ERROR_SUCCESS;
	char*				sysPath;
	char*				rasMonPath;
	
	// See if it's already running
	HWND hWnd = FindWindowEx( NULL, NULL, "RasmonWndClass", "Dial-Up Networking Monitor" );
	if (hWnd)
		return TRUE;
	else
	{
		// Oh well, try to start it...
		sysPath = getSystemDirectory_1();
		if ( !sysPath )
			return FALSE;

		rasMonPath = (char*)malloc( sizeof(char) * strlen( sysPath ) + 30 );
		if ( !rasMonPath )
		{
			free( sysPath );
			return FALSE;
		}
	
		strcpy( rasMonPath, sysPath );
		strcat( rasMonPath, "\\rasmon.exe" );
		strcat( rasMonPath, "\0" );
		free( sysPath );
	
		memset( &sti, 0, sizeof( sti ) );
		sti.cb = sizeof( STARTUPINFO );
	
		// Run the RASMON app (apparently it's idempotent)
		ret = ::CreateProcess( rasMonPath, NULL, NULL, NULL, FALSE, 0, NULL, NULL, &sti, &pi );
		if ( ret == TRUE )
		{
			gRasMon = pi.hProcess;
			//Sleep( 3000 );
			// Instead of just sleeping for 3 seconds, 
			// wait until the process is up or 3 seconds, whichever comes first
			WaitForInputIdle(gRasMon, 3000);

			// No need to hang onto these
			CloseHandle(pi.hThread);
			CloseHandle(pi.hProcess);
		}
		free( rasMonPath );
		return ret;
	}
}

static void setStatusDialogText( int iField, const char* pText )
{
	if ( gHwndStatus )
	{
		HWND hField = GetDlgItem( gHwndStatus, iField );
		if ( hField )
			SetWindowText( hField, pText );
	}
}

#undef USING_REMOVE
static void removeStatusDialog()
{
	if ( gHwndStatus )
	{
		EndDialog( gHwndStatus, TRUE );
		gHwndStatus = NULL;
	}
}

static int displayErrMsgWnd( char* text, int style, HWND hwnd )
{
	char		title[ 50 ];
	getMsgString( (char*)&title, IDS_APP_NAME );
	
	if ( hwnd == NULL )
		hwnd = GetActiveWindow();
		
	int ret = MessageBox( hwnd, text, title, style );
	return ret;
}

static void displayDialErrorMsg( DWORD dwError )
{
	char    szErr[ 256 ];
	char    szErrStr[ 256 ];
	HWND	wind;
	
//	ASSERT( m_lpfnRasGetErrorString );
	(*m_lpfnRasGetErrorString)( (UINT)dwError, szErr, sizeof(szErr) );
	
	// some of the default error strings are pretty lame
	switch ( dwError )
	{
		case ERROR_NO_DIALTONE:
			getMsgString( szErr, IDS_NO_DIALTONE );
		break;
	
		case ERROR_LINE_BUSY:
			getMsgString( szErr, IDS_LINE_BUSY );
		break;

		case ERROR_PROTOCOL_NOT_CONFIGURED:
			getMsgString( szErr, IDS_PROTOCOL_NOT_CONFIGURED );

		default:
		break;
	}
	
	getMsgString( szErrStr, IDS_CONNECTION_FAILED );
	strcat( szErrStr, szErr );
	
	wind = gHwndStatus;
	if ( !wind )
		wind = gHwndParent;
	
	displayErrMsgWnd( szErrStr, MB_OK | MB_ICONEXCLAMATION, wind );  
}

static void connectErr( DWORD dwError )
{
	
	char strText[ 255 ];

	if ( gHwndStatus )
	{
        getMsgString( (char*)strText, IDS_DIAL_ERR );
        setStatusDialogText( IDC_DIAL_STATUS, strText );
		Sleep( 1000 );
#ifdef USING_REMOVE
		removeStatusDialog();
#endif
	}

	gDeviceErr = TRUE;		// some sort of device err
	displayDialErrorMsg( dwError );
}

BOOL CALLBACK statusDialogCallback( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
	BOOL		bRetval = FALSE;
	DWORD		dwRet;

	if ( !gHwndStatus )
		gHwndStatus = hWnd;

	switch ( uMsg )
	{
		case WM_INITDIALOG:
			return TRUE;
		break;
		
		case WM_COMMAND:
		{
			WORD wNotifyCode = HIWORD( wParam );
            WORD wID = LOWORD( wParam );
            HWND hControl = (HWND)lParam;

            switch ( wID )
            {
				case IDDISCONNECTED:
//                if (AfxMessageBox(IDS_LOST_CONNECTION, MB_YESNO) == IDYES)
//                   m_pMainWnd->PostMessage(WM_COMMAND, IDC_DIAL);
					break;

				case IDCANCEL:
				{
					// RasHangUp & destroy dialog box
					bRetval = TRUE;
					gCancelled = TRUE;

//					ASSERT( m_lpfnRasHangUp );

					char strText[ 255 ];

					getMsgString( strText, IDS_CANCELDIAL );
		            setStatusDialogText( IDC_DIAL_STATUS, strText );		
	
					dwRet = ( *m_lpfnRasHangUp )( gRasConn );

					if ( dwRet == ERROR_INVALID_HANDLE)
						trace("dialer.cpp : statusDialogCallback - Can't hangup. Invalid Connection Handle. (r)");
					else if ( dwRet && dwRet != ERROR_INVALID_HANDLE )
						trace("dialer.cpp : statusDialogCallback - Can't hangup. Error %d. (r)", dwRet);

					//Sleep( 3000 );
					//
					// OK, the RasHangup doc says there's a better way to do this...
					while ( (*m_lpfnRasGetConnectStatus)( gRasConn) != ERROR_INVALID_HANDLE)
						Sleep( 0 );

#ifdef USING_REMOVE
					removeStatusDialog();
#endif
					break;
				}
			}
		}

	}
	return bRetval;
}

// Why is this distributed to here???
static void setCallState( CallState newState )
{
	gCallState = newState;
    
	switch ( gCallState )
	{
		case StateConnected:
			// destroy our connection status window
#ifdef USING_REMOVE
			removeStatusDialog();
#endif
		break;

		case StateConnecting:
		{                                          // make this a block to contain local vars
			// creates status dialog box
			HWND	hwndParent = GetActiveWindow();
			int		nResult;
			
			nResult = DialogBox( gDLL, MAKEINTRESOURCE( IDD_STATUS ), hwndParent, (DLGPROC)statusDialogCallback );
//			ASSERT( nResult != -1 );
		}
		break;

		default:
		break;
	}
}

// The docs state that reaching RASCS_connected, any dwError != 0, and calling RasHangup guarantees no
// further events to this callback.
//
//********************************************************************************
// RasDialFunc
//  call back function for RasDial
//
// This callback is always called once with dwError=0 before the corresponding state change.
// If there's an error, it's called _again_ with dwError<>0 to report the error.
//********************************************************************************
void CALLBACK
RasDialFunc( HRASCONN hRasConn, UINT uMsg,
	RASCONNSTATE rasConnState, DWORD dwError, DWORD dwExtendedError )
{
	// According to the docs, sleeping shouldn't be necessary at all,
	// but Cathleen says there were problems without them.  So, we'll 
	// try it out with small values to see if that's sufficient...
	int sleepyTime = 100;  // was 1000, trying smaller values

	if ( uMsg != WM_RASDIALEVENT )
		return;
	                                     // ignore all other messages
	gRASstate = rasConnState;

	char		strText[ 255 ];
	DWORD		dwRet;

	// Disconnect is special...
	if (rasConnState == RASCS_Disconnected)
	{
		// If this is an unexpected disconnect then hangup and take 
		// down the status dialog box
		if ( gCallState == StateConnected || gCallState == StateConnecting )
		{
//			ASSERT( m_lpfnRasHangUp );
			dwRet = ( *m_lpfnRasHangUp )( gRasConn );
		
			if ( dwRet == ERROR_INVALID_HANDLE )
				trace("dialer.cpp : ProcessRasDialEvent (stateConnected) - Can't hangup. Invalid Connection Handle.");
			else if ( dwRet && dwRet != ERROR_INVALID_HANDLE )
				trace("dialer.cpp : ProcessRasDialEvent (stateConnected) - Can't hangup. Error %d", dwRet);

			//Sleep( 3000 );
			//
			// OK, the RasHangup doc says there's a better way to do this...
			while ( (*m_lpfnRasGetConnectStatus)( gRasConn) != ERROR_INVALID_HANDLE)
				Sleep( 0 );

			if ( gCallState == StateConnecting )
			{
				if ( dwError != SUCCESS )
				{
					if ( gHwndStatus )
					{
						getMsgString( strText, IDS_DISCONNECTING );
						setStatusDialogText( IDC_DIAL_STATUS, strText );
					}
				}
			}

			// here we pass redial msg if needed.
#ifdef USING_REMOVE
			removeStatusDialog();
#endif
			if ( gCallState == StateConnecting )
			{
				gLineDrop = TRUE;  // remove if we ask users for redial
				displayDialErrorMsg( dwError );
			}
		}
		
		setCallState( StateIdle );
	}

	// All errors are handled generically if not disconnecting
	else if ( dwError)
		connectErr( dwError );

	// Normal state change processing...
	else
	{
		switch ( rasConnState )
		{
			case RASCS_OpenPort:
				// wait for status dialog to show up first
				while ( gHwndStatus == NULL )
					Sleep( sleepyTime );
		
				getMsgString( strText, IDS_OPENING_PORT );
				setStatusDialogText( IDC_DIAL_STATUS, strText );		
			break;
		
			case RASCS_PortOpened:
				getMsgString( strText, IDS_INIT_MODEM );
				setStatusDialogText( IDC_DIAL_STATUS, strText );
		
			break;
		
			case RASCS_ConnectDevice:
				if ( gDialAttempts == 1 )
				{
					getMsgString( strText, IDS_DIALING );
					setStatusDialogText( IDC_DIAL_STATUS, strText );
				}
				else
				{
					char    szBuf[ 128 ];
		
					getMsgString(strText, IDS_DIALING_OF);
					wsprintf( szBuf, (LPCSTR)strText, gDialAttempts, NUM_ATTEMPTS );
					setStatusDialogText( IDC_DIAL_STATUS, strText );
				}
			break;
		
			case RASCS_Authenticate:
				getMsgString( strText, IDS_VERIFYING );
				setStatusDialogText( IDC_DIAL_STATUS, strText );
			break;
		
			case RASCS_Authenticated:
				getMsgString( strText, IDS_LOGGING_ON );
				setStatusDialogText( IDC_DIAL_STATUS, strText );
			break;
		
			case RASCS_Connected:
				getMsgString( strText, IDS_CONNECTED );
				setStatusDialogText( IDC_DIAL_STATUS, strText );
				setCallState( StateConnected );
			break;
		
		
			case RASCS_WaitForModemReset:
				getMsgString( strText, IDS_DISCONNECTING );
				setStatusDialogText( IDC_DIAL_STATUS, strText );
				trace("dialer.cpp : ProcessRasDialEvent (WaitForModemReset) - Connection Error %d", dwError);
		
			break;
		
			default:
				trace( "dialer.cpp : ProcessRasDialEvent (default case) - Connection Error %d", dwError );
			break;
		}
		Sleep( sleepyTime );
	}
}


// !!!  NB:  In 4.x, this function was a top-level native function called from Java.
// !!!       This probably contributed to all the sleep() calls because returning from
// !!!       this routine probably destroys this thread's context.  That context is 
// !!!       necessary for the callback routine to function properly, so errors would
// !!!       occur unless you wait for the last event.

//********************************************************************************
// DialerConnect
// initiates a dialer connection (used if Dial on Demand is disabled)
// assumes the dialer has already been configured
// returns:
//	FALSE if the dialer cannot be connected
//	TRUE if the dialer is already connected or if it was able to connect
//		successfully
//
// sets these globals: gHwndParent, gDialAttempts, gRasConn, gCancelled
//
// RasDialDlg would be a replacement for this but VC++ QuickInfo says it's NT only :-(
//********************************************************************************
BOOL DialerConnect()
{
	DWORD			dwError;
	DWORD			dwRet;
	BOOL			connectSucceed = TRUE;

	gHwndParent = GetActiveWindow();

	// return if dialer already connected
	if ( IsDialerConnected() )
		return TRUE;
 
	gDialAttempts = 1;
	gRasConn = NULL; 		// init global connection handle
	gCancelled = FALSE;		// assume connection is not canceled by the user, unless otherwise


	char*			pbPath = NULL;
	LPVOID			notifierProc = NULL;
	DWORD			notifierType = 0;
	
	notifierType = 1;
	notifierProc = (LPVOID)RasDialFunc;
	
	// WinNT40 find system phone book first then start RASDIAL
	if ( gPlatformOS == VER_PLATFORM_WIN32_NT )
	{
		// Searches first, then starts RasMon if not found
		startRasMonNT();	
		
		// should check the error here, but I don't know
		// what to do if we fail (report that error?)

		pbPath = getPhoneBookNT();
	}

//	ASSERT( m_lpfnRasDial );
	// do the dialing here
	dwError = ( *m_lpfnRasDial )( NULL, pbPath, &gDialParams, notifierType, notifierProc, &gRasConn );

	if ( dwError == ERROR_NOT_ENOUGH_MEMORY )
		trace( "dialer.cpp : [native] DialerConnect - Not enough memory for dialing activity. Dialing failed." );

	else if ( dwError )
		trace( "dialer.cpp : [native] DialerConnect - Dialing failed. Error code = %d", dwError );

	   
	if ( dwError == SUCCESS )
	{
		// Dialing succeeded
		// display connections status dialog & dispatch window msgs...

		MSG			msg;
		setCallState( StateConnecting );
		while ( (	( gRASstate != RASCS_Connected ) &&
					( gRASstate != RASCS_Disconnected ) ) &&
				( !gCancelled ) &&
				( !gLineDrop ) &&
				( !gDeviceErr) )
		{
			if ( ::GetMessage( &msg, NULL, 0, 0 ) )
			{
				::TranslateMessage( &msg );
				::DispatchMessage( &msg );
			}
			else
			{
				// WM_QUIT!!!
				break;
			}
		}
		
#ifdef USING_REMOVE
		removeStatusDialog();  
#endif
		if ( ( gRASstate != RASCS_Connected ) || gCancelled )
			connectSucceed = FALSE;

	}
	else
	{
		// dialing failed!!!, display err msg
		connectSucceed = FALSE;
		displayDialErrorMsg( dwError );
	}

	// According to the docs, we must do a hangup call if the gRasConn is not null!
	if ( !connectSucceed || gRasConn )
	{
		// hangup connection
		if ( gRasConn )
		{
//			ASSERT( m_lpfnRasHangUp );
			dwRet = ( *m_lpfnRasHangUp )( gRasConn );
			
			if ( dwRet == ERROR_INVALID_HANDLE )
				trace("dialer.cpp : [native] DialerConnect - Can't hangup. Invalid Connection Handle.");
			else if ( dwRet )
				trace("dialer.cpp : [native] DialerConnect - Can't hangup. Error %d", dwRet);
			else
				; // guaranteed no further calls to callback function from this point on

			// give RasHangUp some time till complete hangup
#ifdef USING_REMOVE
			removeStatusDialog();
#endif

			// wait three seconds for tear-down of notification proc.
			// should figure out how to do this better (maybe set something
			// in RasDialFunc which ensures us not getting called again?)
			//Sleep( 3000 );
			//
			// OK, the RasHangup doc says there's a better way to do this...
			while ( (*m_lpfnRasGetConnectStatus)( gRasConn) != ERROR_INVALID_HANDLE)
				Sleep( 0 );
		}
	}

	SetActiveWindow( gHwndParent );
                                             
	// Assuming this routine is only valid owner of this status dialog;
	// change setting of USING_REMOVE if that's not true
	removeStatusDialog();

	return connectSucceed;
}


// !!!  Only caller is in muc.cpp.  This seems strange...
//
//********************************************************************************
// DialerHangup
// 	terminates all active dialer connections!!
// returns:
//	nothing
//********************************************************************************
void DialerHangup() 
{
	RASCONN*		info = NULL;
	RASCONN*		lpTemp = NULL;
	DWORD			code;
	DWORD			count = 0;
	DWORD			dSize = stRASCONN;
	DWORD			dwRet;
    BOOL			triedOnce = FALSE;
    
tryAgain:
	// try to get a buffer to receive the connection data
	info = (RASCONN*)LocalAlloc( LPTR, (UINT)dSize );

	if ( !info )
		return;
	
	// set RAS struct size
 	info->dwSize = dSize;

//	ASSERT( m_lpfnRasEnumConnections);
	// enumerate open connections
	code = ( *m_lpfnRasEnumConnections )( info, &dSize, &count );

	if ( code != 0 && triedOnce == TRUE )
	{
		LocalFree( info );
		return;
	}
	
	if ( code == ERROR_BUFFER_TOO_SMALL )
	{
		triedOnce = TRUE;
		
		// free the old buffer & allocate a new bigger one
		LocalFree( info );
		goto tryAgain;
	}
	
	// check for no connections
	if ( count == 0 )
	{
		LocalFree( info );
		return;
	}

	// just hang up everything
	int i;
//	ASSERT( m_lpfnRasHangUp );
	for ( i = 0; i < (int) count; i++ )
	{
		dwRet = ( *m_lpfnRasHangUp )( info[ i ].hrasconn );

		if ( dwRet == ERROR_INVALID_HANDLE )
			trace( "dialer.cpp : DialerHangup - Can't hangup. Invalid Connection Handle." );
		else if ( dwRet )
			trace("dialer.cpp : DialerHangup - Can't hangup. Error %d.", dwRet);

		// Don't do this here, hang them all up and then wait for them afterwards
		//Sleep( 3000 );
	}

	// Wait for them all to hang up
	for ( i = 0; i < (int) count; i++ )
		while ( (*m_lpfnRasGetConnectStatus)( info[ i ].hrasconn ) != ERROR_INVALID_HANDLE)
			Sleep( 0 );

	LocalFree( info );
}

//********************************************************************************
// findDialerData
// search the javascript array for specific string value
//********************************************************************************
char* findDialerData( char** dialerData, char* name )
{
	// Convert to using globals
	MessageBox(NULL, "Need to get global values differently now", "NSCP", MB_ICONSTOP);

	char**	entry = dialerData;
	char*	value;
	
	while ( *entry )
	{
		if ( strncmp( *entry, name, strlen( name ) ) == 0 )
		{
			value = strchr( *entry, '=' );
			if ( value )
				value++;
			break;
		}
		entry++;
	}
	
	return (char*)value;
}


//********************************************************************************
// fillAccountParameters
// fill in account information, given from JS array
//********************************************************************************
//static void fillAccountParameters( char** dialerData, ACCOUNTPARAMS* account )
static void fillAccountParameters( ACCOUNTPARAMS* account )
{
//	char*		value;
/*	
	value = findDialerData( dialerData, "AccountName" );
	strcpy( account->ISPName, value ? value : "My Account" ); 
	
	// file name
	value = findDialerData( dialerData, "FileName" );
	strcpy( account->FileName, value ? value : "My Account" );
	// DNS
	value = findDialerData( dialerData, "DNSAddress" );
	strcpy( account->DNS, value ? value : "0.0.0.0" );
	// DNS2
	value = findDialerData( dialerData, "DNSAddress2" );
	strcpy( account->DNS2, value ? value : "0.0.0.0" );
	// domain name
	value = findDialerData( dialerData, "DomainName" );
	strcpy( account->DomainName, value ? value : "" );
	// login name
	value = findDialerData( dialerData, "LoginName" );
	strcpy(account->LoginName, value ? value : "");
	// password
	value = findDialerData( dialerData, "Password" );
	strcpy( account->Password, value ? value : "" );
	// script file name
	value = findDialerData( dialerData, "ScriptFileName" );
	strcpy( account->ScriptFileName, value ? value : "" );
	// script enabled?
	value = findDialerData( dialerData, "ScriptEnabled" );
	if ( value )
	{
		account->ScriptEnabled = ( strcmp( value, "TRUE" ) == 0 );
		// get script content
		value = findDialerData( dialerData, "Script" );
		if ( value )
		{
		//	ReggieScript = (char*) malloc(strlen(value) + 1);
		//	strcpy(ReggieScript, value);
		}		   	
	}
	else
		account->ScriptEnabled = 0;
	// need TTY window?
	value = findDialerData( dialerData, "NeedsTTYWindow" );
	account->NeedsTTYWindow = ( value && ( strcmp( value, "TRUE" ) == 0 ) );
	// VJ compression enabled?
	value = findDialerData( dialerData, "VJCompresssionEnabled" );
	account->VJCompressionEnabled = ( value && ( strcmp( value, "TRUE" ) == 0 ) );
	// International mode?
	value = findDialerData( dialerData, "IntlMode" );
	account->IntlMode = ( value && ( strcmp( value, "TRUE" ) == 0 ) );
	// dial on demand?
	value = findDialerData( dialerData, "DialOnDemand" );
	account->DialOnDemand = ( value && ( strcmp( value, "TRUE" ) == 0 ) );
	// isp phone number
	value = findDialerData( dialerData, "ISPPhoneNum" );
	if ( value )
		strcpy( account->ISPPhoneNum, value );
	else
		strcpy( account->ISPPhoneNum, "" );
	// ISDN phone number
	value = findDialerData( dialerData, "ISDNPhoneNum" );
	if ( value )
		strcpy( account->ISDNPhoneNum, value );
	else
		strcpy( account->ISDNPhoneNum, "" );
*/	
	strcpy( account->ISPName,"Account3" ); 
	
	// file name

	//strcpy( account->FileName,"This Account" );
	// DNS

	strcpy( account->DNS,"208.12.38.38" );
	// DNS2

	//strcpy( account->DNS2, "208.12.38.38");
	// domain name

	//strcpy( account->DomainName,"adonai.mcom.com" );
	// login name

	//strcpy(account->LoginName, "mozilla");
	// password

	//strcpy( account->Password,"mozilla" );
	// script file name

	//strcpy( account->ScriptFileName,"Scriptfilename" );
	// script enabled?
//	value = findDialerData( dialerData, "ScriptEnabled" );
//	if ( value )
//	{
//		account->ScriptEnabled = ( strcmp( value, "TRUE" ) == 0 );
		// get script content
//		value = findDialerData( dialerData, "Script" );
//		if ( value )
//		{
		//	ReggieScript = (char*) malloc(strlen(value) + 1);
		//	strcpy(ReggieScript, value);
//		}		   	
//	}
//	else
//		account->ScriptEnabled = 0;
	// need TTY window?

//	account->NeedsTTYWindow = 0;
	// VJ compression enabled?

//	account->VJCompressionEnabled = 0;
	// International mode?

//	account->IntlMode = 0;
	// dial on demand?

//	account->DialOnDemand =1;
	// isp phone number
	strcpy( account->ISPPhoneNum, "9373333");
	// ISDN phone number
//	strcpy( account->ISDNPhoneNum, "" );

}


//********************************************************************************
// fillLocationParameters
// fill in location information, given from JS array
//********************************************************************************
//static void fillLocationParameters( char** dialerData, LOCATIONPARAMS* location )
static void fillLocationParameters( LOCATIONPARAMS* location )

{

//	char*		value;
/*	
	// modem name
	value = findDialerData( dialerData, "ModemName" );
	strcpy( location->ModemName, value ? value : "" );
	// modem type
	value = findDialerData( dialerData, "ModemType" );
	strcpy( location->ModemType, value ? value : "" );
	// dial type
	value = findDialerData( dialerData, "DialType" );
	location->DialType = !( value && ( strcmp( value, "TONE" ) == 0 ) );
	// outside line access
	value = findDialerData( dialerData, "OutsideLineAccess" );
	strcpy( location->OutsideLineAccess, value ? value : "" );
	// disable call waiting?
	value = findDialerData( dialerData, "DisableCallWaiting");
	location->DisableCallWaiting = ( value && ( strcmp(value, "TRUE") == 0 ) );
	// disable call waiting code
	value = findDialerData( dialerData, "DisableCallWaitingCode" );
	strcpy( location->DisableCallWaitingCode, value ? value : "" );
	// user area code
	value = findDialerData( dialerData, "UserAreaCode" );
	strcpy( location->UserAreaCode, value ? value : "" );
	// user country code
	value = findDialerData( dialerData, "CountryCode" );
	if ( value )
	{
		char*	stopstr = "\0";
		location->UserCountryCode = (short)strtol( value, &stopstr, 10 );
	}
	else
		location->UserCountryCode = 1;   // default to US
	// dial as long distance?
	value = findDialerData( dialerData, "DialAsLongDistance");
	location->DialAsLongDistance = ( value && ( strcmp( value, "TRUE" ) == 0 ) );	
	// long distance access
	value = findDialerData( dialerData, "LongDistanceAccess" );
	strcpy( location->LongDistanceAccess, value ? value : "" );
	// dial area code?
	value = findDialerData( dialerData, "DialAreaCode" );
	location->DialAreaCode = ( value && ( strcmp( value, "TRUE" ) == 0 ) );	
	// dial prefix code
	value = findDialerData( dialerData, "DialPrefix" );
	strcpy( location->DialPrefix, value ? value : "" );
	// dial suffix code
	value = findDialerData( dialerData, "DialSuffix" );
	strcpy( location->DialSuffix, value ? value : "" );
	// use both ISDN lines?
	value = findDialerData( dialerData, "UseBothISDNLines" );
	location->UseBothISDNLines = ( value && ( strcmp( value, "TRUE" ) == 0 ) );	
	// 56k ISDN?
	value = findDialerData( dialerData, "56kISDN" );
	location->b56kISDN = ( value && ( strcmp( value, "TRUE" ) == 0 ) );
	// disconnect time
	value = findDialerData( dialerData, "DisconnectTime" );
	if ( value )
		location->DisconnectTime = atoi( value );
	else
		location->DisconnectTime = 5;
*/
	// modem name
	strcpy( location->ModemName,"Sportster 28800 External" );
	// modem type

//	strcpy( location->ModemType,"RASDT_Modem" );
	// dial type

	location->DialType = 1;
	// outside line access

	strcpy( location->OutsideLineAccess,"9" );
	// disable call waiting?

//	location->DisableCallWaiting = 0;
	// disable call waiting code

//	strcpy( location->DisableCallWaitingCode,"" );
	// user area code

	strcpy( location->UserAreaCode,"650" );
	// user country code

	location->UserCountryCode = 1;   // default to US
	// dial as long distance?

//	location->DialAsLongDistance = 0;	
	// long distance access

//	strcpy( location->LongDistanceAccess, "" );
	// dial area code?

//	location->DialAreaCode = 1;	
	// dial prefix code

//	strcpy( location->DialPrefix, "" );
	// dial suffix code

//	strcpy( location->DialSuffix, "" );
	// use both ISDN lines?

//	location->UseBothISDNLines =0;	
	// 56k ISDN?

//	location->b56kISDN =0;
	// disconnect time


//	location->DisconnectTime = 5;

}

//********************************************************************************
// onlyOneSet
// Just an XOR of DialAsLongDistance & DialAreaCode - if only one of them is
// set then we can't use MS Locations (if neither are set then we can use 
// locations, but disable use of both - they just don't allow disabling of each
// individually)
//********************************************************************************
static BOOL onlyOneSet( const LOCATIONPARAMS& location )
{
	return ( location.DialAsLongDistance ?
		( location.DialAreaCode ? FALSE : TRUE ) :
		( location.DialAreaCode ? TRUE : FALSE ) );
}

//********************************************************************************
// prefixAvail
// returns:
//	TRUE if there are prefixes that make location unusable
//********************************************************************************
static BOOL prefixAvail( const LOCATIONPARAMS& location )
{
	return ( location.DisableCallWaiting && location.DisableCallWaitingCode[ 0 ] != 0 ) ||
		( location.OutsideLineAccess && location.OutsideLineAccess[ 0 ] != 0 );
}

//********************************************************************************
// parseNumber
//
// Parses a canonical TAPI phone number into country code, area code, and
// local subscriber number
//********************************************************************************
static void parseNumber( LPCSTR lpszCanonical, LPDWORD lpdwCountryCode,
	LPSTR lpszAreaCode, LPSTR lpszLocal)
{
	//*** sscanf dependency removed for win16 compatibility
	char		temp[ 256 ];
	int			p1, p2;

	// Initialize our return values
	*lpdwCountryCode = 1;  // North America Calling Plan
	*lpszAreaCode = '\0';
	*lpszLocal = '\0';

	if ( !lpszCanonical || !*lpszCanonical )
		return;

	// We allow three variations (two too many):
	//  -: +1 (415) 428-3838    (TAPI canonical number)
	//  -: (415) 428-3838       (TAPI canonical number minus country code)
	//  -: 428-3838             (subscriber number only)
	//
	// NOTE: this approach only works if there is a city/area code. The TAPI
	// spec says the city/area code is optional for countries that have a flat
	// phone numbering system

	// Take my advice, always start at the beginning.
	p1 = 0;

	// Allow spaces
	while ( lpszCanonical[p1] == ' ' )
		p1++;

	// Handle the country code if '+' prefix seen
	if ( lpszCanonical[p1] == '+' )
	{
		p1++;
		if ( !isdigit( lpszCanonical[ p1 ] ) )
			return;

		p2 = p1;
		while ( isdigit( lpszCanonical[ p1 ] ) )
			p1++;
		strncpy( temp, &lpszCanonical[ p2 ], p1 - p2 );
		*lpdwCountryCode = atoi( temp );
	}

	// Allow spaces
	while ( lpszCanonical[p1] == ' ' )
		p1++;

	// Handle the area code if '(' prefix seen
	if ( lpszCanonical[ p1 ] == '(' )
	{
		p1++;

		// Allow spaces
		while ( lpszCanonical[p1] == ' ' )
			p1++;

		if ( !isdigit( lpszCanonical[ p1 ] ) )
			return;

		p2 = p1;
		while ( isdigit( lpszCanonical[ p1 ] ) )
			p1++;
		strncpy( lpszAreaCode, &lpszCanonical[ p2 ], p1 - p2 );

		// Allow spaces
		while ( lpszCanonical[p1] == ' ' )
			p1++;

		p1++;      // Skip over the trailing ')'
	}

	// Allow spaces
	while ( lpszCanonical[p1] == ' ' )
		p1++;

	// Whatever's left is the subscriber number (possibly including the whole string)
	strcpy( lpszLocal, &lpszCanonical[ p1 ] );
}

//********************************************************************************
// composeNumber
// create a phone number encompassing all of the location information to hack
// around dialup networking ignoring the location information if you turn off
// the "dial area and country code" flag
//********************************************************************************
static void composeNumber( ACCOUNTPARAMS& account, const LOCATIONPARAMS& location, char csNumber[] )
{
	// if they need to dial something to get an outside line next
	if ( location.OutsideLineAccess[ 0 ] != 0 )
	{
        strcat( csNumber, location.OutsideLineAccess );
        strcat( csNumber, " " );
	}

	// add disable call waiting if it exists
	if ( location.DisableCallWaiting && location.DisableCallWaitingCode[ 0 ] != 0 )
	{
		strcat( csNumber, location.DisableCallWaitingCode );
		strcat( csNumber, " ");
	}
	
	if ( account.IntlMode )
	{ 
	
		// In international mode we don't fill out the area code or
		//   anything, just the exchange part
		strcat( csNumber, account.ISPPhoneNum );
	}
	else
	{

		// lets parse the number into pieces so we can get the area code & country code
		DWORD		nCntry;
		char		szAreaCode[ 32 ];
		char		szPhoneNumber[ 32 ];
		parseNumber( account.ISPPhoneNum, &nCntry, szAreaCode, szPhoneNumber );
		
		// dial the 1 (country code) first if they want it
		if ( location.DialAsLongDistance )
		{
			char	cntry[ 10 ];
			ultoa( nCntry, cntry, 10 );
		
			if ( strcmp( location.LongDistanceAccess, "" ) == 0 )
				strcat( csNumber, cntry );
			else 
				strcat( csNumber, location.LongDistanceAccess );
		
			strcat( csNumber, " " );
		}
		
		// dial the area code next if requested
		if ( location.DialAreaCode )
		{
			strcat( csNumber, szAreaCode );
			strcat( csNumber, " " );
		}
	
		// dial the local part of the number
		strcat( csNumber, szPhoneNumber );
	}
}

//********************************************************************************
// toNumericAddress
// converts from dotted address to numeric internet address
//********************************************************************************
static BOOL toNumericAddress( LPCSTR lpszAddress, DWORD& dwAddress )
{
	//*** sscanf dependency removed for win16 compatibility

	char		temp[ 256 ];
	int			a, b, c, d;
	int			p1, p2;

	strcpy( temp, lpszAddress );

	p2 = p1 = 0;
	while ( temp[ p1 ] != '.' )
		p1++;
	temp[ p1 ] = '\0';
	a = atoi( &temp[ p2 ] );
	
	p2 = ++p1;
	while ( temp[ p1 ] != '.' )
		p1++;
	temp[ p1 ] = '\0';
	b = atoi( &temp[ p2 ] );

	p2 = ++p1;
	while ( temp[ p1 ] != '.' )
		p1++;
	temp[ p1 ] = '\0';
	c = atoi( &temp[ p2 ] );

	p2 = ++p1;
	d = atoi( &temp[ p2 ] );

    // Must be in network order (different than Intel byte ordering)
    LPBYTE  lpByte = (LPBYTE)&dwAddress;

    *lpByte++ = BYTE( a );
    *lpByte++ = BYTE( b );
    *lpByte++ = BYTE( c );
    *lpByte = BYTE( d );

	return TRUE;
}

//********************************************************************************
// getCountryID
//********************************************************************************
static BOOL getCountryID( DWORD dwCountryCode, DWORD& dwCountryID )
{
//	ASSERT( m_lpfnRasGetCountryInfo );
	if ( !m_lpfnRasGetCountryInfo )
	{
		trace( "dialer.cpp : GetCountryID - RasGetCountryinfo func not availble. (r)" );
		return FALSE;
	}
		
	RASCTRYINFO*		pCI = NULL;
	BOOL				bRetval = FALSE;

	DWORD				dwSize = stRASCTRYINFO + 256;
	pCI = (RASCTRYINFO*)malloc( (UINT) dwSize );        // what purpose does malloc serve here?
	
	if ( pCI )
	{
		pCI->dwSize = stRASCTRYINFO;
		pCI->dwCountryID = 1;
      
		while ( ( *m_lpfnRasGetCountryInfo )( pCI, &dwSize ) == 0 )
		{
			if ( pCI->dwCountryCode == dwCountryCode )
			{
				dwCountryID = pCI->dwCountryID;
				bRetval = TRUE;
				break;
			}
			pCI->dwCountryID = pCI->dwNextCountryID;
		}

		free( pCI );
		pCI = NULL;
	}

	return bRetval;
}


//********************************************************************************
// createLink
// creates a shell shortcut to the PIDL
//********************************************************************************
static HRESULT createLink( LPITEMIDLIST pidl, LPCTSTR lpszPathLink, LPCTSTR lpszDesc,
	char* iconPath )
{
    HRESULT			hres;
    IShellLink*		psl = NULL;

    // Get a pointer to the IShellLink interface.
	//CoInitialize(NULL); // work around for Nav thread lock bug

	hres = CoCreateInstance( CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER,
							IID_IShellLink, (LPVOID FAR*)&psl );
    if ( SUCCEEDED( hres ) )
    {
		IPersistFile* ppf;
		// Set the path to the shortcut target, and add the description.
		psl->SetIDList( pidl );
		psl->SetDescription( lpszDesc );
		if( iconPath && iconPath[ 0 ] )
			psl->SetIconLocation( iconPath, 0 );

		// Query IShellLink for the IPersistFile interface for saving the
		// shortcut in persistent storage.
		hres = psl->QueryInterface( IID_IPersistFile, (LPVOID FAR*)&ppf );

		if ( SUCCEEDED( hres ) )
		{
			WORD		wsz[ MAX_PATH ];

			// Ensure that the string is ANSI.
			MultiByteToWideChar( CP_ACP, 0, lpszPathLink, -1, wsz, MAX_PATH );

			// Save the link by calling IPersistFile::Save.
			hres = ppf->Save( (LPCOLESTR)wsz, STGM_READ );
			ppf->Release();
		}

		psl->Release();
	}

	//CoUninitialize();

	return hres;
}

//********************************************************************************
// fileExists
//********************************************************************************
static BOOL fileExists( LPCSTR lpszFileName )
{
	BOOL bResult = FALSE;

	HANDLE hFile=NULL;

	// opens the file for READ
	hFile = CreateFile(lpszFileName, GENERIC_READ, 0, NULL, OPEN_EXISTING,
						FILE_ATTRIBUTE_NORMAL, NULL);

	if (hFile != INVALID_HANDLE_VALUE) {  // openned file is valid
		bResult = TRUE;
		CloseHandle(hFile);
	}
	return bResult;
}


//********************************************************************************
// processScriptLogin
// generate a script file and return the name of the file.  The
//   caller is responsible for freeing the script file name
//********************************************************************************
static BOOL processScriptedLogin( LPSTR lpszBuf, const char* lpszScriptFile )
{
    // validate our args just for fun
    if (!lpszBuf || (lpszBuf[0] == '\0') || !lpszScriptFile)
        return(FALSE);

    // open the actual script file  
    FILE * fp = fopen(lpszScriptFile, "w");
    if (!fp)
        return(FALSE);   

    // generate a prolog
    char timebuf[24];
    char datebuf[24];
    _strtime(timebuf);
    _strdate(datebuf);
    fprintf(fp, "; %s\n; Created: %s at %s\n;\n;\nproc main\n", lpszScriptFile, datebuf, timebuf);

    // Send a return to poke the server.  Is this needed?
    fprintf(fp, "transmit \"^M\"\n"); 

    for (int i = 0; lpszBuf; i++) {
       LPSTR   lpszDelim;

       // Each event consists of two parts:
       //   1. string to wait for
       //   2. string to reply with
       //
       // The string to reply with is optional. A '|' separates the two strings
       // and another '|' separates each event
       lpszDelim = strchr(lpszBuf, '|');  
       if(lpszDelim)
          *lpszDelim = '\0';
   
       // we are in the "wait for event"
       fprintf(fp, "waitfor \"%s\",matchcase until 30\n", lpszBuf); 

       // skip to the next bit
       lpszBuf = lpszDelim ? lpszDelim + 1 : NULL;

       if (lpszBuf) {
          // now look for the reply event
          lpszDelim = strchr(lpszBuf, '|');  
          if(lpszDelim)
          *lpszDelim = '\0';
       
          // we are in the "reply with" event
          // NOTE: we will want to get the ^M value from someone else
          //   since different ISPs will probably want different ones
          if (!stricmp(lpszBuf, "%name"))
             fprintf(fp, "transmit $USERID\n"); 
          else if(!stricmp(lpszBuf, "%password"))
             fprintf(fp, "transmit $PASSWORD\n"); 
          else if(lpszBuf[0])
             fprintf(fp, "transmit \"%s\"\n", lpszBuf); 

          fprintf(fp, "transmit \"^M\"\n"); 
       }

      lpszBuf = lpszDelim ? lpszDelim + 1 : NULL;
    }                                     

   // writeout the ending bits and cleanup
    fprintf(fp, "endproc\n");
    fclose(fp);

    return(TRUE);
}


//********************************************************************************
// createPhoneBookEntry
// Create a dial-up networking profile
//********************************************************************************
static BOOL createPhoneBookEntry( ACCOUNTPARAMS account, const LOCATIONPARAMS& location )
{
	DWORD		dwRet;
	BOOL		ret = FALSE;
	RASENTRY	rasEntry;
	
	// abort if RAS API ptrs are invalid & mem alloc fails
//	ASSERT( m_lpfnRasSetEntryProperties );
//	ASSERT( m_lpfnRasSetEntryDialParams );
		
	if ( !m_lpfnRasSetEntryProperties || 
		 !m_lpfnRasSetEntryDialParams )
		return FALSE;

	stRASENTRY = sizeof(RASENTRY);

	// Initialize the RNA struct
	memset( &rasEntry, 0, stRASENTRY );
	rasEntry.dwSize = stRASENTRY;
	rasEntry.dwfOptions = RASEO_ModemLights | RASEO_RemoteDefaultGateway;

	// Only allow compression if reg server says its OK
		 rasEntry.dwfOptions |= RASEO_IpHeaderCompression | RASEO_SwCompression;

	if ( account.VJCompressionEnabled )
	if ( account.NeedsTTYWindow )
		if ( gPlatformOS == VER_PLATFORM_WIN32_WINDOWS )
			rasEntry.dwfOptions |= RASEO_TerminalBeforeDial;  //win95 bug! RASEO_TerminalBeforeDial means terminal after dial
		else
			rasEntry.dwfOptions |= RASEO_TerminalAfterDial;

	//
	// !!! There was a bug where this use of onlyOneSet wasn't appropriate on Win3.1
	//
	// If Intl Number (not NorthAmerica), or Area Code w/o LDAccess (1) or
	// visa-versa, then abandon using Location - NOTE: for Intl Number we
	// should be able to use location, check it out!
	if ( account.IntlMode || onlyOneSet( location ) )
	{
		char szNumber[ RAS_MaxPhoneNumber + 1 ];
		szNumber[ 0 ] = '\0';

		composeNumber( account, location, szNumber );
		strcpy( rasEntry.szLocalPhoneNumber, szNumber );
	   
		strcpy( rasEntry.szAreaCode, "415" );	// hack around MS bug -- ignored
		rasEntry.dwCountryCode = 1;				// hack around MS bug -- ignored
	}
	else
	{
		// Let Win95 decide to dial the area code or not
		rasEntry.dwfOptions |= RASEO_UseCountryAndAreaCodes;

		trace( "dialer.cpp : Use country and area codes = %d", rasEntry.dwfOptions );

		// Configure the phone number
		parseNumber( account.ISPPhoneNum, &rasEntry.dwCountryCode, 
						rasEntry.szAreaCode, rasEntry.szLocalPhoneNumber );
		  
		if ( !account.IntlMode )
		{
			// if not internationalize version, check the area code and make
			// sure we got a valid area code, if not throw up a err msg
			if ( rasEntry.szAreaCode[ 0 ] == '\0' )
			{
				// Err: The service provider's phone number is missing its area code
				//    (or is not in TAPI cannonical form in the configuration file).
				//    Account creation will fail until this is fixed.
				char	buf[MAX_PATH];
				if ( getMsgString( buf, IDS_MISSING_AREA_CODE ) )
					displayErrMsgWnd( buf, MB_OK | MB_ICONEXCLAMATION, gHwndParent );
				return FALSE;
			}
		}
	}

	// Now that we have the country code, we need to find the associated
	// country ID
	getCountryID( rasEntry.dwCountryCode, rasEntry.dwCountryID );

	// ??? Is this where we can fix the NT problem using no nameservers ???
	//
	// Configure the IP data
	rasEntry.dwfOptions |= RASEO_SpecificNameServers;
	if ( account.DNS[ 0 ] )
		toNumericAddress( account.DNS, *(LPDWORD)&rasEntry.ipaddrDns );

	if ( account.DNS2[ 0 ] )
		toNumericAddress( account.DNS2, *(LPDWORD)&rasEntry.ipaddrDnsAlt );

	// Configure the protocol and device settings here:

	// Negotiate TCP/IP
	rasEntry.dwfNetProtocols = RASNP_Ip;

	// Point-to-Point protocal (PPP)
	rasEntry.dwFramingProtocol = RASFP_Ppp;

	// modem's information
	strcpy( rasEntry.szDeviceName, location.ModemName );
	strcpy( rasEntry.szDeviceType, location.ModemType );

	// If we have a script, then store it too
	if ( account.ScriptEnabled ) 
	{
		BOOL rtnval = TRUE;
		
#if 0
// ??? Why is this disabled ???

		// if there is script content, 'Translate' and store in file 
		if ( ReggieScript )
		{ 
		   	// construct script filename if it does not exists
		 	if ( strlen( account.ScriptFileName ) == 0 )
		   	{
				GetProfileDirectory( account.ScriptFileName );
				int nIndex = strlen( account.ScriptFileName );
				strncat( account.ScriptFileName, account.ISPName, 8 );
				strcat( account.ScriptFileName, ".scp" );
		   	} 
			rtnval = ProcessScriptedLogin( (LPSTR)ReggieScript, account.ScriptFileName );
			free( ReggieScript );
		}


		// if there really is a script file (from ISP or Reggie) then use it
		if ( rtnval && FileExists( account.ScriptFileName ) )
		{
			strcpy( rasEntry.szScript, account.ScriptFileName );

			// convert forward slash to backward slash
			int nLen = strlen( rasEntry.szScript );
			for ( int i = 0; i < nLen; i++ )
				if ( rasEntry.szScript[ i ] == '/' )
					rasEntry.szScript[ i ] = '\\';
		}
#endif
	}

	dwRet = ( *m_lpfnRasValidateEntryName )( NULL, (LPSTR)account.ISPName );
	
	if ( dwRet == ERROR_ALREADY_EXISTS )
	{
		trace ("dialer.cpp : CreateRNAEntry (RasValidateEntryName) - This name already exists. (r)");
		dwRet = 0;
	}
		
//	ASSERT( dwRet == 0 );
	if ( dwRet == ERROR_INVALID_NAME )
	{
		trace ("dialer.cpp : CreateRNAEntry (RasValidateEntryName) - Invalid Name. Can't set RasEntry properties. (r)");
		return FALSE;
	}
	else if ( dwRet )
	{
		trace ("dialer.cpp : CreateRNAEntry (RasValidateEntryName) - Can't Validate account name. Error %d. (r)", dwRet);
		return FALSE;
	}
	
	dwRet = ( *m_lpfnRasSetEntryProperties )( NULL, (LPSTR)(LPCSTR)account.ISPName,
										  (LPBYTE)&rasEntry, stRASENTRY, NULL, 0 );

//	ASSERT( dwRet == 0 );
	if ( dwRet == ERROR_BUFFER_INVALID )
	{
		trace ("dialer.cpp : CreateRNAEntry (RasSetEntryProperties) - Invalid Buffer. Can't set RasEntry properties. (r)");
		return FALSE;
	}
	else if ( dwRet == ERROR_CANNOT_OPEN_PHONEBOOK )
	{
		trace ("dialer.cpp : CreateRNAEntry (RasSetEntryProperties) - Can't open phonebook. Corrupted phonebook or missing components. Can't set RasEntry properties. (r)");
		return FALSE;
	}
	else if ( dwRet )
	{
		trace ("dialer.cpp : CreateRNAEntry (RasSetEntryProperties) - Can't set RasEntry properties. Error %d. (r)", dwRet);
		return FALSE;
	}

	// We need to set the login name and password with a separate call
	// why doesn't this work for winNT40??
	memset( &gDialParams, 0, sizeof( gDialParams ) );
	gDialParams.dwSize = stRASDIALPARAMS;
	strcpy( gDialParams.szEntryName, account.ISPName );
	strcpy( gDialParams.szUserName, account.LoginName );
	strcpy( gDialParams.szPassword, account.Password );

	// Creating connection entry!
   	char*		pbPath = NULL;

	if ( gPlatformOS == VER_PLATFORM_WIN32_NT )
		pbPath = getPhoneBookNT();
	else
		pbPath = strdup((char*)account.ISPName);  // gets free()d later, so malloc now

	//
	//  !!!  N.B.  Do not use the return statement from this point on
	//  !!!        unless you free(pbPath) first!
	//

	dwRet = ( *m_lpfnRasSetEntryDialParams )( pbPath, &gDialParams, FALSE );

	if ( dwRet == ERROR_BUFFER_INVALID )
	{
		trace ("dialer.cpp : CreateRNAEntry (RasSetEntryDialParams) - Invalid Buffer. Can't set RasEntry Dial properties. (r)");
	}
	else if ( dwRet == ERROR_CANNOT_OPEN_PHONEBOOK )
	{
		trace ("dialer.cpp : CreateRNAEntry (RasSetEntryDialParams) - Can't open phonebook. Corrupted phonebook or missing components. Can't set RasEntry Dial properties. (r)");
	}
	else if ( dwRet == ERROR_CANNOT_FIND_PHONEBOOK_ENTRY )
	{
		trace ("dialer.cpp : CreateRNAEntry (RasSetEntryDialParams) - Phonebook entry does not exist. Can't set RasEntry Dial properties. (r)");
	}
	else if ( dwRet )
	{
		trace ("dialer.cpp : CreateRNAEntry (RasSetEntryDialParams) - Can't set RasEntry Dial properties. Error %d. (r)", dwRet);
	}

	ret = ( dwRet == 0 );

	if ( ret==TRUE && gPlatformOS == VER_PLATFORM_WIN32_NT )
	{
		RASCREDENTIALS credentials;

		// sets up user login info for new phonebook entry
		memset( &credentials, 0, sizeof( RASCREDENTIALS ) );
		credentials.dwSize = sizeof( RASCREDENTIALS );
		credentials.dwMask = RASCM_UserName | RASCM_Password;
		strcpy( credentials.szUserName, account.LoginName );
		strcpy( credentials.szPassword, account.Password );
		strcpy( credentials.szDomain, account.DomainName );

		dwRet = ( *m_lpfnRasSetCredentials )( pbPath, (LPSTR)account.ISPName, &credentials, FALSE );

		if ( dwRet == ERROR_INVALID_PARAMETER )
		{
			trace( "dialer.cpp : CreateRNAEntry (RasSetCredentials) - Invalid Parameter. Can't set user credentials. (r)" );
		}
		else if ( dwRet == ERROR_CANNOT_OPEN_PHONEBOOK )
		{
			trace( "dialer.cpp : CreateRNAEntry (RasSetCredentials) - Can't open phonebook. Corrupted phonebook or missing components. Can't set user credentials. (r)" );
		}
		else if ( dwRet == ERROR_CANNOT_FIND_PHONEBOOK_ENTRY )
		{
			trace( "dialer.cpp : CreateRNAEntry (RasSetCredentials) - Phonebook entry does not exist. Can't set user credentials. (r)" );
		}
		else if ( dwRet == ERROR_INVALID_SIZE )
		{
			trace( "dialer.cpp : CreateRNAEntry (RasSetCredentials) - Invalid size value. Can't set user credentials. (r)" );
		}
		else if ( dwRet )
		{
			trace( "dialer.cpp : CreateRNAEntry (RasSetCredentials) - Can't set user credentials. Error %d. (r)", dwRet );
		}

		ret = ( dwRet == 0 );
	}	

	// dialing on demand is cool.  let's do that on win95 now
	if ( ret == TRUE && account.DialOnDemand )
		if ( gPlatformOS == VER_PLATFORM_WIN32_WINDOWS )
			EnableDialOnDemand95( (LPSTR)account.ISPName, TRUE );
		else if ( gPlatformOS == VER_PLATFORM_WIN32_NT )
			EnableDialOnDemandNT( (LPSTR)account.ISPName, TRUE );
	
	if ( pbPath )
		free( pbPath );

	return ret;

}

//
// Called from muc.cpp, but shouldn't be...
//
//********************************************************************************
// DialerConfig
//
// setup and configures the dialer and networking stuff used in 3 conditions:
// 1. when calling regi for a new account
// 2. to configure new account from regi on users system
// 3. when optionally register Navigator in existing account path
//********************************************************************************
//BOOL DialerConfig( char** dialerDataArray )
BOOL DialerConfig( char* charData)
{
	gHwndParent = GetActiveWindow();
	printf ("This is the return data %s \n", charData);

	char* modems;
	modems=GetModemConfig();	
	printf("this is the modem value %s \n",modems);
	printf("this is the full data value %s \n",charData);

	ACCOUNTPARAMS		account;
	LOCATIONPARAMS		location;

    nsString data; data.AssignWithConversion(charData);
    
	// Set the gathered info into an array
	SetDataArray(data);
    
	char* phone = GetValue("phone");
	printf("this is after phone data %s \n",phone);

    char* loginname = GetValue("loginname");
	printf("this is after login name %s \n",loginname);

    char* loginpass = GetValue("loginpass");
	printf("this is the loginpass %s \n",loginpass);

    char* passagain = GetValue("passagain");
	printf("this is the passagain %s \n",passagain);

    char* domainname = GetValue("domainname");
	printf("this is the domainname %s \n",domainname);

	char* dnsp = GetValue("dnsp");
	printf("this is the dnsp %s \n",dnsp);

    char* dnss = GetValue("dnss");
	printf("this is the dnss %s \n",dnss);




	strcpy( account.ISPName,loginname ); 
	
	strcpy( account.DNS,dnsp );

	strcpy( account.ISPPhoneNum, phone);

	strcpy( location.ModemName,modems);

	strcpy( account.DNS2,dnss );
	strcpy( account.DomainName,domainname );
	strcpy( account.Password,loginpass );
	// script file name
/*
	strcpy( account.ISPName,"Samplew" ); 


	strcpy( account.DNS,"208.12.38.38" );


	strcpy( account.ISPPhoneNum, "9613356");


//	strcpy( location.ModemName,"Sportster 28800 External" );
	strcpy( location.ModemName,modems);

	
*/	
	//	strcpy( location->ModemType,"RASDT_Modem" );
	// dial type

	location.DialType = 1;
	// outside line access

	strcpy( location.OutsideLineAccess,"9" );
	// disable call waiting?

	strcpy( location.UserAreaCode,"650" );
	// user country code

	location.UserCountryCode = 1;   // default to US

	location.DisableCallWaiting = 0;
	// disable call waiting code

	strcpy( location.DisableCallWaitingCode,"" );
	// user area code

	location.DialAsLongDistance = 0;	
	// long distance access

	strcpy( location.LongDistanceAccess, "" );
	// dial area code?

//	location.DialAreaCode = 1;	
	// dial prefix code
//	if ( !dialerDataArray )
//		return FALSE;

//	MessageBox(NULL, "Inside DialerConfig", "NSCP", MB_OK);

	// now we try to get values from the JS array and put them into corresponding 
	// account and location parameters
//	fillAccountParameters( dialerDataArray, &account );
//	fillLocationParameters( dialerDataArray, &location );
//	fillAccountParameters(  &account );
//	fillLocationParameters(  &location );
	
	// configure & creating Dial-Up Networking profile here for Win95 & WinNT40
	// win16 use Shiva's RAS 
	if ( !( createPhoneBookEntry( account, location ) ) )
	{
	
		// Err: Unable to crate RNA phone book entry!
		char	buf[MAX_PATH];
		if ( getMsgString( buf, IDS_NO_RNA_REGSERVER ) )
		displayErrMsgWnd( buf, MB_OK | MB_ICONEXCLAMATION, gHwndParent );
	}
	printf ("Connectoid has been created \n");
#if 0
// This was active in the 4.x code base, why is it not active here?

	int	ret;
	if ( gPlatformOS == VER_PLATFORM_WIN32_WINDOWS )
	{
		// sets the location stuff
		ret = SetLocationInfo( account, location );
		//ret = DisplayDialableNumber();
	}
	else
	{
		ret = SetLocationInfoNT( account, location );
	}
#endif

	return TRUE;
}


//********************************************************************************
// IsDialerConnected
// checks if the dialer is still connected
//
// returns:
//	TRUE if the dialer is connected
//	FALSE if the dialer is not connected or there is an error trying to
//		determine if the dialer is connected
//********************************************************************************
BOOL IsDialerConnected()
{
	RASCONN*		info = NULL;
	RASCONN*		lpTemp = NULL;
	DWORD			code = 0;
	DWORD			count = 0;
	DWORD			dSize = stRASCONN;
    BOOL			triedOnce = FALSE;
	
tryAgain:
	// try to get a buffer to receive the connection data
	if ( ! ( info = (RASCONN*)LocalAlloc( LPTR, dSize ) ) )
		return FALSE;
	
	// see if there are any open connections
	info->dwSize = dSize;
	code = (*m_lpfnRasEnumConnections)( info, &dSize, &count );
	LocalFree( info );
	
	if ( code != 0 && triedOnce == TRUE )
		return FALSE;
	
	if ( code == ERROR_BUFFER_TOO_SMALL )
	{
		triedOnce = TRUE;
		goto tryAgain;
	}
	
	return ( 0 != count );
}



//********************************************************************************
// CheckDNS
// for Win95
// If user has DNS enabled, when setting up an account, we need to warn them
// that there might be problems.
//********************************************************************************
void CheckDNS()
{
    char		buf[ 256 ];
    HKEY		hKey;
    DWORD		cbData;
    LONG		res;

	// open the key if registry
	if( ERROR_SUCCESS != RegOpenKeyEx( HKEY_LOCAL_MACHINE, 
	             "System\\CurrentControlSet\\Services\\VxD\\MSTCP", 
	             NULL, 
	             KEY_ALL_ACCESS   , 
	             &hKey ) )
		return;
	
	cbData = sizeof( buf );
	res = RegQueryValueEx( hKey, "EnableDNS", NULL, NULL, (LPBYTE)buf, &cbData );
	
	RegCloseKey( hKey );
	
	// if DNS is enabled we need to warn the user
	if( buf[ 0 ] == '1' )
	{
		BOOL		correctWinsockVersion = FALSE;
	
		// check for user's winsock version first, see if it's winsock2
		WORD		wVersionRequested;  
		WSADATA		wsaData; 
		int			err; 
	
		wVersionRequested = MAKEWORD( 2, 2 ); 
	
		err = WSAStartup( wVersionRequested, &wsaData ); 
	
		if ( err != 0 )
		{
			// user doesn't have winsock2, so check their winsock's date
			char		winDir[ MAX_PATH ];
			UINT		uSize = MAX_PATH;
			char		winsockFile[ MAX_PATH ];
		
			winDir[ 0 ] = '\0';
			winsockFile[ 0 ] = '\0';
			GetWindowsDirectory( (char*)&winDir, uSize );
			strcpy( winsockFile, winDir );
			strcat( winsockFile, "\\winsock.dll" );
		
			HANDLE hfile = CreateFile( (char*)&winsockFile, GENERIC_READ, 0, NULL,
								OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL );
		
			if ( hfile != INVALID_HANDLE_VALUE )
			{
				// openned file is valid
		
				FILETIME		lastWriteTime;
				BOOL rtnval = GetFileTime( hfile, NULL, NULL, &lastWriteTime );
		
				SYSTEMTIME		systemTime;
				rtnval = FileTimeToSystemTime( &lastWriteTime, &systemTime );
	
				// perfect example of why Windows software is so awful	
				if ( ( systemTime.wYear >= 1996 ) && ( systemTime.wMonth >= 8 ) &&
						( systemTime.wDay >= 24 ) )
					correctWinsockVersion = TRUE;
		
				CloseHandle( hfile );
			}
		}
	
		else
			correctWinsockVersion = TRUE;
	
	
		if ( !correctWinsockVersion )
		{
			// Err: Your system is configured for another Domain Name System (DNS) server.
			//    You might need to edit your DNS configuration.  Check the User's Guide
			//    for more information.
			char	buf[ 255 ];
			if ( getMsgString( buf, IDS_DNS_ALREADY ) ) 
				displayErrMsgWnd( buf, MB_OK | MB_ICONASTERISK, NULL );
		}
	}
	
	return;
}



//********************************************************************************
// CheckDUN
// for Win95
// If user doesn't have Dial-Up Networking installed, they will have problem
// setting up an account.
//********************************************************************************
BOOL CheckDUN()
{
	static const char c_szRNA[] = "rundll.exe setupx.dll,InstallHinfSection RNA 0 rna.inf";
	
	BOOL		bDoInstall = FALSE;
	HKEY		hKey;
	LONG		res;
	char		szBuf[ MAX_PATH ];
	
	// Let's see if its installed
	if( ERROR_SUCCESS != RegOpenKeyEx( HKEY_LOCAL_MACHINE, 
	                 "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Setup\\OptionalComponents\\RNA", 
	                 NULL, 
	                 KEY_QUERY_VALUE, 
	                 &hKey ) )
		bDoInstall = TRUE;
	
	// the key is there, was it actually installed though...
	if( !bDoInstall )
	{
		DWORD cbData = sizeof( szBuf );
		res = RegQueryValueEx( hKey, "Installed", NULL, NULL, (LPBYTE)szBuf, &cbData );

		// Seems like we should do the install if this value isn't found, added success check...
		if ( ERROR_SUCCESS != res || szBuf[ 0 ] != '1' && szBuf[ 0 ] != 'Y' && szBuf[ 0 ] != 'y' )
			bDoInstall = TRUE;
	}
	
	// Release this resource before we lose track of it.
	RegCloseKey( hKey );

	// make sure a random file from the installation exists so that we
	//   know the user actually installed instead of just skipping over
	//   the install step
	if( !bDoInstall )
	{
		if( GetSystemDirectory( szBuf, sizeof( szBuf ) ) )
		{
			// create a name of one of the files
			strcat( szBuf, "\\pppmac.vxd" ); 				// ??? Is this file adequate?
	
			// let's see if that file exists
			struct _stat stat_struct;
			if( _stat( szBuf, &stat_struct ) != 0 )
				bDoInstall = TRUE;
	
		}
		else
			// !!! Until the GetSystemDirectory call is made robust, assume the worst...  !!!
			bDoInstall = TRUE;
	}
	
	// if no Dial-Up Networking installed install it now
	if ( bDoInstall )
	{
		// let the user not install it or exit
		//
		// Err: Dial-Up Networking has not been installed on this machine;
		//    this product will not work until Dial-Up Networking is installed.  
		//    Would you like to install Dial-Up Networking now?
	
		char	buf[MAX_PATH];
		if ( !getMsgString( buf, IDS_NO_DUN ) )
			strcpy(buf, "Would you like to install Dial-Up Networking now?");

		if ( IDOK != displayErrMsgWnd( buf, MB_OKCANCEL | MB_ICONASTERISK, NULL ) )
			return FALSE;
	
		// install Dial-Up Networking
		PROCESS_INFORMATION pi;
		STARTUPINFO         sti;
	
		memset( &sti, 0, sizeof( sti ) );
		sti.cb = sizeof( STARTUPINFO );
	
		// Run the setup application
		if ( CreateProcess( NULL, (LPSTR)c_szRNA, 
							NULL, NULL, FALSE, 0, NULL, NULL, &sti, &pi ) )
		{
			CloseHandle( pi.hThread );
	
			// Wait for the modem wizard process to complete
			WaitForSingleObject( pi.hProcess, INFINITE );

			// Get the modem wizard exit code
			DWORD exitcode;
			UINT  err = STILL_ACTIVE;
			while (err == STILL_ACTIVE) 
				err = GetExitCodeProcess(pi.hProcess, &exitcode);

			// Be sure to return this resource before losing it
			CloseHandle( pi.hProcess );

			MessageBox(NULL, "Check to see that modem wizard exits with zero on success", "NSCP", MB_ICONSTOP);
			if (ERROR_SUCCESS != err || exitcode != 0)  // Is zero the right thing to test for?
				return FALSE;
		}
		else
			return FALSE;		// Caller will put up a dialog announcing failure
	
	}
	
	return TRUE;
}


//********************************************************************************
// check if a user is an Administrator on NT40
//********************************************************************************
static BOOL isWinNTAdmin()
{
	HANDLE			hAccessToken;
    UCHAR			infoBuffer[ 1024 ];
    PTOKEN_GROUPS	ptgGroups = (PTOKEN_GROUPS)infoBuffer;
    DWORD			dwInfoBufferSize;
    PSID			psidAdministrators;
    SID_IDENTIFIER_AUTHORITY siaNtAuthority = SECURITY_NT_AUTHORITY;
    UINT			x;
    BOOL			bSuccess;

	if ( !OpenProcessToken( GetCurrentProcess(), TOKEN_READ, &hAccessToken ) )
		return FALSE;
 
	bSuccess = GetTokenInformation( hAccessToken, TokenGroups, infoBuffer,
									1024, &dwInfoBufferSize );

	CloseHandle( hAccessToken );

	if( !bSuccess )
		return FALSE;
 
	if( !AllocateAndInitializeSid( &siaNtAuthority, 2,
		SECURITY_BUILTIN_DOMAIN_RID,
		DOMAIN_ALIAS_RID_ADMINS,
		0, 0, 0, 0, 0, 0,
		&psidAdministrators ) )
		return FALSE;
 
   // assume that we don't find the admin SID.
	bSuccess = FALSE;
 
	for ( x = 0; x < ptgGroups->GroupCount; x++ )
	{
		if ( EqualSid( psidAdministrators, ptgGroups->Groups[ x ].Sid ) )
		{
			bSuccess = TRUE;
			break;
         }
	}

	FreeSid( &psidAdministrators );
	return bSuccess;
}



//********************************************************************************
// CheckDUN_NT
// for WinNT40
// If user doesn't have Dial-Up Networking installed, they will have problem
// setting up an account.
//********************************************************************************
BOOL CheckDUN_NT()
{
	BOOL		bDoInstall = FALSE;
	BOOL		bAdmin = FALSE;
	HKEY		hKey;
	LONG		res;
	char		szBuf[ MAX_PATH ];
	char*		buf = NULL;
	
	
	// Let's see if its installed
	// "HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Services\\RemoteAccess"
	if ( ERROR_SUCCESS != RegOpenKeyEx( HKEY_LOCAL_MACHINE,
						"SYSTEM\\CurrentControlSet\\Services\\RemoteAccess",
						NULL, 
						KEY_QUERY_VALUE,
						&hKey ) )
		bDoInstall = TRUE;
	
	// the key is there, was it actually installed though...
	// look for some RAS keys
	szBuf[ 0 ] = '\0';
	if ( !bDoInstall )
	{
		DWORD cbData = sizeof( szBuf );
		res = RegQueryValueEx( hKey, "DisplayName", NULL, NULL, (LPBYTE)szBuf, &cbData );
		if( strlen( szBuf ) == 0 )
			bDoInstall = TRUE;
	
		RegCloseKey( hKey );
	
		// how about autodial manager....
		// "HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Services\\RasAuto"
		if ( ERROR_SUCCESS != RegOpenKeyEx( HKEY_LOCAL_MACHINE,
						"SYSTEM\\CurrentControlSet\\Services\\RasAuto",
						NULL,
						KEY_QUERY_VALUE,
						&hKey ) )
			bDoInstall = TRUE;
	
		RegCloseKey( hKey );
	}
	
	
	// if no Dial-Up Networking installed, warn the users depending on
	// their premissions and return FALSE
	if ( bDoInstall )
	{
		bAdmin = isWinNTAdmin();
	
		if ( bAdmin )
		{
			// Err: Dial-Up Networking has not been installed on this machine;
			//    this product will not work until Dial-Up Networking is installed.  
			//    Please install Dial-Up Networking before running Accout Setup.
			char buf[ 255 ];
	
			if ( !getMsgString( buf, IDS_NO_DUN_NT ) )
				strcpy(buf, "Please install Dial-Up Networking before running Accout Setup.");
			displayErrMsgWnd( buf, MB_OK | MB_ICONASTERISK, NULL );
	
			// Return false since we can't install RAS from here
			return FALSE;

// This is done by the helper.dll
// ??? Is it hard to do the same thing here ???
#if 0
			// install Dial-Up Networking
			PROCESS_INFORMATION pi;
			STARTUPINFO         sti;
			UINT                err = ERROR_SUCCESS;
			char				RASphone[ MAX_PATH ];
			
			// Fix this if it's ever activated
			GetSystemDirectory( RASphone, MAX_PATH );
			strcat( RASphone, "\\rasphone.exe" );

			memset( &sti, 0, sizeof( sti ) );
			sti.cb = sizeof( STARTUPINFO );
			
			// Run the setup application
			if ( CreateProcess( (LPCTSTR)&RASphone, NULL, 
				NULL, NULL, FALSE, 0, NULL, NULL, &sti, &pi ) )
			{
				CloseHandle( pi.hThread );

				// Wait for the Dial-Up Networking install process to complete
				WaitForSingleObject( pi.hProcess, INFINITE );
				CloseHandle( pi.hProcess );
			}
#endif
	
		}
		else
		{
			// user need to have administrator premission to install, and ASW won't
			// work if DUN is not installed
			// Err: You do not have Administrator premission on this machine to intall
			//		Dial-Up Networking. Please make sure you have Administrator premission
			//		in order to install Dial-Up Networking first before running Account Setup.
	
			char		buf[ 255 ];
	
			if ( !getMsgString( buf, IDS_NO_ADMIN_PREMISSION ) )
				strcpy(buf, "Please install Dial-Up Networking before running Accout Setup.");
			displayErrMsgWnd( buf, MB_OK | MB_ICONASTERISK, NULL );

			// Return false since we can't install RAS from here
			return FALSE;
		}
	}

	// Never reached
	return TRUE;
}

//********************************************************************************
// CreateDialerShortcut
// Creates a shell shortcut to the PIDL
//********************************************************************************
static short createDialerShortcut( char* szDesktop,     // Desktop path
	LPCSTR 			accountName,  // connectoid/phonebook entry name
	IMalloc*		pMalloc,
	char*			szPath,     // path to PE folder
	char*			strDesc,
	char*			iconPath )      // shortcut description
{
	HRESULT				hres;
	LPITEMIDLIST		pidl;

	char				desktop[ MAX_PATH ];
	DWORD				cbData;
	HKEY				hKey;
	long				res;

	szDesktop[ 0 ] = '\0';

	// gets Desktop folder path from registry for both win95 & winNT40
	// "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Shell Folders"
    if ( ERROR_SUCCESS != RegOpenKeyEx( HKEY_CURRENT_USER,
    		REGSTR_PATH_SPECIAL_FOLDERS,
    		NULL,
    		KEY_QUERY_VALUE,
    		&hKey ) )
		return -3;		// huh?

    cbData = MAX_PATH;
    res = RegQueryValueEx( hKey, "Desktop", NULL, NULL, (LPBYTE)desktop, &cbData );

	RegCloseKey( hKey );

	strcpy( szDesktop, desktop );

	// win95 only
	if ( gPlatformOS == VER_PLATFORM_WIN32_WINDOWS )
	{
		// Get a PIDL that points to the dial-up connection
		hres = getDialUpConnectionPIDL( accountName, &pidl );

		if ( FAILED( hres ) )
		{
			// Err: Unable to create shortcut to RNA phone book entry
			char	buf[MAX_PATH];
			if ( getMsgString( buf, IDS_NO_RNA ) )
			{
				if ( IDOK != displayErrMsgWnd( buf, MB_OK | MB_ICONEXCLAMATION, NULL ) )
					return FALSE;
			}

			return -1;
		}

		// If the dial-up networking folder is open when we create the RNA
		// entry, then we will have failed to get a PIDL above. The dial-up
		// networking folder itself won't display the icon until it is closed
		// and re-opened. There's nothing we can do but return
		//
		// !!! The body of this if stmt had been doing a pMalloc->Release() call,
		// !!! but it isn't the owner of that resource.  I've removed it so the
		// !!! resource doesn't get released twice.
		//
		if ( !pidl )
			return -2;		// huh?

		// Create a shortcut on the desktop
		char strPath[ MAX_PATH ];
		strcpy( strPath, szDesktop );
		strcat( strPath, "\\" );
		strcat( strPath, strDesc );
		strcat( strPath, ".Lnk" );
		createLink( pidl, strPath, strDesc, iconPath );
		
		// And one in our PE folder
		strcpy( strPath, szPath );
		strcat( strPath, "\\" );
		strcat( strPath, strDesc );
		strcat( strPath, ".Lnk" );
		createLink( pidl, strPath, strDesc, iconPath );		

		// release the pidl here!
		pMalloc->Free( pidl );
	}
	else if ( gPlatformOS == VER_PLATFORM_WIN32_NT )
	{ 
		// WinNT40 here
#if 0
// Using getPhoneBookNT function instead of this if it works the same

		char sysDir [MAX_PATH];
		char pbPath[ MAX_PATH ];
		GetSystemDirectory( sysDir, MAX_PATH );
#endif
		// make sure the phonebook entry we created still exists
		char *pbPath = getPhoneBookNT();
		strcat( pbPath, "\\ras\\rasphone.pbk" );
		strcat( pbPath, "\0" );
		
		// ??? Why is this code so different from GetDialupConnectionNT ???
		RASENTRYNAME rasEntryName[ MAX_ENTRIES ];

		rasEntryName[ 0 ].dwSize = stRASENTRYNAME;
		DWORD size = stRASENTRYNAME * MAX_PATH;
		DWORD entries;
		
		if ( 0 != (*m_lpfnRasEnumEntries)( NULL, pbPath, rasEntryName, &size, &entries ) )
			return -4;

		BOOL		exists = FALSE;
		DWORD		i = 0;
		
		while ( ( i < entries) && ( !exists ) )
		{
			if ( strcmp( rasEntryName[ i ].szEntryName, accountName ) == 0 )
				exists = TRUE;
			i++;
		}

		// create a shortcut file on desktop
		if ( exists )
		{
			HANDLE hfile = NULL;
			
			// create phonebook entry shortcut file on desktop, overwrites if exists
			SECURITY_ATTRIBUTES		secAttrib;
			memset( &secAttrib, 0, sizeof( SECURITY_ATTRIBUTES ) );
			secAttrib.nLength = sizeof( SECURITY_ATTRIBUTES );
			secAttrib.lpSecurityDescriptor = NULL;
			secAttrib.bInheritHandle = FALSE;

			// construct phonebook entry shortcut file name
			char file[ MAX_PATH ];
			strcpy( file, szDesktop );
			strcat( file, "\\" );
			strcat( file, accountName );
			strcat( file, ".rnk" );
			
			hfile = CreateFile( file, GENERIC_READ|GENERIC_WRITE, FILE_SHARE_READ|FILE_SHARE_WRITE,
				               &secAttrib, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL );

			if ( hfile == INVALID_HANDLE_VALUE )
				return -9;	// huh? 

			CloseHandle( hfile );
			hfile = NULL;

			// writes shortcut file data in the following format:
			//    [Dial-Up Shortcut]
			//    Entry=stuff
			//    Phonebook=C:\WINNT40\System32\RAS\rasphone.pbk (default system phonebook)

			WritePrivateProfileString( "Dial-Up Shortcut", "Entry", accountName, file );
			WritePrivateProfileString( "Dial-Up Shortcut", "Phonebook", pbPath, file );

			// create the same shortcut file in our PE folder
			strcpy( file, szPath );
			strcat( file, "\\" );
			strcat( file, accountName );
			strcat( file, ".rnk" );

			hfile = CreateFile( file, GENERIC_READ|GENERIC_WRITE, FILE_SHARE_READ|FILE_SHARE_WRITE,
				               &secAttrib, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL );

			if ( hfile == INVALID_HANDLE_VALUE ) 
				return -10;

			CloseHandle( hfile );
			WritePrivateProfileString( "Dial-Up Shortcut", "Entry", accountName, file );
			WritePrivateProfileString( "Dial-Up Shortcut", "Phonebook", pbPath, file );

		}
		else
			return -8;	// huh?
	}

	return 0;
}


//********************************************************************************
// createProgramItems
// adds 2 icons:
// Dialer - to Dial-Up Networking folder, Desktop & our PE folder
// Navigator - to our PE folder
//********************************************************************************
static short createProgramItems( LPCSTR accountName, LPCSTR iniPath, char* iconFile )
{
	char         szPath[ MAX_PATH ];
	char         szBuf[ MAX_PATH ];
	
	IMalloc*     pMalloc;

	SHGetMalloc( &pMalloc );
	
	// gets the path to "Programs" folder
	if ( gPlatformOS == VER_PLATFORM_WIN32_WINDOWS )
	{
		LPITEMIDLIST pidl;
	
		SHGetSpecialFolderLocation( NULL, CSIDL_PROGRAMS, &pidl );
		SHGetPathFromIDList( pidl, szBuf );
		pMalloc->Free( pidl );
	}
	else if ( gPlatformOS == VER_PLATFORM_WIN32_NT )
	{
		// NT4.0: get the "Programs" folder for "All Users"
		HKEY	hKey;
		DWORD	bufsize = sizeof( szBuf );
	
		if( ERROR_SUCCESS == RegOpenKeyEx( HKEY_LOCAL_MACHINE, 
						 "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", 
						 NULL, 
						 KEY_QUERY_VALUE,  
						 &hKey ) )
		{
			RegQueryValueEx( hKey, "PathName", NULL, NULL, (LPBYTE)szBuf, &bufsize );
			RegCloseKey( hKey );
			strcat( szBuf, "\\Profiles\\All Users\\Start Menu\\Programs" );
		}
		else
			return -1;
	}
	
	// gets Netscape PE folder here
	char		buf[MAX_PATH];
	char		*csFolderName = "Netscape Personal Edition";
	
	// check for custom folder name
	DWORD rslt = ::GetPrivateProfileString( "General", "InstallFolder",
				(const char*)csFolderName, buf, sizeof( buf ), iniPath );

	strcpy( szPath, szBuf );
	strcat( szPath, "\\" );
	if (rslt > 0)
		strcat( szPath, buf );
	else
		strcat( szPath, csFolderName );
	
	// First do Dialer Icon
	// Create a dialer icon shortcut description
	char strDesc[ MAX_PATH ];
	
#ifdef UPGRADE
	if ( ???entryInfo.bUpgrading )
	{
		char*	csTmp = "Dialer";
		strcpy( strDesc, accountName );
		strcat( strDesc, " " );
		strcat( strDesc, csTmp );
	}
	else
	{
	strcpy( strDesc, "Dial" );
	strcat( strDesc, " " );
	strcat( strDesc, accountName );
	}
#else
	strcpy( strDesc, "Dial" );
	strDesc[ strlen( strDesc ) + 1 ] = '\0';
	strDesc[ strlen( strDesc ) ] = (char)32;
	strcat( strDesc, accountName );
#endif
	
	char		szDesktop[ 512 ];  // ??? Why is this retrieved from createDialerShortcut and never used ???
	
	// create dialer shortcut icon on desktop and in PE folder
	int rtn = createDialerShortcut( szDesktop, accountName, pMalloc, szPath, strDesc, iconFile );
	
	if ( rtn != 0 )
		return rtn;
	
	pMalloc->Release();
	
	// Why?
	::Sleep( 250 );
	
	return 0;
}


//********************************************************************************
// DesktopConfig
// Sets up user's desktop (creates icons and short cuts)
//********************************************************************************
void DesktopConfig( char* accountName, char* iconFileName,
	char* acctsetIniPath)
{
	static char lastIconFile[ _MAX_PATH ] = { '\0' };

	if ( iconFileName != NULL )
	{
		// JS may pass us different file for icon file
		if ( strcmp( iconFileName, lastIconFile ) != 0 )
		{
			if ( strcmp( lastIconFile, "" ) != 0 )
			{
				if ( fileExists( lastIconFile ) ) 	// a temp icon file may already existed
					_unlink( lastIconFile );
			}
			
			// check if icon file exists
			if ( fileExists( iconFileName ) )
				strcpy( lastIconFile, iconFileName );
			else
				lastIconFile[0] = '\0';
		}
	}
	
	/////////////////////////////////////////////////////////////////////////////////////
	//
	// !!! Depends on how the app is put together, this may not belong here !!!
	//
	// remove the RegiServer RAS
	char	regiRAS[ 50 ];
	getMsgString( (char*)regiRAS, IDS_REGGIE_PROGITEM_NAME );
	(*m_lpfnRasDeleteEntry)( NULL, (LPSTR)regiRAS );
	/////////////////////////////////////////////////////////////////////////////////////

	// creates prgram icons in folders and desktop
	int ret = createProgramItems( accountName, acctsetIniPath, lastIconFile );
}


//********************************************************************************
// QuitNavigator
// quits the navigator
//********************************************************************************
void QuitNavigator()
{
	MessageBox(NULL, "QuitNavigator probably isn't needed anymore and also probably doesn't work this way.", "NSCP", MB_ICONSTOP);

	// Bug#112622 - don't broadcast this, java catches it and dies.
	// Instead, find the hidden window and tell _it_ what to do.

	// The strings in this call are hard-coded because the constants needed
	// do not exist and multiple places are doing similar things.  The 
	// values themselves come from winfe's Netscape.cpp.
	HWND hWnd = FindWindowEx( NULL, NULL, "aHiddenFrameClass", "Netscape's Hidden Frame" );
	PostMessage(/*HWND_BROADCAST*/ hWnd, WM_COMMAND, ID_APP_SUPER_EXIT, 0L );
}

//********************************************************************************
// getCurrentProfileDirectory
// gets the current Navigator user profile directory 
//********************************************************************************
/*extern char* GetCurrentProfileDirectory()
{
	char*		profilePath = NULL;
	char		buf[ MAX_PATH ];
	buf[ 0 ] = '\0';

	int	bufSize = sizeof(buf);
	if ( PREF_OK == PREF_GetCharPref( "profile.directory", buf, &bufSize ) )
	{
		// make sure we append the last '\' in the profile dir path
		strcat( buf, "\\" );	
		trace( "profile.cpp : GetCurrentProfileDirectory : Got the Current User profile = %s", buf );
	}
	else
		trace( "profile.cpp : GetCurrentProfileDirectory : Error in obtaining Current User profile" );

	profilePath = strdup( buf );

	return profilePath;
}
	

//********************************************************************************
// GetCurrentProfileName
// gets the current Navigator user profile name 
//********************************************************************************
extern char* GetCurrentProfileName()
{
	char*			profileName = NULL;
	int				bufSize = 256;
	char			buf[ 256 ];
	buf[ 0 ] = '\0';

	if ( PREF_OK == PREF_GetCharPref( "profile.name", buf, &bufSize ) )
		trace( "profile.cpp : GetCurrentProfileDirectory : Got the Current profile name = %s", buf );
	else
		trace( "profile.cpp : GetCurrentProfileName : Error in obtaining Current User profile" );

	profileName = strdup( buf );

	return profileName;
}


//********************************************************************************
// SetCurrentProfileName
// changes the current Navigator user profile name to a different name
//********************************************************************************
void SetCurrentProfileName( char* profileName )
{
	assert( profileName );

	if ( NULL == profileName )	// abort if string allocation fails
		return;
	
	if ( PREF_ERROR == PREF_SetDefaultCharPref( "profile.name", profileName ) )
		trace( "profile.cpp : SetCurrentProfileName : Error in setting Current User profile name." );
	else
	{
		trace( "profile.cpp : SetCurrentProfileName : Current User profile is set to = %s", profileName );

		if ( PREF_ERROR == PREF_SetDefaultBoolPref( "profile.temporary", FALSE ) ) 
			trace( "profile.cpp : SetCurrentProfileName : Error in setting Temporary flag to false." );
		else
			trace( "profile.cpp : SetCurrentProfileName : Made the profile to be permanent." );
	}

}
*/
// The format of the entry is
//  LocationX=Y,"name","outside-line-local","outside-line-long-D","area-code",1,0,0,1,"",tone=0,"call-waiting-string"
typedef struct tapiLineStruct
{
	int		nIndex;
	char		csLocationName[ 60 ];
	char		csOutsideLocal[ 20 ];
	char		csOutsideLongDistance[ 20 ];
	char		csAreaCode[ 20 ];
	int		nCountryCode;
	int		nCreditCard;
	int		nDunnoB;
	int		nDunnoC;
	char		csDialAsLongDistance[ 10 ];
	int		nPulseDialing;
	char		csCallWaiting[ 20 ];
} TAPILINE;

//********************************************************************************
// readNextInt
// Pull an int off the front of the string and return the new string ptr
//********************************************************************************
char* readNextInt( char* pInString, int* pInt )
{
	char*		pStr;
	char		buf[ 32 ];
	char*		retval;
	
	if ( !pInString )
	{
		*pInt = 0;
		return NULL;
	}
	
	// copy the string over.  This strchr should be smarter    
	pStr = strchr( pInString, ',' );

#if 0
// This old handling wouldn't work if called on last element in the string
	if ( !pStr )
	{
		*pInt = 0;
		return NULL;
	}
#else
	// Allow for using whole string if no comma was found
	if ( !pStr )
	{
		pStr = pInString + strlen(pInString);
		retval = NULL;
	}
	else
		retval = pStr + 1;
#endif
	
	// convert the string            
	strncpy( buf, pInString, pStr - pInString );
	buf[ pStr - pInString ] = '\0';
	*pInt = atoi( buf );
	
	// return the part after the int (if any)
	return retval;	
}

//********************************************************************************
// readNextString
// Pull a string from the front of the incoming string and return
//   the first character after the string
//********************************************************************************
char* readNextString( char* pInString, char* csString )
{
	csString[ 0 ] = '\0';
	int		i = 0, x = 0;
	BOOL	copy = FALSE;
	
	if ( !pInString )
	{
		csString = "";
		return NULL;
	}
	
	// skip over first quote
	if ( pInString[ i ] == '\"' )
		i++;
	
	// copy over stuff by hand line a moron
	while ( pInString[ i ] != '\"' )
	{
		//strcat(csString, (char *)pInString[i]);
		csString[ x ] = pInString[ i ];
		i++;
		x++;
		copy = TRUE;
	}
	
	if ( copy )
		csString[ x ] = '\0';
	
	
	// skip over the closing quote
	if( pInString[ i ] == '\"' )
		i++;
	
	// skip over the comma at the end
	if( pInString[ i ] == ',' )
		i++;

#if 0
// Pardon me, but this looks really stupid.

	char	newpInString[ MAX_PATH ];
	newpInString[ 0 ]='\0';
	x = 0;

	for ( unsigned short j = i; j < strlen( pInString ); j++ )
	{
		//strcat(newpInString, (char *)pInString[j]);
		newpInString[ x ] = pInString[ j ];
		x++;
	}
	
	newpInString[ x ]='\0';
	strcpy( pInString, newpInString );

	return pInString;
#else
	return (pInString + i);
#endif
	
}

//********************************************************************************
// readTapiLine
// Read a line out of the telephon.ini file and fill the stuff in
//********************************************************************************
void readTapiLine( char* lpszFile, int nLineNumber, TAPILINE* line ) 
{
	char		buf[ 256 ];
	char		pLocation[ 128 ];
	sprintf( pLocation, "Location%d", nLineNumber );
	::GetPrivateProfileString( "Locations", pLocation, "", buf, sizeof( buf ), lpszFile );
	
	char*		pString = buf;
	pString = readNextInt( pString, &line->nIndex );
	pString = readNextString( pString, (char *)&line->csLocationName );
	pString = readNextString( pString, (char *)&line->csOutsideLocal );
	pString = readNextString( pString, (char *)&line->csOutsideLongDistance );
	pString = readNextString( pString, (char *)&line->csAreaCode );
	pString = readNextInt( pString, &line->nCountryCode );
	pString = readNextInt( pString, &line->nCreditCard );
	pString = readNextInt( pString, &line->nDunnoB );
	pString = readNextInt( pString, &line->nDunnoC );
	pString = readNextString( pString, (char *)&line->csDialAsLongDistance );
	pString = readNextInt( pString, &line->nPulseDialing );
	pString = readNextString( pString, (char *)&line->csCallWaiting );	
}



//********************************************************************************
// writeTapiLine
// Given a tapiLine structure write it out to telephon.ini
//********************************************************************************
void writeTapiLine( char* lpszFile, int nLineNumber, TAPILINE* line ) 
{
    char		buffer[ 256 ];
    sprintf( buffer, "%d,\"%s\",\"%s\",\"%s\",\"%s\",%d,%d,%d,%d,\"%s\",%d,\"%s\"",
        line->nIndex,
        (const char*) line->csLocationName,
        (const char*) line->csOutsideLocal,
        (const char*) line->csOutsideLongDistance,
        (const char*) line->csAreaCode,
        line->nCountryCode,
        line->nCreditCard,
        line->nDunnoB,
        line->nDunnoC,
        (const char*) line->csDialAsLongDistance,
        line->nPulseDialing,
        (const char*) line->csCallWaiting );

    char			pLocation[ 32 ];
    sprintf( pLocation, "Location%d", nLineNumber );
    ::WritePrivateProfileString( "Locations", pLocation, buffer, lpszFile );
}

//********************************************************************************
// SetLocationInfo
// sets the location info for win95 dialers
// The format of the entry is
//  LocationX=Y,"name","outside-line-local","outside-line-long-D","area-code",1,0,0,1,"",tone=0,"call-waiting-string"
//********************************************************************************
BOOL SetLocationInfo( ACCOUNTPARAMS account, LOCATIONPARAMS location )
{
	// first read information from telephon.ini
	char		buf[ MAX_PATH ];
	buf[ 0 ] = '\0';
	TAPILINE		line;
	int nId, nCount;
	BOOL found;
	
	// get windows directory
	char		lpszDir[ MAX_PATH ];
	if ( GetWindowsDirectory( lpszDir, sizeof(lpszDir) ) == 0 )
		return FALSE;
	
	strcat( lpszDir, "\\telephon.ini" );
	
	// now we build new line information based on the old one and some info
	// see if there were any locations to begin with
	::GetPrivateProfileString( "Locations", "CurrentLocation", "", buf, sizeof( buf ), lpszDir );
	if( buf[ 0 ] == '\0' )
	{
		// Create a new entry
		found = FALSE;
		nId = 0;

		// build the string
		line.nIndex = 0;
		strcpy( line.csLocationName, "Default Location" );
		line.nCountryCode = location.UserCountryCode;
		line.nCreditCard = 0;
		line.nDunnoB = 0;
		line.nDunnoC = 1;
	
	}
	else
	{
		// Overwrite an existing entry
		found = TRUE;
		sscanf( buf, "%d,%d", &nId, &nCount );
	
		// read the line
		TAPILINE		line;                                
		readTapiLine( lpszDir, nId, &line );
	
	}

	// Set the stuff common to create and overwrite
	strcpy( line.csOutsideLocal, location.OutsideLineAccess );
	strcpy( line.csOutsideLongDistance, location.OutsideLineAccess );

	if ( location.DialAsLongDistance == TRUE )
		strcpy( line.csDialAsLongDistance, "528" );    // ??? What's the magic of 528 ???
	else
		strcpy( line.csDialAsLongDistance, "" );
	
	line.nPulseDialing = ( location.DialType == 0 ? 1 : 0 );
	
	if ( location.DisableCallWaiting )
		strcpy( line.csCallWaiting, location.DisableCallWaitingCode );
	else
		strcpy( line.csCallWaiting, "" );
	
	if ( strcmp( location.UserAreaCode, "" ) != 0 )
		strcpy( line.csAreaCode, location.UserAreaCode );
	
	// write the line back out
	writeTapiLine( lpszDir, nId, &line );

	if (!found)
	{
		// Create a whole location section if creating a new entry
		::WritePrivateProfileString( "Locations", "CurrentLocation", "0,0", lpszDir );
		::WritePrivateProfileString( "Locations", "Locations", "1,1", lpszDir );
		::WritePrivateProfileString( "Locations", "Inited", "1", lpszDir );
	}

	return TRUE;
}
	

//********************************************************************************
// SetLocationInfoNT
// sets the location info for winNT dialers
//********************************************************************************
BOOL SetLocationInfoNT( ACCOUNTPARAMS account, LOCATIONPARAMS location )
{
	LINEINITIALIZEEXPARAMS  m_LineInitExParams;
	HLINEAPP                m_LineApp;
	DWORD                   dwNumDevs;
	LINETRANSLATECAPS       m_LineTranslateCaps;
	
	DWORD					dwApiVersion = 0x00020000;
	
	
	// Initialize TAPI. in order to get current location ID
	m_LineInitExParams.dwOptions = LINEINITIALIZEEXOPTION_USEEVENT;
	m_LineInitExParams.dwTotalSize = sizeof( LINEINITIALIZEEXPARAMS );
	m_LineInitExParams.dwNeededSize = sizeof( LINEINITIALIZEEXPARAMS );
	
	
	if ( lineInitialize( &m_LineApp, gDLL, lineCallbackFuncNT, NULL, &dwNumDevs) != 0 )
	{
		char		buf[ 255 ];
		if ( getMsgString( buf, IDS_NO_TAPI ) )
			displayErrMsgWnd( buf, MB_OK | MB_ICONEXCLAMATION, NULL );
		return FALSE;
	}
	
	m_LineTranslateCaps.dwTotalSize = sizeof( LINETRANSLATECAPS );
	m_LineTranslateCaps.dwNeededSize = sizeof( LINETRANSLATECAPS );
	if ( lineGetTranslateCaps( m_LineApp, dwApiVersion, &m_LineTranslateCaps ) != 0 )
		return FALSE;
	
	
	//m_LineTranslateCaps.dwCurrentLocationID
	
	// gets the location info from registry
	HKEY		hKey;
	HKEY		hkeyNewSubkey;
	BOOL		subkeyAllocated = FALSE;
	char*	keyPath = "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Telephony\\locations";
	
	// finds the user profile location in registry
	if( ERROR_SUCCESS != RegOpenKeyEx( HKEY_LOCAL_MACHINE, keyPath, NULL, KEY_ALL_ACCESS, &hKey ) )
		return FALSE;
	
	DWORD					subKeys;
	DWORD					maxSubKeyLen;
	DWORD					maxClassLen;
	DWORD					values;
	DWORD					maxValueNameLen;
	DWORD					maxValueLen;
	DWORD					securityDescriptor;
	FILETIME				lastWriteTime;
	char					*subkeyName;
	
	// get some information about this reg key 
	if ( ERROR_SUCCESS != RegQueryInfoKey( hKey, NULL, NULL, NULL, &subKeys, &maxSubKeyLen, &maxClassLen, &values, &maxValueNameLen, &maxValueLen, &securityDescriptor, &lastWriteTime ) )
		goto fail;
	
	
	// now loop through the location keys to find the one that matches current location ID
	if ( subKeys > 0 )
	{
		// Pulled this out of the loop since it's constant
		DWORD 	flagsVal = 0;
		if ( location.DialType != 0 )
			flagsVal += 1;
		if ( location.DisableCallWaiting )
			flagsVal += 4;

		// Pulled this out of the loop since it's constant
		char*	callwaiting;
		if( location.DisableCallWaiting )
			callwaiting = location.DisableCallWaitingCode;
		else
			callwaiting ="";
	
		// Pulled this out of the loop since it's constant
		char * areacode = NULL;
		if( strcmp( location.UserAreaCode, "" ) != 0 )
			areacode = location.UserAreaCode;
		else if ( account.IntlMode )
			// There won't be an area code when we're in international mode,
			// force a default area code, so that we don't get an error creating a dialer
			areacode = "415"; 	

		DWORD		subkeyNameSize = maxSubKeyLen + 1;
		subkeyName = (char *)malloc( subkeyNameSize );
		if (subkeyName == NULL)
			goto fail;
	
		for ( DWORD index = 0; index < subKeys; index++ )
		{
			// gets a location key name
			if ( ERROR_SUCCESS != RegEnumKey( hKey, index, subkeyName, subkeyNameSize ) ) 
				continue;	// not fatal, just go to the next one
	
			// try open location key
			char		newSubkeyPath[ 260 ];
	
			strcpy( (char*)newSubkeyPath, keyPath );
			strcat( (char*)newSubkeyPath, "\\" ); 
			strcat( (char*)newSubkeyPath, subkeyName );
	
			if ( ERROR_SUCCESS != RegOpenKeyEx( HKEY_LOCAL_MACHINE, (char*)newSubkeyPath, NULL, KEY_ALL_ACCESS, &hkeyNewSubkey ) )
				goto fail;
			subkeyAllocated = TRUE;
	
			DWORD		valbuf;
			DWORD		type = REG_SZ;
			DWORD		bufsize = 20;
	
			// get location key's ID value
			if ( ERROR_SUCCESS == RegQueryValueEx( hkeyNewSubkey, "ID", NULL, &type, (LPBYTE)&valbuf, &bufsize ) )
			{
				// if it matches the default location ID
				if ( valbuf == m_LineTranslateCaps.dwCurrentLocationID )
				{
					if ( ERROR_SUCCESS != RegSetValueEx( hkeyNewSubkey, "Flags", NULL, type, (LPBYTE)&flagsVal, bufsize ) )
						goto fail;
	
					// sets the OutsideAccess
					if ( ERROR_SUCCESS != RegSetValueEx( hkeyNewSubkey, "OutsideAccess", NULL, REG_SZ, (LPBYTE)&location.OutsideLineAccess, strlen(location.OutsideLineAccess ) + 1 ) )
						goto fail;
	
					if ( ERROR_SUCCESS != RegSetValueEx( hkeyNewSubkey, "LongDistanceAccess", NULL, REG_SZ, (LPBYTE)&location.OutsideLineAccess, strlen(location.OutsideLineAccess ) + 1 ) )
						goto fail;
	
					if ( ERROR_SUCCESS != RegSetValueEx( hkeyNewSubkey, "DisableCallWaiting", NULL, REG_SZ, (LPBYTE)callwaiting, strlen( callwaiting ) + 1 ) )
						goto fail;
	
					if (areacode)
						if ( ERROR_SUCCESS != RegSetValueEx( hkeyNewSubkey, "AreaCode", NULL, REG_SZ, (LPBYTE)areacode, strlen( areacode ) + 1 ) )
							goto fail;
				}
			}
	
			RegCloseKey( hkeyNewSubkey );
			subkeyAllocated = FALSE;
		}

		free(subkeyName);
		subkeyName = NULL;
	}
	
	RegCloseKey( hKey );
	
	return TRUE;

fail:
	if (subkeyName)
		free(subkeyName);

	if (subkeyAllocated)
		RegCloseKey( hkeyNewSubkey );

	RegCloseKey( hKey );

	return FALSE;
}

//
// !!! Since this routine sets up a bunch of globals, one presumes it must be 
// !!! called before any other routines are used.
//
//********************************************************************************
// native method:
// CheckEnvironment
// checks for DUN, RAS function loading correctly
//********************************************************************************
BOOL CheckEnvironment()
{
	// Ensure gPlatformOS is set properly
	OSVERSIONINFO*	lpOsVersionInfo = new OSVERSIONINFO;
	lpOsVersionInfo->dwOSVersionInfoSize = sizeof( OSVERSIONINFO );

	if ( GetVersionEx( lpOsVersionInfo ) ) 
		gPlatformOS = (int)lpOsVersionInfo->dwPlatformId;
	else
		gPlatformOS = VER_PLATFORM_WIN32_WINDOWS;  // Make reasonable assumption

	if ( lpOsVersionInfo != NULL )
	{
		delete lpOsVersionInfo;
		lpOsVersionInfo = NULL;
	}

	// try loading RAS routines in RAS dlls
	// if fails return FALSE
/*	switch ( gPlatformOS )
	{
		case VER_PLATFORM_WIN32_NT:   // NT
			//check if it's WinNT40 first
			g_lpfn_loaded = LoadRasFunctionsNT( "RASAPI32.DLL" );
			break;
	
		case VER_PLATFORM_WIN32_WINDOWS:    // defaults to win95	
			g_lpfn_loaded = LoadRasFunctions( "RASAPI32.DLL" ) && LoadRasFunctions( "RNAPH.DLL" );
			break;
	}
	if ( !g_lpfn_loaded )
	{
		// Err: Unable to dynamically load extended RAS functions!
		char	buf[MAX_PATH];
		if ( getMsgString( buf, IDS_NO_RAS_FUNCTIONS ) )
			displayErrMsgWnd( buf, MB_OK | MB_ICONEXCLAMATION, NULL);
	
		return FALSE;
	}
*/	
	// Check to make sure Dial-Up Networking is installed.
	//   It may be uninstalled by user.
	// return FALSE if Dialup Networking is not installed
	BOOL checkOK = FALSE;
	switch ( gPlatformOS )
	{
		case VER_PLATFORM_WIN32_NT:
			checkOK = CheckDUN_NT();
			break;
		default:
			checkOK = CheckDUN();
			break;
	}
	if ( !checkOK )
	{
		char buf[ 255 ];
		if ( getMsgString( (char*)buf, IDS_NO_DUN_INSTALLED ) )
			displayErrMsgWnd( (char*)buf, MB_OK | MB_ICONEXCLAMATION, NULL );
		return FALSE;
	}
	
	// for win95 only:
	// Check to see if DNS is already configured for a LAN connection.
	// If so warn the user that this may cause conflicts, and continue.
//	if ( gPlatformOS == VER_PLATFORM_WIN32_WINDOWS )
//		CheckDNS();
	
	return TRUE;
}
// Set the data stream into an array
void SetDataArray(nsString data)
{

#if defined(DEBUG_profile)
	printf("ProfileManager : Setting new profile data\n");
	printf("SetDataArray data : %s\n", data.ToNewCString());
#endif

	int index = 0;
	char *newStr=nsnull;
	char *tokstr = data.ToNewCString();
	char *token = nsCRT::strtok(tokstr, "%", &newStr);

#if defined(DEBUG_profile)
	printf("before while loop\n");
#endif

	while (token)
    {

#if defined(DEBUG_profile)
	printf("subTok : %s\n", token);
#endif
    
		PL_strcpy(gNewAccountData[index], token);
		index++;
		g_Count = index;
      
		token = nsCRT::strtok(newStr, "%", &newStr);
    }
  
	delete[] tokstr;

#if defined(DEBUG_profile)
  printf("after while loop\n");
#endif

}

//Get the value associated with the name from Data Array
// i.e., generated from the data passed by the JS reflection
// request to create a new profile (CreateProfile Wizard).
char* GetValue(char *name)
{
    int nameLength;
    char* value;
    value = (char *) PR_Malloc(sizeof(char) * _MAX_LENGTH);

    for (int i = 0; i < g_Count; i=i+1) 
	{
        if (gNewAccountData[i]) 
		{
            nameLength = PL_strlen(name);

            if (PL_strncmp(name, gNewAccountData[i], nameLength) == 0) 
			{
                char* equalsPosition = PL_strchr(gNewAccountData[i], '=');
                if (equalsPosition) 
				{
                    PL_strcpy(value, 1 + equalsPosition);
                    return value;
                }
            }
        }
    }

#if defined(DEBUG_profile)
    printf("after for loop\n");
#endif

    return nsnull;
} 

// Gets the number of Modems
// Location: Common/Profiles
char* GetModemConfig(void)
{
	char**                  modemResults;
	int             numDevices;
	int             i;
	nsString                 str, tmp, returnData;

	if ( !::GetModemList( &modemResults, &numDevices ) )
	{
		if ( modemResults != NULL )
		{
			for ( i = 0; i < numDevices; i++ )
			{
				if ( modemResults[ i ] != NULL )
					delete []modemResults[ i ];
			}
			delete []modemResults;
		}
		return NULL;
	}

	// copy all entries to the array
//	returnData[ 0 ] = 0x00;
	returnData.SetLength(0);
/*
	// pile up account names in a single array, separated by a ()
	for ( i = 0; i < numDevices; i++ ) 
	{   
		tmp = modemResults[ i ];
		str += tmp;
		str += "()";  
		delete []modemResults[ i ];
	}
//	strcpy( returnData, (const char*)str ); */
	returnData.AssignWithConversion(modemResults[0]);
//	returnData = tmp;
	printf("this is the modem inside the modemconfig %s \n", returnData.ToNewCString());
	delete []modemResults;

return returnData.ToNewCString();
}


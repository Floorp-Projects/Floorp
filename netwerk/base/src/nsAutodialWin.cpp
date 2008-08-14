/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Steve Meredith <smeredith@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

// This source is mostly a bunch of Windows API calls. It is only compiled for
// Windows builds.
//
// Registry entries for Autodial mappings are located here:
//  HKEY_CURRENT_USER\Software\Microsoft\RAS Autodial\Addresses

#include <windows.h>
#include <winsvc.h>
#include "nsAutodialWin.h"
#include "prlog.h"

#ifdef WINCE
#include <objbase.h>
#include <initguid.h>
#include <connmgr.h>
#endif

#ifdef WINCE
#define AUTODIAL_DEFAULT AUTODIAL_ALWAYS
#else
#define AUTODIAL_DEFAULT AUTODIAL_NEVER
#endif

//
// Log module for autodial logging...
//
// To enable logging (see prlog.h for full details):
//
//    set NSPR_LOG_MODULES=Autodial:5
//    set NSPR_LOG_FILE=nspr.log
//
// this enables PR_LOG_DEBUG level information and places all output in
// the file nspr.log
//

#ifdef PR_LOGGING
static PRLogModuleInfo* gLog = nsnull;
#endif

#define LOGD(args) PR_LOG(gLog, PR_LOG_DEBUG, args)
#define LOGE(args) PR_LOG(gLog, PR_LOG_ERROR, args)

// Don't try to dial again within a few seconds of when user pressed cancel.
#define NO_RETRY_PERIOD_SEC 5
PRIntervalTime nsRASAutodial::mDontRetryUntil = 0;


tRASPHONEBOOKDLG nsRASAutodial::mpRasPhonebookDlg = nsnull;
tRASENUMCONNECTIONS nsRASAutodial::mpRasEnumConnections = nsnull;
tRASENUMENTRIES nsRASAutodial::mpRasEnumEntries = nsnull;
tRASDIALDLG nsRASAutodial::mpRasDialDlg = nsnull;
tRASSETAUTODIALADDRESS nsRASAutodial::mpRasSetAutodialAddress = nsnull;
tRASGETAUTODIALADDRESS nsRASAutodial::mpRasGetAutodialAddress = nsnull;
tRASGETAUTODIALENABLE nsRASAutodial::mpRasGetAutodialEnable = nsnull;
tRASGETAUTODIALPARAM nsRASAutodial::mpRasGetAutodialParam = nsnull;

HINSTANCE nsRASAutodial::mhRASdlg = nsnull;
HINSTANCE nsRASAutodial::mhRASapi32 = nsnull;

// ctor. 
nsRASAutodial::nsRASAutodial()
:   mAutodialBehavior(AUTODIAL_DEFAULT),
    mNumRASConnectionEntries(0),
    mAutodialServiceDialingLocation(-1)
{
    mOSVerInfo.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
    GetVersionEx(&mOSVerInfo);

    // Initializations that can be made again since RAS OS settings can 
    // change.
    Init();
}

// dtor
nsRASAutodial::~nsRASAutodial()
{
}


// Get settings from the OS. These are settings that might change during
// the OS session. Call Init() again to pick up those changes if required.
// Returns NS_ERROR_FAILURE if error or NS_OK if success.
nsresult nsRASAutodial::Init()
{
#ifdef PR_LOGGING
    if (!gLog)
        gLog = PR_NewLogModule("Autodial");
#endif

    mDefaultEntryName[0] = '\0';
    mNumRASConnectionEntries = 0;
    mAutodialBehavior = QueryAutodialBehavior();
    
    // No need to continue in this case.
    if (mAutodialBehavior == AUTODIAL_NEVER)
    {
        return NS_OK;
    }


    // Get the number of dialup entries in the phonebook.
    mNumRASConnectionEntries = NumRASEntries();
    
    // Get the name of the default entry.
    nsresult result = GetDefaultEntryName(mDefaultEntryName, 
                                           RAS_MaxEntryName + 1);
    
    return result;
}


// Should we attempt to dial on a network error? Yes if the Internet Options
// configured as such. Yes if the RAS autodial service is running (we'll try to
// force it to dial in that case by adding the network address to its db.)
PRBool nsRASAutodial::ShouldDialOnNetworkError()
{
#ifndef WINCE
    // Don't try to dial again within a few seconds of when user pressed cancel.
    if (mDontRetryUntil) 
    {
        PRIntervalTime intervalNow = PR_IntervalNow();
        if (intervalNow < mDontRetryUntil) 
        {
            LOGD(("Autodial: Not dialing: too soon."));
            return PR_FALSE;
        }
    }
     

    return ((mAutodialBehavior == AUTODIAL_ALWAYS) 
             || (mAutodialBehavior == AUTODIAL_ON_NETWORKERROR)
             || (mAutodialBehavior == AUTODIAL_USE_SERVICE));
#else
    return PR_TRUE;
#endif
}


// The autodial info is set in Control Panel | Internet Options | Connections.
// The values are stored in the registry. This function gets those values from
// the registry and determines if we should never dial, always dial, or dial
// when there is no network found.
int nsRASAutodial::QueryAutodialBehavior()
{
#ifndef WINCE
    if (IsAutodialServiceRunning())
    {
        if (!LoadRASapi32DLL())
            return AUTODIAL_NEVER;

        // Is Autodial service enabled for the current login session?
        DWORD disabled = 0;
        DWORD size = sizeof(DWORD);
        if ((*mpRasGetAutodialParam)(RASADP_LoginSessionDisable, &disabled, &size) == ERROR_SUCCESS)
        {
            if (!disabled)
            {
                // If current dialing location has autodial on, we'll let the service dial.
                mAutodialServiceDialingLocation = GetCurrentLocation();
                if (IsAutodialServiceEnabled(mAutodialServiceDialingLocation))
                {
                    return AUTODIAL_USE_SERVICE;
                }
            }
        }
    }

    // If we get to here, then the service is not going to dial on error, so we
    // can dial ourselves if the control panel settings are set up that way.
    HKEY hKey = 0;
    LONG result = ::RegOpenKeyEx(
                    HKEY_CURRENT_USER, 
                    "Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings", 
                    0, 
                    KEY_READ, 
                    &hKey);

    if (result != ERROR_SUCCESS)
    {
        LOGE(("Autodial: Error opening reg key Internet Settings"));
        return AUTODIAL_NEVER;
    }

    DWORD entryType = 0;
    DWORD autodial = 0;
    DWORD onDemand = 0;
    DWORD paramSize = sizeof(DWORD);

    result = ::RegQueryValueEx(hKey, "EnableAutodial", nsnull, &entryType, (LPBYTE)&autodial, &paramSize);
    if (result != ERROR_SUCCESS)
    {
        ::RegCloseKey(hKey);
        LOGE(("Autodial: Error reading reg value EnableAutodial."));
        return AUTODIAL_NEVER;
    }

    result = ::RegQueryValueEx(hKey, "NoNetAutodial", nsnull, &entryType, (LPBYTE)&onDemand, &paramSize);
    if (result != ERROR_SUCCESS)
    {
        ::RegCloseKey(hKey);
        LOGE(("Autodial: Error reading reg value NoNetAutodial."));
        return AUTODIAL_NEVER;
    }
  
    ::RegCloseKey(hKey);

    if (!autodial)
    {
        return AUTODIAL_NEVER;
    }
    else 
    {
        if (onDemand)
        {
            return AUTODIAL_ON_NETWORKERROR;
        }
        else
        {
            return AUTODIAL_ALWAYS;
        }
    }
#else
    return AUTODIAL_DEFAULT;
#endif
}


#ifdef WINCE
static nsresult DoPPCConnection()
{
    static HANDLE    gConnectionHandle = NULL;

    // Make the connection to the new network
    CONNMGR_CONNECTIONINFO conn_info;
    memset(&conn_info, 0, sizeof(CONNMGR_CONNECTIONINFO));

    conn_info.cbSize      = sizeof(CONNMGR_CONNECTIONINFO);
    conn_info.dwParams    = CONNMGR_PARAM_GUIDDESTNET;
    conn_info.dwPriority  = CONNMGR_PRIORITY_USERINTERACTIVE;
    conn_info.guidDestNet = IID_DestNetInternet;
    conn_info.bExclusive  = FALSE;
    conn_info.bDisabled   = FALSE;

    HANDLE tempConnectionHandle;
    DWORD status;
    HRESULT result = ConnMgrEstablishConnectionSync(&conn_info, 
                                                    &tempConnectionHandle, 
                                                    60000,
                                                    &status);

    if (result != S_OK)
    {
      return NS_ERROR_FAILURE;
    }

    if (status != CONNMGR_STATUS_CONNECTED)
    {
      // could not connect to this network.  release the
      // temp connection.
      ConnMgrReleaseConnection(tempConnectionHandle, 0);
      return NS_ERROR_FAILURE;
    }

    // At this point, we have a new connection, so release
    // the old connection
    if (gConnectionHandle)
      ConnMgrReleaseConnection(gConnectionHandle, 0);
      
    gConnectionHandle = tempConnectionHandle;
    return NS_OK;
}

#endif

// If the RAS autodial service is running, use it. Otherwise, dial
// the default RAS connection. There are two possible RAS dialogs:
// one that dials a single entry, and one that lets the user choose which
// to dial. If there is only one connection entry in the phone book, or
// there are multiple entries but one is defined as the default, we'll use
// the single entry dial dialog. If there are multiple connection entries,
// and none is specified as default, we'll bring up the diallog which lets
// the user select the connection entry to use.
//
// Return values:
//  NS_OK: dialing was successful and caller should retry
//  all other values indicate that the caller should not retry
nsresult nsRASAutodial::DialDefault(const char* hostName)
{
#ifndef WINCE
    mDontRetryUntil = 0;

    if (mAutodialBehavior == AUTODIAL_NEVER)
    {
        return NS_ERROR_FAILURE;    // don't retry the network error
    }

    // If already a RAS connection, bail.
    if (IsRASConnected())
    {
        LOGD(("Autodial: Not dialing: active connection."));
        return NS_ERROR_FAILURE;    // don't retry
    }

    // If no dialup connections configured, bail.
    if (mNumRASConnectionEntries <= 0)
    {
        LOGD(("Autodial: Not dialing: no entries."));
        return NS_ERROR_FAILURE;    // don't retry
    }


    // If autodial service is running, let it dial. In order for it to dial more
    // reliably, we have to add the target address to the autodial database.
    // This is the only way the autodial service dial if there is a network
    // adapter installed. But even then it might not dial. We have to assume that
    // it will though, or we could end up with two attempts to dial on the same
    // network error if the user cancels the first one: one from the service and
    // one from us.
    // See http://msdn.microsoft.com/library/default.asp?url=/library/en-us/rras/ras4over_3dwl.asp
    if (mAutodialBehavior == AUTODIAL_USE_SERVICE)
    {
        AddAddressToAutodialDirectory(hostName);
        return NS_ERROR_FAILURE;    // don't retry
    }

    // Do the dialing ourselves.
    else
    {
        // Don't need to load the dll before this.
        if (!LoadRASdlgDLL())
            return NS_ERROR_NULL_POINTER;

        // If a default dial entry is configured, use it.
        if (mDefaultEntryName[0] != '\0') 
        {
            LOGD(("Autodial: Dialing default: %s.",mDefaultEntryName));

            RASDIALDLG rasDialDlg;
            memset(&rasDialDlg, 0, sizeof(RASDIALDLG));
            rasDialDlg.dwSize = sizeof(RASDIALDLG);

            PRBool dialed = 
             (*mpRasDialDlg)(nsnull, mDefaultEntryName, nsnull, &rasDialDlg);

            if (!dialed)
            {
                if (rasDialDlg.dwError != 0)
                {
                    LOGE(("Autodial ::RasDialDlg failed: Error: %d.", 
                     rasDialDlg.dwError));
                }
                else
                {
                    mDontRetryUntil = PR_IntervalNow() + PR_SecondsToInterval(NO_RETRY_PERIOD_SEC);
                    LOGD(("Autodial: User cancelled dial."));
                }
                return NS_ERROR_FAILURE;    // don't retry
            }

            LOGD(("Autodial: RAS dialup connection successful."));
        }

        // If no default connection specified, open the dialup dialog that lets
        // the user specifiy which connection to dial.
        else
        {
            LOGD(("Autodial: Prompting for phonebook entry."));

            RASPBDLG rasPBDlg;
            memset(&rasPBDlg, 0, sizeof(RASPBDLG));
            rasPBDlg.dwSize = sizeof(RASPBDLG);
 
            PRBool dialed = (*mpRasPhonebookDlg)(nsnull, nsnull, &rasPBDlg);

            if (!dialed)
            {
                if (rasPBDlg.dwError != 0)
                {
                    LOGE(("Autodial: ::RasPhonebookDlg failed: Error = %d.", 
                     rasPBDlg.dwError));
                }
                else
                {
                    mDontRetryUntil = PR_IntervalNow() + PR_SecondsToInterval(NO_RETRY_PERIOD_SEC);
                    LOGD(("Autodial: User cancelled dial."));
                }

                return NS_ERROR_FAILURE;    // don't retry
            }

            LOGD(("Autodial: RAS dialup connection successful."));
        }
    }

    // Retry because we just established a dialup connection.
    return NS_OK;

#else
    return  DoPPCConnection();
#endif
}


// Check to see if RAS is already connected.
PRBool nsRASAutodial::IsRASConnected()
{
    DWORD connections;
    RASCONN rasConn;
    rasConn.dwSize = sizeof(RASCONN);
    DWORD structSize = sizeof(RASCONN);

    if (!LoadRASapi32DLL())
        return NS_ERROR_NULL_POINTER;

    DWORD result = (*mpRasEnumConnections)(&rasConn, &structSize, &connections);

    // ERROR_BUFFER_TOO_SMALL is OK because we only need one struct.
    if (result == ERROR_SUCCESS || result == ERROR_BUFFER_TOO_SMALL)
    {
        return (connections > 0);
    }

    LOGE(("Autodial: ::RasEnumConnections failed: Error = %d", result));
    return PR_FALSE;
}

// Get the first RAS dial entry name from the phonebook.
nsresult nsRASAutodial::GetFirstEntryName(char* entryName, int bufferSize)
{
    // Need to load the DLL if not loaded yet.
    if (!LoadRASapi32DLL())
        return NS_ERROR_NULL_POINTER;

    RASENTRYNAME rasEntryName;
    rasEntryName.dwSize = sizeof(RASENTRYNAME);
    DWORD cb = sizeof(RASENTRYNAME);
    DWORD cEntries = 0;

    DWORD result = 
     (*mpRasEnumEntries)(nsnull, nsnull, &rasEntryName, &cb, &cEntries);

    // ERROR_BUFFER_TOO_SMALL is OK because we only need one struct.
    if (result == ERROR_SUCCESS || result == ERROR_BUFFER_TOO_SMALL)
    {
#ifndef WINCE
        strncpy(entryName, rasEntryName.szEntryName, bufferSize);
#endif
        return NS_OK;
    }

    return NS_ERROR_FAILURE;
}

// Get the number of RAS dial entries in the phonebook.
int nsRASAutodial::NumRASEntries()
{
    // Need to load the DLL if not loaded yet.
    if (!LoadRASapi32DLL())
        return 0;

    RASENTRYNAME rasEntryName;
    rasEntryName.dwSize = sizeof(RASENTRYNAME);
    DWORD cb = sizeof(RASENTRYNAME);
    DWORD cEntries = 0;


    DWORD result = 
     (*mpRasEnumEntries)(nsnull, nsnull, &rasEntryName, &cb, &cEntries);

    // ERROR_BUFFER_TOO_SMALL is OK because we only need one struct.
    if (result == ERROR_SUCCESS || result == ERROR_BUFFER_TOO_SMALL)
    {
        return (int)cEntries;
    }

    return 0;
}

// Get the name of the default dial entry.
nsresult nsRASAutodial::GetDefaultEntryName(char* entryName, int bufferSize)
{
    // No RAS dialup entries. 
    if (mNumRASConnectionEntries <= 0)
    {
        return NS_ERROR_FAILURE;
    }

    // Single RAS dialup entry. Use it as the default to autodial.
    if (mNumRASConnectionEntries == 1)
    {
        return GetFirstEntryName(entryName, bufferSize);
    }

    // Multiple RAS dialup entries. If a default configured in the registry,
    // use it. 
    //
    // For Windows XP: HKCU/Software/Microsoft/RAS Autodial/Default/DefaultInternet.
    //              or HKLM/Software/Microsoft/RAS Autodial/Default/DefaultInternet.
    // For Windows 2K: HKCU/RemoteAccess/InternetProfile.

    char* key = nsnull;
    char* val = nsnull;

    HKEY hKey = 0;
    LONG result = 0;

    // Windows NT and 2000
    if ((mOSVerInfo.dwMajorVersion == 4) // Windows NT
     || ((mOSVerInfo.dwMajorVersion == 5) && (mOSVerInfo.dwMinorVersion == 0))) // Windows 2000
    {
        key = "RemoteAccess";
        val = "InternetProfile";

        result = ::RegOpenKeyEx(
                    HKEY_CURRENT_USER, 
                    key, 
                    0, 
                    KEY_READ, 
                    &hKey);

        if (result != ERROR_SUCCESS)
        {
            return NS_ERROR_FAILURE;
        }
    }
    else  // Windows XP
    {
        key = "Software\\Microsoft\\RAS Autodial\\Default";
        val = "DefaultInternet";

        
        // Try HKCU first.
        result = ::RegOpenKeyEx(
                    HKEY_CURRENT_USER, 
                    key, 
                    0, 
                    KEY_READ, 
                    &hKey);

        if (result != ERROR_SUCCESS)
        {
            // If not present, try HKLM.
            result = ::RegOpenKeyEx(
                        HKEY_LOCAL_MACHINE, 
                        key, 
                        0, 
                        KEY_READ, 
                        &hKey);

            if (result != ERROR_SUCCESS)
            {
                return NS_ERROR_FAILURE;
            }
        }
    }


    DWORD entryType = 0;
    DWORD buffSize = bufferSize;

    result = ::RegQueryValueEx(hKey, 
                                val, 
                                nsnull, 
                                &entryType, 
                                (LPBYTE)entryName, 
                                &buffSize);

    ::RegCloseKey(hKey);


    if (result != ERROR_SUCCESS)
    {
        // Results in a prompt for which to use at dial time.
        return NS_ERROR_FAILURE;
    }

    return NS_OK;
}


// Determine if the autodial service is running on this PC.
PRBool nsRASAutodial::IsAutodialServiceRunning()
{
#ifndef WINCE
    SC_HANDLE hSCManager = 
      OpenSCManager(nsnull, SERVICES_ACTIVE_DATABASE, SERVICE_QUERY_STATUS);

    if (hSCManager == nsnull)
    {
        LOGE(("Autodial: failed to open service control manager. Error %d.", 
          ::GetLastError()));

        return PR_FALSE;
    }

    SC_HANDLE hService = 
      OpenService(hSCManager, "RasAuto", SERVICE_QUERY_STATUS);

    if (hSCManager == nsnull)
    {
        LOGE(("Autodial: failed to open RasAuto service."));
        return PR_FALSE;
    }

    SERVICE_STATUS status;
    if (!QueryServiceStatus(hService, &status))
    {
        LOGE(("Autodial: ::QueryServiceStatus() failed. Error: %d", 
          ::GetLastError()));

        return PR_FALSE;
    }

    return (status.dwCurrentState == SERVICE_RUNNING);
#else
    return PR_TRUE;
#endif
}

// Add the specified address to the autodial directory.
PRBool nsRASAutodial::AddAddressToAutodialDirectory(const char* hostName)
{
    // Need to load the DLL if not loaded yet.
    if (!LoadRASapi32DLL())
        return PR_FALSE;

    // First see if there is already a db entry for this address. 
    RASAUTODIALENTRY autodialEntry;
    autodialEntry.dwSize = sizeof(RASAUTODIALENTRY);
    DWORD size = sizeof(RASAUTODIALENTRY);
    DWORD entries = 0;

    DWORD result = (*mpRasGetAutodialAddress)(hostName, 
                                    nsnull, 
                                    &autodialEntry, 
                                    &size, 
                                    &entries);

    // If there is already at least 1 entry in db for this address, return.
    if (result != ERROR_FILE_NOT_FOUND)
    {
        LOGD(("Autodial: Address %s already in autodial db.", hostName));
        return PR_FALSE;
    }

    autodialEntry.dwSize = sizeof(RASAUTODIALENTRY);
    autodialEntry.dwFlags = 0;
    autodialEntry.dwDialingLocation = mAutodialServiceDialingLocation;
    GetDefaultEntryName(autodialEntry.szEntry, RAS_MaxEntryName);

    result = (*mpRasSetAutodialAddress)(hostName, 
                                    0, 
                                    &autodialEntry, 
                                    sizeof(RASAUTODIALENTRY), 
                                    1);

    if (result != ERROR_SUCCESS)
    {
        LOGE(("Autodial ::RasSetAutodialAddress failed result %d.", result));
        return PR_FALSE;
    }

    LOGD(("Autodial: Added address %s to RAS autodial db for entry %s.",
     hostName, autodialEntry.szEntry));

    return PR_TRUE;
}

// Get the current TAPI dialing location.
int nsRASAutodial::GetCurrentLocation()
{
    HKEY hKey = 0;
    LONG result = ::RegOpenKeyEx(
                    HKEY_LOCAL_MACHINE, 
                    "Software\\Microsoft\\Windows\\CurrentVersion\\Telephony\\Locations", 
                    0, 
                    KEY_READ, 
                    &hKey);

    if (result != ERROR_SUCCESS)
    {
        LOGE(("Autodial: Error opening reg key ...CurrentVersion\\Telephony\\Locations"));
        return -1;
    }

    DWORD entryType = 0;
    DWORD location = 0;
    DWORD paramSize = sizeof(DWORD);

    result = ::RegQueryValueEx(hKey, "CurrentID", nsnull, &entryType, (LPBYTE)&location, &paramSize);
    if (result != ERROR_SUCCESS)
    {
        ::RegCloseKey(hKey);
        LOGE(("Autodial: Error reading reg value CurrentID."));
        return -1;
    }

    ::RegCloseKey(hKey);
    return location;

}

// Check to see if autodial for the specified location is enabled. 
PRBool nsRASAutodial::IsAutodialServiceEnabled(int location)
{
    if (location < 0)
        return PR_FALSE;

    if (!LoadRASapi32DLL())
        return PR_FALSE;

    PRBool enabled;
    if ((*mpRasGetAutodialEnable)(location, &enabled) != ERROR_SUCCESS)
    {
        LOGE(("Autodial: Error calling RasGetAutodialEnable()"));
        return PR_FALSE;
    }

    return enabled;
}



PRBool nsRASAutodial::LoadRASapi32DLL()
{
    if (!mhRASapi32)
    {
        mhRASapi32 = ::LoadLibrary("rasapi32.dll");
        if ((UINT)mhRASapi32 > 32)
        {
            // RasEnumConnections
            mpRasEnumConnections = (tRASENUMCONNECTIONS)
             ::GetProcAddress(mhRASapi32, "RasEnumConnectionsA");

            // RasEnumEntries
            mpRasEnumEntries = (tRASENUMENTRIES)
             ::GetProcAddress(mhRASapi32, "RasEnumEntriesA");

            // RasSetAutodialAddress
            mpRasSetAutodialAddress = (tRASSETAUTODIALADDRESS)
                ::GetProcAddress(mhRASapi32, "RasSetAutodialAddressA");

            // RasGetAutodialAddress
            mpRasGetAutodialAddress = (tRASGETAUTODIALADDRESS)
             ::GetProcAddress(mhRASapi32, "RasGetAutodialAddressA");

            // RasGetAutodialEnable
            mpRasGetAutodialEnable = (tRASGETAUTODIALENABLE)
             ::GetProcAddress(mhRASapi32, "RasGetAutodialEnableA");

            // RasGetAutodialParam
            mpRasGetAutodialParam = (tRASGETAUTODIALPARAM)
             ::GetProcAddress(mhRASapi32, "RasGetAutodialParamA");
        }

    }

    if (!mhRASapi32 
        || !mpRasEnumConnections 
        || !mpRasEnumEntries 
        || !mpRasSetAutodialAddress
        || !mpRasGetAutodialAddress
        || !mpRasGetAutodialEnable
        || !mpRasGetAutodialParam)
    {
        LOGE(("Autodial: Error loading RASAPI32.DLL."));
        return PR_FALSE;
    }

    return PR_TRUE;
}

PRBool nsRASAutodial::LoadRASdlgDLL()
{
    if (!mhRASdlg)
    {
        mhRASdlg = ::LoadLibrary("rasdlg.dll");
        if ((UINT)mhRASdlg > 32)
        {
            // RasPhonebookDlg
            mpRasPhonebookDlg =
             (tRASPHONEBOOKDLG)::GetProcAddress(mhRASdlg, "RasPhonebookDlgA");

            // RasDialDlg
            mpRasDialDlg =
             (tRASDIALDLG)::GetProcAddress(mhRASdlg, "RasDialDlgA");

        }
    }

    if (!mhRASdlg || !mpRasPhonebookDlg || !mpRasDialDlg)
    {
        LOGE(("Autodial: Error loading RASDLG.DLL."));
        return PR_FALSE;
    }

    return PR_TRUE;
}


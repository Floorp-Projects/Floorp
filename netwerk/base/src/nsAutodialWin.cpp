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
#include "nsString.h"
#include "nsAutodialWin.h"
#include "prlog.h"

#define AUTODIAL_DEFAULT AUTODIAL_NEVER

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
PRIntervalTime nsAutodial::mDontRetryUntil = 0;

// ctor. 
nsAutodial::nsAutodial()
:   mAutodialBehavior(AUTODIAL_DEFAULT),
    mNumRASConnectionEntries(0),
    mAutodialServiceDialingLocation(-1)
{
    mOSVerInfo.dwOSVersionInfoSize = sizeof(mOSVerInfo);
    GetVersionEx(&mOSVerInfo);

    // Initializations that can be made again since RAS OS settings can 
    // change.
    Init();
}

// dtor
nsAutodial::~nsAutodial()
{
}


// Get settings from the OS. These are settings that might change during
// the OS session. Call Init() again to pick up those changes if required.
// Returns NS_ERROR_FAILURE if error or NS_OK if success.
nsresult nsAutodial::Init()
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
                                          sizeof(mDefaultEntryName));
    
    return result;
}


// Should we attempt to dial on a network error? Yes if the Internet Options
// configured as such. Yes if the RAS autodial service is running (we'll try to
// force it to dial in that case by adding the network address to its db.)
bool nsAutodial::ShouldDialOnNetworkError()
{
    // Don't try to dial again within a few seconds of when user pressed cancel.
    if (mDontRetryUntil) 
    {
        PRIntervalTime intervalNow = PR_IntervalNow();
        if (intervalNow < mDontRetryUntil) 
        {
            LOGD(("Autodial: Not dialing: too soon."));
            return false;
        }
    }
     

    return ((mAutodialBehavior == AUTODIAL_ALWAYS) 
             || (mAutodialBehavior == AUTODIAL_ON_NETWORKERROR)
             || (mAutodialBehavior == AUTODIAL_USE_SERVICE));
}


// The autodial info is set in Control Panel | Internet Options | Connections.
// The values are stored in the registry. This function gets those values from
// the registry and determines if we should never dial, always dial, or dial
// when there is no network found.
int nsAutodial::QueryAutodialBehavior()
{
    if (IsAutodialServiceRunning())
    {
        // Is Autodial service enabled for the current login session?
        DWORD disabled = 0;
        DWORD size = sizeof(DWORD);
        if (RasGetAutodialParamW(RASADP_LoginSessionDisable, &disabled, &size) == ERROR_SUCCESS)
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
    LONG result = ::RegOpenKeyExW(
                    HKEY_CURRENT_USER, 
                    L"Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings", 
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

    result = ::RegQueryValueExW(hKey, L"EnableAutodial", nsnull, &entryType, (LPBYTE)&autodial, &paramSize);
    if (result != ERROR_SUCCESS)
    {
        ::RegCloseKey(hKey);
        LOGE(("Autodial: Error reading reg value EnableAutodial."));
        return AUTODIAL_NEVER;
    }

    result = ::RegQueryValueExW(hKey, L"NoNetAutodial", nsnull, &entryType, (LPBYTE)&onDemand, &paramSize);
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
}

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
nsresult nsAutodial::DialDefault(const PRUnichar* hostName)
{
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
        // If a default dial entry is configured, use it.
        if (mDefaultEntryName[0] != '\0') 
        {
            LOGD(("Autodial: Dialing default: %s.",mDefaultEntryName));

            RASDIALDLG rasDialDlg;
            memset(&rasDialDlg, 0, sizeof(rasDialDlg));
            rasDialDlg.dwSize = sizeof(rasDialDlg);

            BOOL dialed = 
             RasDialDlgW(nsnull, mDefaultEntryName, nsnull, &rasDialDlg);

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
            memset(&rasPBDlg, 0, sizeof(rasPBDlg));
            rasPBDlg.dwSize = sizeof(rasPBDlg);
 
            BOOL dialed = RasPhonebookDlgW(nsnull, nsnull, &rasPBDlg);

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
}


// Check to see if RAS is already connected.
bool nsAutodial::IsRASConnected()
{
    DWORD connections;
    RASCONN rasConn;
    rasConn.dwSize = sizeof(rasConn);
    DWORD structSize = sizeof(rasConn);

    DWORD result = RasEnumConnectionsW(&rasConn, &structSize, &connections);

    // ERROR_BUFFER_TOO_SMALL is OK because we only need one struct.
    if (result == ERROR_SUCCESS || result == ERROR_BUFFER_TOO_SMALL)
    {
        return (connections > 0);
    }

    LOGE(("Autodial: ::RasEnumConnections failed: Error = %d", result));
    return false;
}

// Get the first RAS dial entry name from the phonebook.
nsresult nsAutodial::GetFirstEntryName(PRUnichar* entryName, int bufferSize)
{
    RASENTRYNAMEW rasEntryName;
    rasEntryName.dwSize = sizeof(rasEntryName);
    DWORD cb = sizeof(rasEntryName);
    DWORD cEntries = 0;

    DWORD result = 
     RasEnumEntriesW(nsnull, nsnull, &rasEntryName, &cb, &cEntries);

    // ERROR_BUFFER_TOO_SMALL is OK because we only need one struct.
    if (result == ERROR_SUCCESS || result == ERROR_BUFFER_TOO_SMALL)
    {
        wcsncpy(entryName, rasEntryName.szEntryName,
                bufferSize / sizeof(*entryName));
        return NS_OK;
    }

    return NS_ERROR_FAILURE;
}

// Get the number of RAS dial entries in the phonebook.
int nsAutodial::NumRASEntries()
{
    RASENTRYNAMEW rasEntryName;
    rasEntryName.dwSize = sizeof(rasEntryName);
    DWORD cb = sizeof(rasEntryName);
    DWORD cEntries = 0;


    DWORD result = 
     RasEnumEntriesW(nsnull, nsnull, &rasEntryName, &cb, &cEntries);

    // ERROR_BUFFER_TOO_SMALL is OK because we only need one struct.
    if (result == ERROR_SUCCESS || result == ERROR_BUFFER_TOO_SMALL)
    {
        return (int)cEntries;
    }

    return 0;
}

// Get the name of the default dial entry.
nsresult nsAutodial::GetDefaultEntryName(PRUnichar* entryName, int bufferSize)
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

    const PRUnichar* key = nsnull;
    const PRUnichar* val = nsnull;

    HKEY hKey = 0;
    LONG result = 0;

    // Windows 2000
    if ((mOSVerInfo.dwMajorVersion == 5) && (mOSVerInfo.dwMinorVersion == 0)) // Windows 2000
    {
        key = L"RemoteAccess";
        val = L"InternetProfile";

        result = ::RegOpenKeyExW(
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
        key = L"Software\\Microsoft\\RAS Autodial\\Default";
        val = L"DefaultInternet";

        
        // Try HKCU first.
        result = ::RegOpenKeyExW(
                    HKEY_CURRENT_USER, 
                    key, 
                    0, 
                    KEY_READ, 
                    &hKey);

        if (result != ERROR_SUCCESS)
        {
            // If not present, try HKLM.
            result = ::RegOpenKeyExW(
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

    result = ::RegQueryValueExW(hKey, 
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
bool nsAutodial::IsAutodialServiceRunning()
{
    SC_HANDLE hSCManager = 
      OpenSCManager(nsnull, SERVICES_ACTIVE_DATABASE, SERVICE_QUERY_STATUS);

    if (hSCManager == nsnull)
    {
        LOGE(("Autodial: failed to open service control manager. Error %d.", 
          ::GetLastError()));

        return false;
    }

    SC_HANDLE hService = 
      OpenServiceW(hSCManager, L"RasAuto", SERVICE_QUERY_STATUS);

    if (hSCManager == nsnull)
    {
        LOGE(("Autodial: failed to open RasAuto service."));
        return false;
    }

    SERVICE_STATUS status;
    if (!QueryServiceStatus(hService, &status))
    {
        LOGE(("Autodial: ::QueryServiceStatus() failed. Error: %d", 
          ::GetLastError()));

        return false;
    }

    return (status.dwCurrentState == SERVICE_RUNNING);
}

// Add the specified address to the autodial directory.
bool nsAutodial::AddAddressToAutodialDirectory(const PRUnichar* hostName)
{
    // First see if there is already a db entry for this address. 
    RASAUTODIALENTRYW autodialEntry;
    autodialEntry.dwSize = sizeof(autodialEntry);
    DWORD size = sizeof(autodialEntry);
    DWORD entries = 0;

    DWORD result = RasGetAutodialAddressW(hostName, 
                                          nsnull, 
                                          &autodialEntry, 
                                          &size, 
                                          &entries);

    // If there is already at least 1 entry in db for this address, return.
    if (result != ERROR_FILE_NOT_FOUND)
    {
        LOGD(("Autodial: Address %s already in autodial db.", hostName));
        return false;
    }

    autodialEntry.dwSize = sizeof(autodialEntry);
    autodialEntry.dwFlags = 0;
    autodialEntry.dwDialingLocation = mAutodialServiceDialingLocation;
    GetDefaultEntryName(autodialEntry.szEntry, sizeof(autodialEntry.szEntry));

    result = RasSetAutodialAddressW(hostName, 
                                    0, 
                                    &autodialEntry, 
                                    sizeof(autodialEntry), 
                                    1);

    if (result != ERROR_SUCCESS)
    {
        LOGE(("Autodial ::RasSetAutodialAddress failed result %d.", result));
        return false;
    }

    LOGD(("Autodial: Added address %s to RAS autodial db for entry %s.",
         hostName, NS_ConvertUTF16toUTF8(autodialEntry.szEntry).get()));

    return true;
}

// Get the current TAPI dialing location.
int nsAutodial::GetCurrentLocation()
{
    HKEY hKey = 0;
    LONG result = ::RegOpenKeyExW(
                    HKEY_LOCAL_MACHINE, 
                    L"Software\\Microsoft\\Windows\\CurrentVersion\\Telephony\\Locations", 
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

    result = ::RegQueryValueExW(hKey, L"CurrentID", nsnull, &entryType, (LPBYTE)&location, &paramSize);
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
bool nsAutodial::IsAutodialServiceEnabled(int location)
{
    if (location < 0)
        return false;

    BOOL enabled;
    if (RasGetAutodialEnableW(location, &enabled) != ERROR_SUCCESS)
    {
        LOGE(("Autodial: Error calling RasGetAutodialEnable()"));
        return false;
    }

    return enabled;
}

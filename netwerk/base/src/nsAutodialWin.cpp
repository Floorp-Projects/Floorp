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
#include <windows.h>
#include <winsvc.h>
#include "nsAutodialWin.h"
#include "prlog.h"

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
tRASENUMCONNECTIONS	nsRASAutodial::mpRasEnumConnections = nsnull;
tRASENUMENTRIES nsRASAutodial::mpRasEnumEntries = nsnull;
tRASDIALDLG nsRASAutodial::mpRasDialDlg = nsnull;
tRASSETAUTODIALADDRESS nsRASAutodial::mpRasSetAutodialAddress = nsnull;
tRASGETAUTODIALADDRESS nsRASAutodial::mpRasGetAutodialAddress = nsnull;
HINSTANCE nsRASAutodial::mhRASdlg = nsnull;
HINSTANCE nsRASAutodial::mhRASapi32 = nsnull;

// ctor. 
nsRASAutodial::nsRASAutodial()
:   mAutodialBehavior(AUTODIAL_NEVER),
    mNumRASConnectionEntries(0)
{
    mOSVerInfo.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
    GetVersionEx(&mOSVerInfo);

    // We only need to dial on nt based systems. For all other platforms, 
    // mAutodialBehavior will remain AUTODIAL_NEVER, and we can skip
    // these initializations.
    if ((mOSVerInfo.dwPlatformId == VER_PLATFORM_WIN32_NT) 
     && (mOSVerInfo.dwMajorVersion >= 4))
    {
        // Initializations that can be made again since RAS OS settings can 
        // change.
        Init();
    }
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

    // Don't need to load this DLL before now.
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
        }

        if (!mhRASapi32 
            || !mpRasEnumConnections 
            || !mpRasEnumEntries 
            || !mpRasSetAutodialAddress
            || !mpRasGetAutodialAddress)
        {
            LOGE(("Autodial: Error loading RASAPI32.DLL."));
        }
    }

    // Get the number of dialup entries in the phonebook.
    mNumRASConnectionEntries = NumRASEntries();
    
    // Get the name of the default entry.
    nsresult result = GetDefaultEntryName(mDefaultEntryName, 
                                           RAS_MaxEntryName + 1);
    
    return result;
}


// Should we attempt to dial on a network error? Yes if the Internet Options
// configured as such. Yes if the RAS autodial service is running (we'll 
// force it to dail in that case by adding the network address to its db.)
PRBool nsRASAutodial::ShouldDialOnNetworkError()
{
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
}


// The autodial info is set in Control Panel | Internet Options | Connections.
// The values are stored in the registry. This function gets those values from
// the registry and determines if we should never dial, always dial, or dial
// when there is no network found.
int nsRASAutodial::QueryAutodialBehavior()
{
    if (IsAutodialServiceRunning())
        return AUTODIAL_USE_SERVICE;

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
}

// If the RAS autodial service is running, use it. Otherwise, dial
// the default RAS connection. There are two possible RAS dialogs:
// one that dials a single entry, and one that lets the user choose which
// to dial. If there is only one connection entry in the phone book, or
// there are multiple entries but one is defined as the default, we'll use
// the single entry dial dialog. If there are multiple connection entries,
// and none is specified as default, we'll bring up the diallog which lets
// the user select the connection entry to use.
nsresult nsRASAutodial::DialDefault(const char* hostName)
{
    mDontRetryUntil = 0;

    if (mAutodialBehavior == AUTODIAL_NEVER)
    {
        return NS_ERROR_FAILURE;
    }

    // If already a RAS connection, bail.
    if (IsRASConnected())
    {
        LOGD(("Autodial: Not dialing: active connection."));
        return NS_OK; //NS_ERROR_FAILURE;
    }

    // If no dialup connections configured, bail.
    if (mNumRASConnectionEntries <= 0)
    {
        LOGD(("Autodial: Not dialing: no entries."));
        return NS_ERROR_FAILURE;
    }


    // If autodial service is running, let it dial. In order for it to dial 
    // reliably, we have to add the target address to the autodial database.
    // This is the only way the autodial service dial if there is a network
    // adapter installed. 
    // See http://msdn.microsoft.com/library/default.asp?url=/library/en-us/rras/ras4over_3dwl.asp
    if (mAutodialBehavior == AUTODIAL_USE_SERVICE)
    {
        if (!AddAddressToAutodialDirectory(hostName))
            return NS_ERROR_FAILURE;
    }

    // Do the dialing ourselves.
    else
    {
        // Don't need to load the dll before this.
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

            if (!mhRASdlg || !mpRasPhonebookDlg || !mpRasDialDlg)
            {
                LOGE(("Autodial: Error loading RASDLG.DLL."));
            }
        }


        // If a default dial entry is configured, use it.
        if (mDefaultEntryName[0] != '\0') 
        {
            LOGD(("Autodial: Dialing default: %s.",mDefaultEntryName));

            RASDIALDLG rasDialDlg;
            memset(&rasDialDlg, 0, sizeof(RASDIALDLG));
            rasDialDlg.dwSize = sizeof(RASDIALDLG);

            NS_ASSERTION(mpRasDialDlg != nsnull, 
             "RAS DLLs only loaded for NT-based OSs.");

            if (!mpRasDialDlg)
            {
                return NS_ERROR_NULL_POINTER;
            }

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
                return NS_ERROR_FAILURE;
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
 
            NS_ASSERTION(mpRasPhonebookDlg != nsnull, 
             "RAS DLLs only loaded for NT-based OSs.");

            if (!mpRasPhonebookDlg)
            {
                return NS_ERROR_NULL_POINTER;
            }

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

                return NS_ERROR_FAILURE;
            }

            LOGD(("Autodial: RAS dialup connection successful."));
        }
    }

    return NS_OK;

}

// Check to see if RAS is already connected.
PRBool nsRASAutodial::IsRASConnected()
{
    DWORD connections;
    RASCONN rasConn;
    rasConn.dwSize = sizeof(RASCONN);
    DWORD structSize = sizeof(RASCONN);

    NS_ASSERTION(mpRasEnumConnections != nsnull, 
     "RAS DLLs only loaded for NT-based OSs.");

    if (!mpRasEnumConnections)
    {
        return NS_ERROR_NULL_POINTER;
    }

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
    RASENTRYNAME rasEntryName;
    rasEntryName.dwSize = sizeof(RASENTRYNAME);
    DWORD cb = sizeof(RASENTRYNAME);
    DWORD cEntries = 0;

    NS_ASSERTION(mpRasEnumEntries != nsnull, 
     "RAS DLLs only loaded for NT-based OSs.");

    if (!mpRasEnumEntries)
    {
        return NS_ERROR_NULL_POINTER;
    }

    DWORD result = 
     (*mpRasEnumEntries)(nsnull, nsnull, &rasEntryName, &cb, &cEntries);

    // ERROR_BUFFER_TOO_SMALL is OK because we only need one struct.
    if (result == ERROR_SUCCESS || result == ERROR_BUFFER_TOO_SMALL)
    {
        strncpy(entryName, rasEntryName.szEntryName, bufferSize);
        return NS_OK;
    }

    return NS_ERROR_FAILURE;
}

// Get the number of RAS dial entries in the phonebook.
int nsRASAutodial::NumRASEntries()
{
    RASENTRYNAME rasEntryName;
    rasEntryName.dwSize = sizeof(RASENTRYNAME);
    DWORD cb = sizeof(RASENTRYNAME);
    DWORD cEntries = 0;

    NS_ASSERTION(mpRasEnumEntries != nsnull, 
     "RAS DLLs only loaded for NT-based OSs.");

    if (!mpRasEnumEntries)
    {
        return 0;
    }

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

    // Single RAS dialup entry. Use it as the default to autodail.
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
}

// Add the specified address to the autodial directory.
PRBool nsRASAutodial::AddAddressToAutodialDirectory(const char* hostName)
{
    NS_ASSERTION(mpRasGetAutodialAddress != nsnull, 
     "RAS DLLs only loaded for NT-based OSs.");

    NS_ASSERTION(mpRasSetAutodialAddress != nsnull, 
     "RAS DLLs only loaded for NT-based OSs.");

    if (!mpRasGetAutodialAddress || !mpRasSetAutodialAddress)
    {
        return PR_FALSE;
    }

    // First see if there is already a db entry for this address. 
    DWORD size = 0;
    DWORD entries = 0;

    DWORD result = (*mpRasGetAutodialAddress)(hostName, 
                                    NULL, 
                                    NULL, 
                                    &size, 
                                    &entries);

    // If there is already an entry in db for this address, return.
    if (size != 0)
    {
        LOGD(("Autodial: Address %s already in autodial db.", hostName));
        return PR_FALSE;
    }

    RASAUTODIALENTRY autodialEntry;
    autodialEntry.dwSize = sizeof(RASAUTODIALENTRY);
    autodialEntry.dwFlags = 0;
    autodialEntry.dwDialingLocation = 1;
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

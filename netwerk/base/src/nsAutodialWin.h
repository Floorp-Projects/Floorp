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

#ifndef nsAutodialWin_h__
#define nsAutodialWin_h__

#include <windows.h>
#include <ras.h>
#include <rasdlg.h>
#include <raserror.h>
#include "nscore.h"
#include "nspr.h"

// For Windows NT 4, 2000, and XP, we sometimes want to open the RAS dialup 
// window ourselves, since those versions aren't very nice about it. 
// See bug 93002. If the RAS autodial service is running, (Remote Access 
// Auto Connection Manager, aka RasAuto) we will force it to dial
// on network error by adding the target address to the autodial
// database. If the service is not running, we will open the RAS dialogs
// instead.
//
// The OS can be configured to always dial, or dial when there is no 
// connection. We implement both by dialing when a network error occurs.
//
// An object of this class checks with the OS when it is constructed and 
// remembers those settings. If required, it can be resynced with
// the OS at anytime with the Init() method. At the time of implementation,
// the caller creates an object of this class each time a network error occurs. 
// In this case, the initialization overhead is not significant, and it will
// guaranteed to be in sync with OS.
//
// To use, create an instance and call ShouldDialOnNetworkError() to determine 
// if you should dial or not. That function only return true for the
// target OS's so the caller doesn't have to deal with OS version checking.
//

class nsAutodial
{
private:

    //
    // Some helper functions to query the OS for autodial configuration.
    //

    // Determine if the autodial service is running on this PC.
    bool IsAutodialServiceRunning();

    // Get the number of RAS connection entries configured from the OS.
    int NumRASEntries();

    // Get the name of the default connection from the OS.
    nsresult GetDefaultEntryName(PRUnichar* entryName, int bufferSize);

    // Get the name of the first RAS dial entry from the OS.
    nsresult GetFirstEntryName(PRUnichar* entryName, int bufferSize);

    // Check to see if RAS already has a dialup connection going.
    bool IsRASConnected();

    // Get the autodial behavior from the OS.
    int QueryAutodialBehavior();

    // Add the specified address to the autodial directory.
    bool AddAddressToAutodialDirectory(const PRUnichar* hostName);

    // Get the  current TAPI dialing location.
    int GetCurrentLocation();

    // See if autodial is enabled for specified location.
    bool IsAutodialServiceEnabled(int location);

    //
    // Autodial behavior. This comes from the Windows registry, set in the ctor. 
    // Object won't pick up changes to the registry automatically, but can be 
    // refreshed at anytime by calling Init(). So if the user changed the 
    // autodial settings, they wouldn't be noticed unless Init() is called.
    int mAutodialBehavior;

    int mAutodialServiceDialingLocation;

    enum { AUTODIAL_NEVER = 1 };            // Never autodial.
    enum { AUTODIAL_ALWAYS = 2 };           // Always autodial as set in Internet Options.
    enum { AUTODIAL_ON_NETWORKERROR = 3 };  // Autodial when a connection error occurs as set in Internet Options.
    enum { AUTODIAL_USE_SERVICE = 4 };      // Use the RAS autodial service to dial.

    // Number of RAS connection entries in the phonebook. 
    int mNumRASConnectionEntries;

    // Default connection entry name.
    PRUnichar mDefaultEntryName[RAS_MaxEntryName + 1];  

    // Don't try to dial again within a few seconds of when user pressed cancel.
    static PRIntervalTime mDontRetryUntil;

    // OS version info.
    OSVERSIONINFO mOSVerInfo;

public:
  
    // ctor
    nsAutodial();

    // dtor
    virtual ~nsAutodial();

    // Get the autodial info from the OS and init this obj with it. Call it any
    // time to refresh the object's settings from the OS.
    nsresult Init();

    // Dial the default RAS dialup connection.
    nsresult DialDefault(const PRUnichar* hostName);

    // Should we try to dial on network error?
    bool ShouldDialOnNetworkError();
};

#endif // !nsAutodialWin_h__


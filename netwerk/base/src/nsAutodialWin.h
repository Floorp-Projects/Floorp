/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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


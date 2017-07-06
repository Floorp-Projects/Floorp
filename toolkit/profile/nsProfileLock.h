/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __nsProfileLock_h___
#define __nsProfileLock_h___

#include "nsIFile.h"

class nsIProfileUnlocker;

#if defined (XP_WIN)
#include <windows.h>
#endif

#if defined (XP_UNIX)
#include <signal.h>
#include "prclist.h"
#endif

class nsProfileLock
#if defined (XP_UNIX)
  : public PRCList
#endif
{
public:
                            nsProfileLock();
                            nsProfileLock(nsProfileLock& src);

                            ~nsProfileLock();

    nsProfileLock&          operator=(nsProfileLock& rhs);

    /**
     * Attempt to lock a profile directory.
     *
     * @param aProfileDir  [in] The profile directory to lock.
     * @param aUnlocker    [out] Optional. This is only returned when locking
     *                     fails with NS_ERROR_FILE_ACCESS_DENIED, and may not
     *                     be returned at all.
     * @throws NS_ERROR_FILE_ACCESS_DENIED if the profile is locked.
     */
    nsresult                Lock(nsIFile* aProfileDir, nsIProfileUnlocker* *aUnlocker);

    /**
     * Unlock a profile directory.  If you're unlocking the directory because
     * the application is in the process of shutting down because of a fatal
     * signal, set aFatalSignal to true.
     */
    nsresult                Unlock(bool aFatalSignal = false);

    /**
     * Clean up any left over files in the directory.
     */
    nsresult                Cleanup();

    /**
     * Get the modification time of a replaced profile lock, otherwise 0.
     */
    nsresult                GetReplacedLockTime(PRTime* aResult);

private:
    bool                    mHaveLock;
    PRTime                  mReplacedLockTime;
    nsCOMPtr<nsIFile>       mLockFile;

#if defined (XP_WIN)
    HANDLE                  mLockFileHandle;
#elif defined (XP_UNIX)

    struct RemovePidLockFilesExiting {
        RemovePidLockFilesExiting() {}
        ~RemovePidLockFilesExiting() {
            RemovePidLockFiles(false);
        }
    };

    static void             RemovePidLockFiles(bool aFatalSignal);
    static void             FatalSignalHandler(int signo
#ifdef SA_SIGINFO
                                               , siginfo_t *info, void *context
#endif
                                               );
    static PRCList          mPidLockList;

    nsresult                LockWithFcntl(nsIFile *aLockFile);

    /**
     * @param aHaveFcntlLock if true, we've already acquired an fcntl lock so this
     * lock is merely an "obsolete" lock to keep out old Firefoxes
     */
    nsresult                LockWithSymlink(nsIFile *aLockFile, bool aHaveFcntlLock);

    char*                   mPidLockFileName;
    int                     mLockFileDesc;
#endif

};

#endif /* __nsProfileLock_h___ */

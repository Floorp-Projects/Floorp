/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Conrad Carlen <ccarlen@netscape.com>
 *   Brendan Eich <brendan@mozilla.org>
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

#ifndef __nsProfileLock_h___
#define __nsProfileLock_h___

#include "nsILocalFile.h"

#if defined (XP_WIN)
#include <windows.h>
#endif

#if defined (XP_OS2)
#define INCL_DOSERRORS
#define INCL_DOSFILEMGR
#include <os2.h>
#endif

#if defined (XP_UNIX)
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
                       
    nsresult                Lock(nsILocalFile* aFile);
    nsresult                Unlock();
        
private:
    PRPackedBool            mHaveLock;

#if defined (XP_WIN)
    HANDLE                  mLockFileHandle;
#elif defined (XP_OS2)
    LHANDLE                 mLockFileHandle;
#elif defined (XP_UNIX)
    static void             RemovePidLockFiles();
    static void             FatalSignalHandler(int signo);
    static PRCList          mPidLockList;

    nsresult                LockWithFcntl(const nsACString& lockFilePath);
    nsresult                LockWithSymlink(const nsACString& lockFilePath);

    char*                   mPidLockFileName;
    int                     mLockFileDesc;
#endif

};

#endif /* __nsProfileLock_h___ */

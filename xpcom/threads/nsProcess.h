/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998-1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Don Bragg <dbragg@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#ifndef _nsPROCESSWIN_H_
#define _nsPROCESSWIN_H_

#if defined(XP_WIN) && !defined (WINCE) /* wince uses nspr */
#define PROCESSMODEL_WINAPI
#endif

#include "nsIProcess.h"
#include "nsIFile.h"
#include "nsIThread.h"
#include "nsIObserver.h"
#include "nsIWeakReference.h"
#include "nsIWeakReferenceUtils.h"
#include "nsIObserver.h"
#include "nsString.h"
#include "prproces.h"
#if defined(PROCESSMODEL_WINAPI) 
#include <windows.h>
#include <shellapi.h>
#endif

#define NS_PROCESS_CID \
{0x7b4eeb20, 0xd781, 0x11d4, \
   {0x8A, 0x83, 0x00, 0x10, 0xa4, 0xe0, 0xc9, 0xca}}

class nsProcess : public nsIProcess,
                  public nsIObserver
{
public:

  NS_DECL_ISUPPORTS
  NS_DECL_NSIPROCESS
  NS_DECL_NSIOBSERVER

  nsProcess();

private:
  ~nsProcess();
  static void PR_CALLBACK Monitor(void *arg);
  void ProcessComplete();
  NS_IMETHOD RunProcess(PRBool blocking, const char **args, PRUint32 count,
                        nsIObserver* observer, PRBool holdWeak);

  PRThread* mThread;
  PRLock* mLock;
  PRBool mShutdown;

  nsCOMPtr<nsIFile> mExecutable;
  nsCString mTargetPath;
  PRInt32 mPid;
  nsCOMPtr<nsIObserver> mObserver;
  nsWeakPtr mWeakObserver;

  // These members are modified by multiple threads, any accesses should be
  // protected with mLock.
  PRInt32 mExitValue;
#if defined(PROCESSMODEL_WINAPI) 
  typedef DWORD (WINAPI*GetProcessIdPtr)(HANDLE process);
  HANDLE mProcess;
#else
  PRProcess *mProcess;
#endif
};

#endif

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
 * The Original Code is Mozilla Communicator.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Kai Engert <kaie@netscape.com>
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

#include "nsPSMTracker.h"
#include "nsCOMPtr.h"

nsPSMTracker *nsPSMTracker::singleton = nsnull;

nsPSMTracker *nsPSMTracker::construct()
{
  if (singleton) {
    // we should never ever be called twice
    return nsnull;
  }

  singleton = new nsPSMTracker();
  return singleton;
}

nsPSMTracker::nsPSMTracker()
{
  mLock = PR_NewLock();
  mActiveSSLSockets = 0;
  mActiveUIContexts = 0;
  mIsUIForbidden = PR_FALSE;
}

nsPSMTracker::~nsPSMTracker()
{
  if (mLock) {
    PR_DestroyLock(mLock);
  }
  PR_ASSERT(this == singleton);
  singleton = nsnull;
}

void nsPSMTracker::enterUIContext()
{
  if (!singleton)
    return;
  
  PR_Lock(singleton->mLock);
    ++singleton->mActiveUIContexts;
  PR_Unlock(singleton->mLock);
}

void nsPSMTracker::leaveUIContext()
{
  if (!singleton)
    return;
  
  PR_Lock(singleton->mLock);
    --singleton->mActiveUIContexts;
  PR_Unlock(singleton->mLock);
}

PRBool nsPSMTracker::isUIActive()
{
  if (!singleton) {
    // be pessimistic
    return PR_TRUE;
  }
  
  PRBool retval;
  PR_Lock(singleton->mLock);
    retval = (singleton->mActiveUIContexts > 0);
  PR_Unlock(singleton->mLock);
  
  return retval;
}

PRBool nsPSMTracker::ifPossibleDisallowUI()
{
  if (!singleton) {
    // be pessimistic
    return PR_FALSE;
  }
  
  PRBool retval;
  PR_Lock(singleton->mLock);
    if (singleton->mActiveUIContexts > 0) {
      retval = PR_FALSE;
    }
    else {
      retval = PR_TRUE;
      singleton->mIsUIForbidden = PR_TRUE;
    }
  PR_Unlock(singleton->mLock);
  
  return retval;
}

void nsPSMTracker::allowUI()
{
  if (!singleton)
    return;
  
  PR_Lock(singleton->mLock);
    singleton->mIsUIForbidden = PR_FALSE;
  PR_Unlock(singleton->mLock);
}

PRBool nsPSMTracker::isUIForbidden()
{
  if (!singleton) {
    // be pessimistic
    return PR_FALSE;
  }
  
  PRBool retval;
  PR_Lock(singleton->mLock);
    retval = singleton->mIsUIForbidden;
  PR_Unlock(singleton->mLock);
  
  return retval;
}

void nsPSMTracker::increaseSSLSocketCounter()
{
  if (!singleton)
    return;
  
  PR_Lock(singleton->mLock);
    ++singleton->mActiveSSLSockets;
  PR_Unlock(singleton->mLock);
}

void nsPSMTracker::decreaseSSLSocketCounter()
{
  if (!singleton)
    return;
  
  PR_Lock(singleton->mLock);
    --singleton->mActiveSSLSockets;
  PR_Unlock(singleton->mLock);
}

PRBool nsPSMTracker::areSSLSocketsActive()
{
  if (!singleton) {
    // be pessimistic
    return PR_TRUE;
  }
  
  PRBool retval;
  PR_Lock(singleton->mLock);
    retval = (singleton->mActiveSSLSockets > 0);
  PR_Unlock(singleton->mLock);
  
  return retval;
}

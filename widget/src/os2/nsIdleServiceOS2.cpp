/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 sts=2 et cin: */
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
 * The Original Code is the Mozilla OS/2 widget code.
 *
 * The Initial Developer of the Original Code is
 * Peter Weilbacher <mozilla@Weilbacher.org>.
 * Portions created by the Initial Developer are Copyright (C) 2008
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
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsIdleServiceOS2.h"

// prototype for funtion imported from DSSaver
static int (*_System DSSaver_GetInactivityTime)(ULONG *, ULONG *);
#define SSCORE_NOERROR 0 // as in the DSSaver header files

NS_IMPL_ISUPPORTS1(nsIdleServiceOS2, nsIIdleService)

nsIdleServiceOS2::nsIdleServiceOS2()
  : mHMod(NULLHANDLE), mInitialized(PR_FALSE)
{
  const char error[256] = "";
  if (DosLoadModule(error, 256, "SSCORE", &mHMod) == NO_ERROR) {
    if (DosQueryProcAddr(mHMod, 0, "SSCore_GetInactivityTime",
                         (PFN*)&DSSaver_GetInactivityTime) == NO_ERROR) {
      mInitialized = PR_TRUE;
    }
  }
}

nsIdleServiceOS2::~nsIdleServiceOS2()
{
  if (mHMod != NULLHANDLE) {
    DosFreeModule(mHMod);
  }
}

NS_IMETHODIMP
nsIdleServiceOS2::GetIdleTime(PRUint32 *aIdleTime)
{
  if (!mInitialized)
    return NS_ERROR_NOT_INITIALIZED;

  ULONG mouse, keyboard;
  if (DSSaver_GetInactivityTime(&mouse, &keyboard) != SSCORE_NOERROR) {
    return NS_ERROR_FAILURE;
  }

  // we are only interested in activity in general, so take the minimum
  // of both timers
  *aIdleTime = PR_MIN(mouse, keyboard);
  return NS_OK;
}

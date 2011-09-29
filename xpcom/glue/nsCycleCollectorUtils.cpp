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
 * The Original Code is Cycle Collector.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
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

#include "nsCycleCollectorUtils.h"

#include "prthread.h"
#include "nsCOMPtr.h"
#include "nsServiceManagerUtils.h"
#include "nsXPCOMCIDInternal.h"

#include "nsIThreadManager.h"

#if defined(XP_WIN)
#include "windows.h"
#endif

#ifndef MOZILLA_INTERNAL_API

bool
NS_IsCycleCollectorThread()
{
  bool result = false;
  nsCOMPtr<nsIThreadManager> mgr =
    do_GetService(NS_THREADMANAGER_CONTRACTID);
  if (mgr)
    mgr->GetIsCycleCollectorThread(&result);
  return bool(result);
}

#elif defined(XP_WIN)

// Defined in nsThreadManager.cpp.
extern DWORD gTLSThreadIDIndex;

bool
NS_IsCycleCollectorThread()
{
  return TlsGetValue(gTLSThreadIDIndex) ==
    (void*) mozilla::threads::CycleCollector;
}

#elif !defined(NS_TLS)

// Defined in nsCycleCollector.cpp
extern PRThread* gCycleCollectorThread;

bool
NS_IsCycleCollectorThread()
{
  return PR_GetCurrentThread() == gCycleCollectorThread;
}

#endif

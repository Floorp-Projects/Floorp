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
 * The Initial Developer of the Original Code is IBM Corporation.
 * Portions created by IBM Corporation are Copyright (C) 2004
 * IBM Corporation. All Rights Reserved.
 *
 * Contributor(s):
 *   Darin Fisher <darin@meer.net>
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

#ifndef TestCommon_h__
#define TestCommon_h__

#include <stdlib.h>
#include "nsThreadUtils.h"

inline int test_common_init(int *argc, char ***argv)
{
  return 0;
}

//-----------------------------------------------------------------------------

static bool gKeepPumpingEvents = false;

class nsQuitPumpingEvent : public nsIRunnable {
public:
  NS_DECL_ISUPPORTS
  NS_IMETHOD Run() {
    gKeepPumpingEvents = PR_FALSE;
    return NS_OK;
  }
};
NS_IMPL_THREADSAFE_ISUPPORTS1(nsQuitPumpingEvent, nsIRunnable)

static inline void PumpEvents()
{
  nsCOMPtr<nsIThread> thread = do_GetCurrentThread();

  gKeepPumpingEvents = PR_TRUE;
  while (gKeepPumpingEvents)
    NS_ProcessNextEvent(thread);

  NS_ProcessPendingEvents(thread);
}

static inline void QuitPumpingEvents()
{
  // Dispatch a task that toggles gKeepPumpingEvents so that we flush all
  // of the pending tasks before exiting from PumpEvents.
  nsCOMPtr<nsIRunnable> event = new nsQuitPumpingEvent();
  NS_DispatchToMainThread(event);
}

#endif

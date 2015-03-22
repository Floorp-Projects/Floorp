/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TestCommon_h__
#define TestCommon_h__

#include <stdlib.h>
#include "nsThreadUtils.h"
#include "mozilla/Attributes.h"

inline int test_common_init(int *argc, char ***argv)
{
  return 0;
}

//-----------------------------------------------------------------------------

static bool gKeepPumpingEvents = false;

class nsQuitPumpingEvent final : public nsIRunnable {
  ~nsQuitPumpingEvent() {}
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_IMETHOD Run() override {
    gKeepPumpingEvents = false;
    return NS_OK;
  }
};
NS_IMPL_ISUPPORTS(nsQuitPumpingEvent, nsIRunnable)

static inline void PumpEvents()
{
  nsCOMPtr<nsIThread> thread = do_GetCurrentThread();

  gKeepPumpingEvents = true;
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

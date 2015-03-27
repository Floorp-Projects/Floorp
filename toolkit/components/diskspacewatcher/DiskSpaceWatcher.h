/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __DISKSPACEWATCHER_H__

#include "nsIDiskSpaceWatcher.h"
#include "nsIObserver.h"
#include "nsCOMPtr.h"

class DiskSpaceWatcher final : public nsIDiskSpaceWatcher,
                               public nsIObserver
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIDISKSPACEWATCHER
  NS_DECL_NSIOBSERVER

  static already_AddRefed<DiskSpaceWatcher>
  FactoryCreate();

  static void UpdateState(bool aIsDiskFull, uint64_t aFreeSpace);

private:
  DiskSpaceWatcher();
  ~DiskSpaceWatcher();

  static uint64_t sFreeSpace;
  static bool     sIsDiskFull;
};

#endif // __DISKSPACEWATCHER_H__

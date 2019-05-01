/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsToolkitCompsModule.h"

#include "nsUrlClassifierDBService.h"
#include "nsISupports.h"

#if defined(XP_WIN)
#  include "NativeFileWatcherWin.h"
#else
#  include "NativeFileWatcherNotSupported.h"

NS_IMPL_ISUPPORTS(mozilla::NativeFileWatcherService,
                  nsINativeFileWatcherService);
#endif  // (XP_WIN)

using namespace mozilla;

/////////////////////////////////////////////////////////////////////////////

nsresult nsUrlClassifierDBServiceConstructor(nsISupports* aOuter,
                                             const nsIID& aIID,
                                             void** aResult) {
  nsresult rv;
  NS_ENSURE_ARG_POINTER(aResult);
  NS_ENSURE_NO_AGGREGATION(aOuter);

  RefPtr<nsUrlClassifierDBService> inst =
      nsUrlClassifierDBService::GetInstance(&rv);
  if (!inst) {
    return rv;
  }

  return inst->QueryInterface(aIID, aResult);
}

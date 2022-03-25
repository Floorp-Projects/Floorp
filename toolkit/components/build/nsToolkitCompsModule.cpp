/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsToolkitCompsModule.h"

#include "nsUrlClassifierDBService.h"
#include "nsISupports.h"

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

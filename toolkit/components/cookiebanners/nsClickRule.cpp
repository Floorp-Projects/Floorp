/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsClickRule.h"

#include "nsString.h"

namespace mozilla {

NS_IMPL_ISUPPORTS(nsClickRule, nsIClickRule)

NS_IMETHODIMP
nsClickRule::GetPresence(nsACString& aPresence) {
  aPresence.Assign(mPresence);
  return NS_OK;
}

NS_IMETHODIMP
nsClickRule::GetHide(nsACString& aHide) {
  aHide.Assign(mHide);
  return NS_OK;
}

NS_IMETHODIMP
nsClickRule::GetOptOut(nsACString& aOptOut) {
  aOptOut.Assign(mOptOut);
  return NS_OK;
}

NS_IMETHODIMP
nsClickRule::GetOptIn(nsACString& aOptIn) {
  aOptIn.Assign(mOptIn);
  return NS_OK;
}

}  // namespace mozilla

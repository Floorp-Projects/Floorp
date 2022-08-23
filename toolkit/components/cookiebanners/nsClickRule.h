/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_nsclickrule_h__
#define mozilla_nsclickrule_h__

#include "nsIClickRule.h"
#include "nsString.h"

namespace mozilla {

class nsClickRule final : public nsIClickRule {
  NS_DECL_ISUPPORTS
  NS_DECL_NSICLICKRULE

  explicit nsClickRule(const nsACString& aPresence, const nsACString& aHide,
                       const nsACString& aOptOut, const nsACString& aOptIn)
      : mPresence(aPresence), mHide(aHide), mOptOut(aOptOut), mOptIn(aOptIn) {}

 private:
  ~nsClickRule() = default;

  nsCString mPresence;
  nsCString mHide;
  nsCString mOptOut;
  nsCString mOptIn;
};

}  // namespace mozilla

#endif  // mozilla_nsclickrule_h__

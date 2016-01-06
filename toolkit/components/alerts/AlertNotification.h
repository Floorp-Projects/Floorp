/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_AlertNotification_h__
#define mozilla_AlertNotification_h__

#include "nsIAlertsService.h"
#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsIPrincipal.h"
#include "nsString.h"

namespace mozilla {

class AlertNotification final : public nsIAlertNotification
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(AlertNotification,
                                           nsIAlertNotification)
  NS_DECL_NSIALERTNOTIFICATION
  AlertNotification();

protected:
  virtual ~AlertNotification();

private:
  nsString mName;
  nsString mImageURL;
  nsString mTitle;
  nsString mText;
  bool mTextClickable;
  nsString mCookie;
  nsString mDir;
  nsString mLang;
  nsString mData;
  nsCOMPtr<nsIPrincipal> mPrincipal;
  bool mInPrivateBrowsing;
};

} // namespace mozilla

#endif /* mozilla_AlertNotification_h__ */

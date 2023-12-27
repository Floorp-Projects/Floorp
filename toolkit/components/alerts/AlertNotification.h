/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_AlertNotification_h__
#define mozilla_AlertNotification_h__

#include "imgINotificationObserver.h"
#include "nsIAlertsService.h"
#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsICancelable.h"
#include "nsINamed.h"
#include "nsIPrincipal.h"
#include "nsString.h"
#include "nsITimer.h"

namespace mozilla {

class AlertImageRequest final : public imgINotificationObserver,
                                public nsICancelable,
                                public nsITimerCallback,
                                public nsINamed {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(AlertImageRequest,
                                           imgINotificationObserver)
  NS_DECL_IMGINOTIFICATIONOBSERVER
  NS_DECL_NSICANCELABLE
  NS_DECL_NSITIMERCALLBACK
  NS_DECL_NSINAMED

  AlertImageRequest(nsIURI* aURI, nsIPrincipal* aPrincipal,
                    bool aInPrivateBrowsing, uint32_t aTimeout,
                    nsIAlertNotificationImageListener* aListener,
                    nsISupports* aUserData);

  nsresult Start();

 private:
  virtual ~AlertImageRequest();

  nsresult NotifyMissing();
  void NotifyComplete();

  nsCOMPtr<nsIURI> mURI;
  nsCOMPtr<nsIPrincipal> mPrincipal;
  bool mInPrivateBrowsing;
  uint32_t mTimeout;
  nsCOMPtr<nsIAlertNotificationImageListener> mListener;
  nsCOMPtr<nsISupports> mUserData;
  nsCOMPtr<nsITimer> mTimer;
  nsCOMPtr<imgIRequest> mRequest;
};

class AlertNotification : public nsIAlertNotification {
 public:
  NS_DECL_ISUPPORTS
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
  bool mRequireInteraction;
  nsString mData;
  nsCOMPtr<nsIPrincipal> mPrincipal;
  bool mInPrivateBrowsing;
  bool mSilent;
  nsTArray<uint32_t> mVibrate;
  nsTArray<RefPtr<nsIAlertAction>> mActions;
  nsString mOpaqueRelaunchData;
};

}  // namespace mozilla

#endif /* mozilla_AlertNotification_h__ */

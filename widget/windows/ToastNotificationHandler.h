/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ToastNotificationHandler_h__
#define ToastNotificationHandler_h__

#include <windows.ui.notifications.h>
#include <windows.data.xml.dom.h>
#include <wrl.h>
#include "nsCOMPtr.h"
#include "nsIAlertsService.h"
#include "nsICancelable.h"
#include "nsIFile.h"
#include "nsString.h"

namespace mozilla {
namespace widget {

class ToastNotification;

class ToastNotificationHandler final
    : public nsIAlertNotificationImageListener {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIALERTNOTIFICATIONIMAGELISTENER

  ToastNotificationHandler(ToastNotification* backend,
                           nsIObserver* aAlertListener, const nsAString& aName,
                           const nsAString& aCookie, const nsAString& aTitle,
                           const nsAString& aMsg, const nsAString& aHostPort,
                           bool aClickable)
      : mBackend(backend),
        mHasImage(false),
        mAlertListener(aAlertListener),
        mName(aName),
        mCookie(aCookie),
        mTitle(aTitle),
        mMsg(aMsg),
        mHostPort(aHostPort),
        mClickable(aClickable) {}

  nsresult InitAlertAsync(nsIAlertNotification* aAlert);

  void OnWriteBitmapFinished(nsresult rv);

  void UnregisterHandler();

 protected:
  virtual ~ToastNotificationHandler();

  typedef ABI::Windows::Data::Xml::Dom::IXmlDocument IXmlDocument;
  typedef ABI::Windows::UI::Notifications::IToastNotifier IToastNotifier;
  typedef ABI::Windows::UI::Notifications::IToastNotification
      IToastNotification;
  typedef ABI::Windows::UI::Notifications::IToastDismissedEventArgs
      IToastDismissedEventArgs;
  typedef ABI::Windows::UI::Notifications::IToastFailedEventArgs
      IToastFailedEventArgs;
  typedef ABI::Windows::UI::Notifications::ToastTemplateType ToastTemplateType;

  Microsoft::WRL::ComPtr<IToastNotification> mNotification;
  Microsoft::WRL::ComPtr<IToastNotifier> mNotifier;

  RefPtr<ToastNotification> mBackend;

  nsCOMPtr<nsICancelable> mImageRequest;
  nsCOMPtr<nsIFile> mImageFile;
  nsString mImageUri;
  bool mHasImage;

  EventRegistrationToken mActivatedToken;
  EventRegistrationToken mDismissedToken;
  EventRegistrationToken mFailedToken;

  nsCOMPtr<nsIObserver> mAlertListener;
  nsString mName;
  nsString mCookie;
  nsString mTitle;
  nsString mMsg;
  nsString mHostPort;
  bool mClickable;

  nsresult TryShowAlert();
  bool ShowAlert();
  nsresult AsyncSaveImage(imgIRequest* aRequest);
  nsresult OnWriteBitmapSuccess();

  bool CreateWindowsNotificationFromXml(IXmlDocument* aToastXml);
  Microsoft::WRL::ComPtr<IXmlDocument> InitializeXmlForTemplate(
      ToastTemplateType templateType);

  HRESULT OnActivate(IToastNotification* notification,
                     IInspectable* inspectable);
  HRESULT OnDismiss(IToastNotification* notification,
                    IToastDismissedEventArgs* aArgs);
  HRESULT OnFail(IToastNotification* notification,
                 IToastFailedEventArgs* aArgs);
};

}  // namespace widget
}  // namespace mozilla

#endif

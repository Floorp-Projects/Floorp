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
#include "mozilla/Result.h"

namespace mozilla {
namespace widget {

class ToastNotification;

class ToastNotificationHandler final
    : public nsIAlertNotificationImageListener {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIALERTNOTIFICATIONIMAGELISTENER

  ToastNotificationHandler(ToastNotification* backend, const nsAString& aumid,
                           nsIObserver* aAlertListener, const nsAString& aName,
                           const nsAString& aCookie, const nsAString& aTitle,
                           const nsAString& aMsg, const nsAString& aHostPort,
                           bool aClickable, bool aRequireInteraction,
                           const nsTArray<RefPtr<nsIAlertAction>>& aActions,
                           bool aIsSystemPrincipal, const nsAString& aLaunchUrl,
                           bool aInPrivateBrowsing)
      : mBackend(backend),
        mAumid(aumid),
        mHasImage(false),
        mAlertListener(aAlertListener),
        mName(aName),
        mCookie(aCookie),
        mTitle(aTitle),
        mMsg(aMsg),
        mHostPort(aHostPort),
        mClickable(aClickable),
        mRequireInteraction(aRequireInteraction),
        mInPrivateBrowsing(aInPrivateBrowsing),
        mActions(aActions.Clone()),
        mIsSystemPrincipal(aIsSystemPrincipal),
        mLaunchUrl(aLaunchUrl),
        mSentFinished(!aAlertListener) {}

  nsresult InitAlertAsync(nsIAlertNotification* aAlert);

  void OnWriteBitmapFinished(nsresult rv);

  void HideIfPrivate();

  void UnregisterHandler();

  nsresult CreateToastXmlString(const nsAString& aImageURL, nsAString& aString);

  nsresult GetWindowsTag(nsAString& aWindowsTag);
  nsresult SetWindowsTag(const nsAString& aWindowsTag);

  // Exposed for consumption by `ToastNotification.cpp`.
  static nsresult FindLaunchURLAndPrivilegedNameForWindowsTag(
      const nsAString& aWindowsTag, const nsAString& aAumid, bool& aFoundTag,
      nsAString& aLaunchUrl, nsAString& aPrivilegedName);

 protected:
  virtual ~ToastNotificationHandler();

  using IXmlDocument = ABI::Windows::Data::Xml::Dom::IXmlDocument;
  using IToastNotifier = ABI::Windows::UI::Notifications::IToastNotifier;
  using IToastNotification =
      ABI::Windows::UI::Notifications::IToastNotification;
  using IToastDismissedEventArgs =
      ABI::Windows::UI::Notifications::IToastDismissedEventArgs;
  using IToastFailedEventArgs =
      ABI::Windows::UI::Notifications::IToastFailedEventArgs;
  using ToastTemplateType = ABI::Windows::UI::Notifications::ToastTemplateType;
  template <typename T>
  using ComPtr = Microsoft::WRL::ComPtr<T>;

  Result<nsString, nsresult> GetLaunchArgument();

  ComPtr<IToastNotification> mNotification;
  ComPtr<IToastNotifier> mNotifier;

  RefPtr<ToastNotification> mBackend;

  nsString mAumid;
  nsString mWindowsTag;

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
  bool mRequireInteraction;
  bool mInPrivateBrowsing;
  nsTArray<RefPtr<nsIAlertAction>> mActions;
  bool mIsSystemPrincipal;
  nsString mLaunchUrl;
  bool mSentFinished;

  nsresult TryShowAlert();
  bool ShowAlert();
  void HideAlert();
  nsresult AsyncSaveImage(imgIRequest* aRequest);
  nsresult OnWriteBitmapSuccess();
  void SendFinished();

  nsresult InitWindowsTag();
  bool CreateWindowsNotificationFromXml(ComPtr<IXmlDocument>& aToastXml);
  ComPtr<IXmlDocument> CreateToastXmlDocument();

  HRESULT OnActivate(const ComPtr<IToastNotification>& notification,
                     const ComPtr<IInspectable>& inspectable);
  HRESULT OnDismiss(const ComPtr<IToastNotification>& notification,
                    const ComPtr<IToastDismissedEventArgs>& aArgs);
  HRESULT OnFail(const ComPtr<IToastNotification>& notification,
                 const ComPtr<IToastFailedEventArgs>& aArgs);

  static HRESULT GetLaunchArgumentValueForKey(
      const ComPtr<IToastNotification> toast, const nsAString& key,
      nsAString& value);
  static ComPtr<IToastNotification> FindNotificationByTag(
      const nsAString& aWindowsTag, const nsAString& nsAumid);
};

}  // namespace widget
}  // namespace mozilla

#endif

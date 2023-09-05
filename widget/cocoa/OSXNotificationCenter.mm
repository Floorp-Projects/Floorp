/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "OSXNotificationCenter.h"
#import <AppKit/AppKit.h>
#include "imgIRequest.h"
#include "imgIContainer.h"
#include "nsICancelable.h"
#include "nsIStringBundle.h"
#include "nsNetUtil.h"
#import "nsCocoaUtils.h"
#include "nsComponentManagerUtils.h"
#include "nsContentUtils.h"
#include "nsObjCExceptions.h"
#include "nsString.h"
#include "nsCOMPtr.h"
#include "nsIObserver.h"

using namespace mozilla;

#define MAX_NOTIFICATION_NAME_LEN 5000

@protocol FakeNSUserNotification <NSObject>
@property(copy) NSString* title;
@property(copy) NSString* subtitle;
@property(copy) NSString* informativeText;
@property(copy) NSString* actionButtonTitle;
@property(copy) NSDictionary* userInfo;
@property(copy) NSDate* deliveryDate;
@property(copy) NSTimeZone* deliveryTimeZone;
@property(copy) NSDateComponents* deliveryRepeatInterval;
@property(readonly) NSDate* actualDeliveryDate;
@property(readonly, getter=isPresented) BOOL presented;
@property(readonly, getter=isRemote) BOOL remote;
@property(copy) NSString* soundName;
@property BOOL hasActionButton;
@property(readonly) NSUserNotificationActivationType activationType;
@property(copy) NSString* otherButtonTitle;
@property(copy) NSImage* contentImage;
@end

@protocol FakeNSUserNotificationCenter <NSObject>
+ (id<FakeNSUserNotificationCenter>)defaultUserNotificationCenter;
@property(assign) id<NSUserNotificationCenterDelegate> delegate;
@property(copy) NSArray* scheduledNotifications;
- (void)scheduleNotification:(id<FakeNSUserNotification>)notification;
- (void)removeScheduledNotification:(id<FakeNSUserNotification>)notification;
@property(readonly) NSArray* deliveredNotifications;
- (void)deliverNotification:(id<FakeNSUserNotification>)notification;
- (void)removeDeliveredNotification:(id<FakeNSUserNotification>)notification;
- (void)removeAllDeliveredNotifications;
- (void)_removeAllDisplayedNotifications;
- (void)_removeDisplayedNotification:(id<FakeNSUserNotification>)notification;
@end

@interface mozNotificationCenterDelegate
    : NSObject <NSUserNotificationCenterDelegate> {
  OSXNotificationCenter* mOSXNC;
}
- (id)initWithOSXNC:(OSXNotificationCenter*)osxnc;
@end

@implementation mozNotificationCenterDelegate

- (id)initWithOSXNC:(OSXNotificationCenter*)osxnc {
  [super init];
  // We should *never* outlive this OSXNotificationCenter.
  mOSXNC = osxnc;
  return self;
}

- (void)userNotificationCenter:(id<FakeNSUserNotificationCenter>)center
        didDeliverNotification:(id<FakeNSUserNotification>)notification {
}

- (void)userNotificationCenter:(id<FakeNSUserNotificationCenter>)center
       didActivateNotification:(id<FakeNSUserNotification>)notification {
  unsigned long long additionalActionIndex = ULLONG_MAX;
  if ([notification respondsToSelector:@selector(_alternateActionIndex)]) {
    NSNumber* alternateActionIndex =
        [(NSObject*)notification valueForKey:@"_alternateActionIndex"];
    additionalActionIndex = [alternateActionIndex unsignedLongLongValue];
  }
  mOSXNC->OnActivate([[notification userInfo] valueForKey:@"name"],
                     notification.activationType, additionalActionIndex);
}

- (BOOL)userNotificationCenter:(id<FakeNSUserNotificationCenter>)center
     shouldPresentNotification:(id<FakeNSUserNotification>)notification {
  return YES;
}

// This is an undocumented method that we need for parity with Safari.
// Apple bug #15440664.
- (void)userNotificationCenter:(id<FakeNSUserNotificationCenter>)center
    didRemoveDeliveredNotifications:(NSArray*)notifications {
  for (id<FakeNSUserNotification> notification in notifications) {
    NSString* name = [[notification userInfo] valueForKey:@"name"];
    mOSXNC->CloseAlertCocoaString(name);
  }
}

// This is an undocumented method that we need to be notified if a user clicks
// the close button.
- (void)userNotificationCenter:(id<FakeNSUserNotificationCenter>)center
               didDismissAlert:(id<FakeNSUserNotification>)notification {
  NSString* name = [[notification userInfo] valueForKey:@"name"];
  mOSXNC->CloseAlertCocoaString(name);
}

@end

namespace mozilla {

enum {
  OSXNotificationActionDisable = 0,
  OSXNotificationActionSettings = 1,
};

class OSXNotificationInfo final : public nsISupports {
 private:
  virtual ~OSXNotificationInfo();

 public:
  NS_DECL_ISUPPORTS
  OSXNotificationInfo(NSString* name, nsIObserver* observer,
                      const nsAString& alertCookie);

  NSString* mName;
  nsCOMPtr<nsIObserver> mObserver;
  nsString mCookie;
  RefPtr<nsICancelable> mIconRequest;
  id<FakeNSUserNotification> mPendingNotification;
};

NS_IMPL_ISUPPORTS0(OSXNotificationInfo)

OSXNotificationInfo::OSXNotificationInfo(NSString* name, nsIObserver* observer,
                                         const nsAString& alertCookie) {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  NS_ASSERTION(name, "Cannot create OSXNotificationInfo without a name!");
  mName = [name retain];
  mObserver = observer;
  mCookie = alertCookie;
  mPendingNotification = nil;

  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

OSXNotificationInfo::~OSXNotificationInfo() {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  [mName release];
  [mPendingNotification release];

  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

static id<FakeNSUserNotificationCenter> GetNotificationCenter() {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  Class c = NSClassFromString(@"NSUserNotificationCenter");
  return [c performSelector:@selector(defaultUserNotificationCenter)];

  NS_OBJC_END_TRY_BLOCK_RETURN(nil);
}

OSXNotificationCenter::OSXNotificationCenter() {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  mDelegate = [[mozNotificationCenterDelegate alloc] initWithOSXNC:this];
  GetNotificationCenter().delegate = mDelegate;
  mSuppressForScreenSharing = false;

  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

OSXNotificationCenter::~OSXNotificationCenter() {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  [GetNotificationCenter() removeAllDeliveredNotifications];
  [mDelegate release];

  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

NS_IMPL_ISUPPORTS(OSXNotificationCenter, nsIAlertsService, nsIAlertsIconData,
                  nsIAlertsDoNotDisturb, nsIAlertNotificationImageListener)

nsresult OSXNotificationCenter::Init() {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  return (!!NSClassFromString(@"NSUserNotification")) ? NS_OK
                                                      : NS_ERROR_FAILURE;

  NS_OBJC_END_TRY_BLOCK_RETURN(NS_ERROR_FAILURE);
}

NS_IMETHODIMP
OSXNotificationCenter::ShowAlertNotification(
    const nsAString& aImageUrl, const nsAString& aAlertTitle,
    const nsAString& aAlertText, bool aAlertTextClickable,
    const nsAString& aAlertCookie, nsIObserver* aAlertListener,
    const nsAString& aAlertName, const nsAString& aBidi, const nsAString& aLang,
    const nsAString& aData, nsIPrincipal* aPrincipal, bool aInPrivateBrowsing,
    bool aRequireInteraction) {
  nsCOMPtr<nsIAlertNotification> alert =
      do_CreateInstance(ALERT_NOTIFICATION_CONTRACTID);
  NS_ENSURE_TRUE(alert, NS_ERROR_FAILURE);
  // vibrate is unused for now
  nsTArray<uint32_t> vibrate;
  nsresult rv = alert->Init(aAlertName, aImageUrl, aAlertTitle, aAlertText,
                            aAlertTextClickable, aAlertCookie, aBidi, aLang,
                            aData, aPrincipal, aInPrivateBrowsing,
                            aRequireInteraction, false, vibrate);
  NS_ENSURE_SUCCESS(rv, rv);
  return ShowAlert(alert, aAlertListener);
}

NS_IMETHODIMP
OSXNotificationCenter::ShowPersistentNotification(
    const nsAString& aPersistentData, nsIAlertNotification* aAlert,
    nsIObserver* aAlertListener) {
  return ShowAlert(aAlert, aAlertListener);
}

NS_IMETHODIMP
OSXNotificationCenter::ShowAlert(nsIAlertNotification* aAlert,
                                 nsIObserver* aAlertListener) {
  return ShowAlertWithIconData(aAlert, aAlertListener, 0, nullptr);
}

NS_IMETHODIMP
OSXNotificationCenter::ShowAlertWithIconData(nsIAlertNotification* aAlert,
                                             nsIObserver* aAlertListener,
                                             uint32_t aIconSize,
                                             const uint8_t* aIconData) {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  NS_ENSURE_ARG(aAlert);

  if (mSuppressForScreenSharing) {
    return NS_OK;
  }

  Class unClass = NSClassFromString(@"NSUserNotification");
  id<FakeNSUserNotification> notification = [[unClass alloc] init];

  nsAutoString title;
  nsresult rv = aAlert->GetTitle(title);
  NS_ENSURE_SUCCESS(rv, rv);
  notification.title = nsCocoaUtils::ToNSString(title);

  nsAutoString hostPort;
  rv = aAlert->GetSource(hostPort);
  NS_ENSURE_SUCCESS(rv, rv);
  nsCOMPtr<nsIStringBundle> bundle;
  nsCOMPtr<nsIStringBundleService> sbs =
      do_GetService(NS_STRINGBUNDLE_CONTRACTID);
  sbs->CreateBundle("chrome://alerts/locale/alert.properties",
                    getter_AddRefs(bundle));

  if (!hostPort.IsEmpty() && bundle) {
    AutoTArray<nsString, 1> formatStrings = {hostPort};
    nsAutoString notificationSource;
    bundle->FormatStringFromName("source.label", formatStrings,
                                 notificationSource);
    notification.subtitle = nsCocoaUtils::ToNSString(notificationSource);
  }

  nsAutoString text;
  rv = aAlert->GetText(text);
  NS_ENSURE_SUCCESS(rv, rv);
  notification.informativeText = nsCocoaUtils::ToNSString(text);

  bool isSilent;
  aAlert->GetSilent(&isSilent);
  notification.soundName = isSilent ? nil : NSUserNotificationDefaultSoundName;
  notification.hasActionButton = NO;

  // If this is not an application/extension alert, show additional actions
  // dealing with permissions.
  bool isActionable;
  if (bundle && NS_SUCCEEDED(aAlert->GetActionable(&isActionable)) &&
      isActionable) {
    nsAutoString closeButtonTitle, actionButtonTitle, disableButtonTitle,
        settingsButtonTitle;
    bundle->GetStringFromName("closeButton.title", closeButtonTitle);
    bundle->GetStringFromName("actionButton.label", actionButtonTitle);
    if (!hostPort.IsEmpty()) {
      AutoTArray<nsString, 1> formatStrings = {hostPort};
      bundle->FormatStringFromName("webActions.disableForOrigin.label",
                                   formatStrings, disableButtonTitle);
    }
    bundle->GetStringFromName("webActions.settings.label", settingsButtonTitle);

    notification.otherButtonTitle = nsCocoaUtils::ToNSString(closeButtonTitle);

    // OS X 10.8 only shows action buttons if the "Alerts" style is set in
    // Notification Center preferences, and doesn't support the alternate
    // action menu.
    if ([notification respondsToSelector:@selector(set_showsButtons:)] &&
        [notification
            respondsToSelector:@selector(set_alwaysShowAlternateActionMenu:)] &&
        [notification
            respondsToSelector:@selector(set_alternateActionButtonTitles:)]) {
      notification.hasActionButton = YES;
      notification.actionButtonTitle =
          nsCocoaUtils::ToNSString(actionButtonTitle);

      [(NSObject*)notification setValue:@(YES) forKey:@"_showsButtons"];
      [(NSObject*)notification setValue:@(YES)
                                 forKey:@"_alwaysShowAlternateActionMenu"];
      [(NSObject*)notification setValue:@[
        nsCocoaUtils::ToNSString(disableButtonTitle),
        nsCocoaUtils::ToNSString(settingsButtonTitle)
      ]
                                 forKey:@"_alternateActionButtonTitles"];
    }
  }
  nsAutoString name;
  rv = aAlert->GetName(name);
  // Don't let an alert name be more than MAX_NOTIFICATION_NAME_LEN characters.
  // More than that shouldn't be necessary and userInfo (assigned to below) has
  // a length limit of 16k on OS X 10.11. Exception thrown if limit exceeded.
  if (name.Length() > MAX_NOTIFICATION_NAME_LEN) {
    return NS_ERROR_FAILURE;
  }

  NS_ENSURE_SUCCESS(rv, rv);
  NSString* alertName = nsCocoaUtils::ToNSString(name);
  if (!alertName) {
    return NS_ERROR_FAILURE;
  }
  notification.userInfo = [NSDictionary
      dictionaryWithObjects:[NSArray arrayWithObjects:alertName, nil]
                    forKeys:[NSArray arrayWithObjects:@"name", nil]];

  nsAutoString cookie;
  rv = aAlert->GetCookie(cookie);
  NS_ENSURE_SUCCESS(rv, rv);

  OSXNotificationInfo* osxni =
      new OSXNotificationInfo(alertName, aAlertListener, cookie);

  // Show the favicon if supported on this version of OS X.
  if (aIconSize > 0 &&
      [notification respondsToSelector:@selector(set_identityImage:)] &&
      [notification
          respondsToSelector:@selector(set_identityImageHasBorder:)]) {
    NSData* iconData = [NSData dataWithBytes:aIconData length:aIconSize];
    NSImage* icon = [[[NSImage alloc] initWithData:iconData] autorelease];

    [(NSObject*)notification setValue:icon forKey:@"_identityImage"];
    [(NSObject*)notification setValue:@(NO) forKey:@"_identityImageHasBorder"];
  }

  bool inPrivateBrowsing;
  rv = aAlert->GetInPrivateBrowsing(&inPrivateBrowsing);
  NS_ENSURE_SUCCESS(rv, rv);

  // Show the notification without waiting for an image if there is no icon URL
  // or notification icons are not supported on this version of OS X.
  if (![unClass instancesRespondToSelector:@selector(setContentImage:)]) {
    CloseAlertCocoaString(alertName);
    mActiveAlerts.AppendElement(osxni);
    [GetNotificationCenter() deliverNotification:notification];
    [notification release];
    if (aAlertListener) {
      aAlertListener->Observe(nullptr, "alertshow", cookie.get());
    }
  } else {
    mPendingAlerts.AppendElement(osxni);
    osxni->mPendingNotification = notification;
    // Wait six seconds for the image to load.
    rv = aAlert->LoadImage(6000, this, osxni,
                           getter_AddRefs(osxni->mIconRequest));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      ShowPendingNotification(osxni);
    }
  }

  return NS_OK;

  NS_OBJC_END_TRY_BLOCK_RETURN(NS_ERROR_FAILURE);
}

NS_IMETHODIMP
OSXNotificationCenter::CloseAlert(const nsAString& aAlertName,
                                  bool aContextClosed) {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  NSString* alertName = nsCocoaUtils::ToNSString(aAlertName);
  CloseAlertCocoaString(alertName);
  return NS_OK;

  NS_OBJC_END_TRY_BLOCK_RETURN(NS_ERROR_FAILURE);
}

void OSXNotificationCenter::CloseAlertCocoaString(NSString* aAlertName) {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  if (!aAlertName) {
    return;  // Can't do anything without a name
  }

  NSArray* notifications = [GetNotificationCenter() deliveredNotifications];
  for (id<FakeNSUserNotification> notification in notifications) {
    NSString* name = [[notification userInfo] valueForKey:@"name"];
    if ([name isEqualToString:aAlertName]) {
      [GetNotificationCenter() removeDeliveredNotification:notification];
      [GetNotificationCenter() _removeDisplayedNotification:notification];
      break;
    }
  }

  for (unsigned int i = 0; i < mActiveAlerts.Length(); i++) {
    OSXNotificationInfo* osxni = mActiveAlerts[i];
    if ([aAlertName isEqualToString:osxni->mName]) {
      if (osxni->mObserver) {
        osxni->mObserver->Observe(nullptr, "alertfinished",
                                  osxni->mCookie.get());
      }
      if (osxni->mIconRequest) {
        osxni->mIconRequest->Cancel(NS_BINDING_ABORTED);
        osxni->mIconRequest = nullptr;
      }
      mActiveAlerts.RemoveElementAt(i);
      break;
    }
  }

  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

void OSXNotificationCenter::OnActivate(
    NSString* aAlertName, NSUserNotificationActivationType aActivationType,
    unsigned long long aAdditionalActionIndex) {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  if (!aAlertName) {
    return;  // Can't do anything without a name
  }

  for (unsigned int i = 0; i < mActiveAlerts.Length(); i++) {
    OSXNotificationInfo* osxni = mActiveAlerts[i];
    if ([aAlertName isEqualToString:osxni->mName]) {
      if (osxni->mObserver) {
        switch ((int)aActivationType) {
          case NSUserNotificationActivationTypeAdditionalActionClicked:
          case NSUserNotificationActivationTypeActionButtonClicked:
            switch (aAdditionalActionIndex) {
              case OSXNotificationActionDisable:
                osxni->mObserver->Observe(nullptr, "alertdisablecallback",
                                          osxni->mCookie.get());
                break;
              case OSXNotificationActionSettings:
                osxni->mObserver->Observe(nullptr, "alertsettingscallback",
                                          osxni->mCookie.get());
                break;
              default:
                NS_WARNING(
                    "Unknown NSUserNotification additional action clicked");
                break;
            }
            break;
          default:
            osxni->mObserver->Observe(nullptr, "alertclickcallback",
                                      osxni->mCookie.get());
            break;
        }
      }
      return;
    }
  }

  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

void OSXNotificationCenter::ShowPendingNotification(
    OSXNotificationInfo* osxni) {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  if (osxni->mIconRequest) {
    osxni->mIconRequest->Cancel(NS_BINDING_ABORTED);
    osxni->mIconRequest = nullptr;
  }

  CloseAlertCocoaString(osxni->mName);

  for (unsigned int i = 0; i < mPendingAlerts.Length(); i++) {
    if (mPendingAlerts[i] == osxni) {
      mActiveAlerts.AppendElement(osxni);
      mPendingAlerts.RemoveElementAt(i);
      break;
    }
  }

  [GetNotificationCenter() deliverNotification:osxni->mPendingNotification];

  if (osxni->mObserver) {
    osxni->mObserver->Observe(nullptr, "alertshow", osxni->mCookie.get());
  }

  [osxni->mPendingNotification release];
  osxni->mPendingNotification = nil;

  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

NS_IMETHODIMP
OSXNotificationCenter::OnImageMissing(nsISupports* aUserData) {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  OSXNotificationInfo* osxni = static_cast<OSXNotificationInfo*>(aUserData);
  if (osxni->mPendingNotification) {
    // If there was an error getting the image, or the request timed out, show
    // the notification without a content image.
    ShowPendingNotification(osxni);
  }
  return NS_OK;

  NS_OBJC_END_TRY_BLOCK_RETURN(NS_ERROR_FAILURE);
}

NS_IMETHODIMP
OSXNotificationCenter::OnImageReady(nsISupports* aUserData,
                                    imgIRequest* aRequest) {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  nsCOMPtr<imgIContainer> image;
  nsresult rv = aRequest->GetImage(getter_AddRefs(image));
  if (NS_WARN_IF(NS_FAILED(rv) || !image)) {
    return rv;
  }

  OSXNotificationInfo* osxni = static_cast<OSXNotificationInfo*>(aUserData);
  if (!osxni->mPendingNotification) {
    return NS_ERROR_FAILURE;
  }

  NSImage* cocoaImage = nil;
  // TODO: Pass pres context / ComputedStyle here to support context paint
  // properties
  nsCocoaUtils::CreateDualRepresentationNSImageFromImageContainer(
      image, imgIContainer::FRAME_FIRST, nullptr, nullptr, &cocoaImage);
  (osxni->mPendingNotification).contentImage = cocoaImage;
  [cocoaImage release];
  ShowPendingNotification(osxni);

  return NS_OK;

  NS_OBJC_END_TRY_BLOCK_RETURN(NS_ERROR_FAILURE);
}

// nsIAlertsDoNotDisturb
NS_IMETHODIMP
OSXNotificationCenter::GetManualDoNotDisturb(bool* aRetVal) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
OSXNotificationCenter::SetManualDoNotDisturb(bool aDoNotDisturb) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
OSXNotificationCenter::GetSuppressForScreenSharing(bool* aRetVal) {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN

  NS_ENSURE_ARG(aRetVal);
  *aRetVal = mSuppressForScreenSharing;
  return NS_OK;

  NS_OBJC_END_TRY_BLOCK_RETURN(NS_ERROR_FAILURE)
}

NS_IMETHODIMP
OSXNotificationCenter::SetSuppressForScreenSharing(bool aSuppress) {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN

  mSuppressForScreenSharing = aSuppress;
  return NS_OK;

  NS_OBJC_END_TRY_BLOCK_RETURN(NS_ERROR_FAILURE)
}

}  // namespace mozilla

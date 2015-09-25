/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "OSXNotificationCenter.h"
#import <AppKit/AppKit.h>
#include "imgIRequest.h"
#include "imgIContainer.h"
#include "nsIStringBundle.h"
#include "nsNetUtil.h"
#include "imgLoader.h"
#import "nsCocoaUtils.h"
#include "nsContentUtils.h"
#include "nsObjCExceptions.h"
#include "nsString.h"
#include "nsCOMPtr.h"
#include "nsIObserver.h"
#include "nsIContentPolicy.h"
#include "imgRequestProxy.h"

using namespace mozilla;

#if !defined(MAC_OS_X_VERSION_10_8) || (MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_8)
@protocol NSUserNotificationCenterDelegate
@end
static NSString * const NSUserNotificationDefaultSoundName = @"DefaultSoundName";
enum {
  NSUserNotificationActivationTypeNone = 0,
  NSUserNotificationActivationTypeContentsClicked = 1,
  NSUserNotificationActivationTypeActionButtonClicked = 2,
};
#endif

#if !defined(MAC_OS_X_VERSION_10_9) || (MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_9)
enum {
  NSUserNotificationActivationTypeReplied = 3,
};
#endif

#if !defined(MAC_OS_X_VERSION_10_10) || (MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_10)
enum {
  NSUserNotificationActivationTypeAdditionalActionClicked = 4
};
#endif

@protocol FakeNSUserNotification <NSObject>
@property (copy) NSString* title;
@property (copy) NSString* subtitle;
@property (copy) NSString* informativeText;
@property (copy) NSString* actionButtonTitle;
@property (copy) NSDictionary* userInfo;
@property (copy) NSDate* deliveryDate;
@property (copy) NSTimeZone* deliveryTimeZone;
@property (copy) NSDateComponents* deliveryRepeatInterval;
@property (readonly) NSDate* actualDeliveryDate;
@property (readonly, getter=isPresented) BOOL presented;
@property (readonly, getter=isRemote) BOOL remote;
@property (copy) NSString* soundName;
@property BOOL hasActionButton;
@property (readonly) NSUserNotificationActivationType activationType;
@property (copy) NSString *otherButtonTitle;
@property (copy) NSImage *contentImage;
@end

@protocol FakeNSUserNotificationCenter <NSObject>
+ (id<FakeNSUserNotificationCenter>)defaultUserNotificationCenter;
@property (assign) id <NSUserNotificationCenterDelegate> delegate;
@property (copy) NSArray *scheduledNotifications;
- (void)scheduleNotification:(id<FakeNSUserNotification>)notification;
- (void)removeScheduledNotification:(id<FakeNSUserNotification>)notification;
@property (readonly) NSArray *deliveredNotifications;
- (void)deliverNotification:(id<FakeNSUserNotification>)notification;
- (void)removeDeliveredNotification:(id<FakeNSUserNotification>)notification;
- (void)removeAllDeliveredNotifications;
- (void)_removeAllDisplayedNotifications;
- (void)_removeDisplayedNotification:(id<FakeNSUserNotification>)notification;
@end

@interface mozNotificationCenterDelegate : NSObject <NSUserNotificationCenterDelegate>
{
  OSXNotificationCenter *mOSXNC;
}
  - (id)initWithOSXNC:(OSXNotificationCenter*)osxnc;
@end

@implementation mozNotificationCenterDelegate

- (id)initWithOSXNC:(OSXNotificationCenter*)osxnc
{
  [super init];
  // We should *never* outlive this OSXNotificationCenter.
  mOSXNC = osxnc;
  return self;
}

- (void)userNotificationCenter:(id<FakeNSUserNotificationCenter>)center
        didDeliverNotification:(id<FakeNSUserNotification>)notification
{

}

- (void)userNotificationCenter:(id<FakeNSUserNotificationCenter>)center
       didActivateNotification:(id<FakeNSUserNotification>)notification
{
  NSNumber *alternateActionIndex = [(NSObject*)notification valueForKey:@"_alternateActionIndex"];
  mOSXNC->OnActivate([[notification userInfo] valueForKey:@"name"],
                     notification.activationType,
                     [alternateActionIndex unsignedLongLongValue]);
}

- (BOOL)userNotificationCenter:(id<FakeNSUserNotificationCenter>)center
     shouldPresentNotification:(id<FakeNSUserNotification>)notification
{
  return YES;
}

// This is an undocumented method that we need for parity with Safari.
// Apple bug #15440664.
- (void)userNotificationCenter:(id<FakeNSUserNotificationCenter>)center
  didRemoveDeliveredNotifications:(NSArray *)notifications
{
  for (id<FakeNSUserNotification> notification in notifications) {
    NSString *name = [[notification userInfo] valueForKey:@"name"];
    mOSXNC->CloseAlertCocoaString(name);
  }
}

// This is an undocumented method that we need to be notified if a user clicks the close button.
- (void)userNotificationCenter:(id<FakeNSUserNotificationCenter>)center
  didDismissAlert:(id<FakeNSUserNotification>)notification
{
  NSString *name = [[notification userInfo] valueForKey:@"name"];
  mOSXNC->CloseAlertCocoaString(name);
}

@end

namespace mozilla {

enum {
  OSXNotificationActionDisable = 0,
  OSXNotificationActionSettings = 1,
};

class OSXNotificationInfo {
private:
  ~OSXNotificationInfo();

public:
  NS_INLINE_DECL_REFCOUNTING(OSXNotificationInfo)
  OSXNotificationInfo(NSString *name, nsIObserver *observer,
                      const nsAString & alertCookie);

  NSString *mName;
  nsCOMPtr<nsIObserver> mObserver;
  nsString mCookie;
  nsRefPtr<imgRequestProxy> mIconRequest;
  id<FakeNSUserNotification> mPendingNotifiction;
  nsCOMPtr<nsITimer> mIconTimeoutTimer;
};

OSXNotificationInfo::OSXNotificationInfo(NSString *name, nsIObserver *observer,
                                         const nsAString & alertCookie)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  NS_ASSERTION(name, "Cannot create OSXNotificationInfo without a name!");
  mName = [name retain];
  mObserver = observer;
  mCookie = alertCookie;
  mPendingNotifiction = nil;

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

OSXNotificationInfo::~OSXNotificationInfo()
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  [mName release];
  [mPendingNotifiction release];

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

static id<FakeNSUserNotificationCenter> GetNotificationCenter() {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  Class c = NSClassFromString(@"NSUserNotificationCenter");
  return [c performSelector:@selector(defaultUserNotificationCenter)];

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

OSXNotificationCenter::OSXNotificationCenter()
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  mDelegate = [[mozNotificationCenterDelegate alloc] initWithOSXNC:this];
  GetNotificationCenter().delegate = mDelegate;

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

OSXNotificationCenter::~OSXNotificationCenter()
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  [GetNotificationCenter() removeAllDeliveredNotifications];
  [mDelegate release];

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

NS_IMPL_ISUPPORTS(OSXNotificationCenter, nsIAlertsService, imgINotificationObserver, nsITimerCallback)

nsresult OSXNotificationCenter::Init()
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  return (!!NSClassFromString(@"NSUserNotification")) ? NS_OK : NS_ERROR_FAILURE;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

NS_IMETHODIMP
OSXNotificationCenter::ShowAlertNotification(const nsAString & aImageUrl, const nsAString & aAlertTitle,
                                             const nsAString & aAlertText, bool aAlertTextClickable,
                                             const nsAString & aAlertCookie,
                                             nsIObserver * aAlertListener,
                                             const nsAString & aAlertName,
                                             const nsAString & aBidi,
                                             const nsAString & aLang,
                                             const nsAString & aData,
                                             nsIPrincipal * aPrincipal,
                                             bool aInPrivateBrowsing)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  Class unClass = NSClassFromString(@"NSUserNotification");
  id<FakeNSUserNotification> notification = [[unClass alloc] init];
  notification.title = [NSString stringWithCharacters:(const unichar *)aAlertTitle.BeginReading()
                                               length:aAlertTitle.Length()];
  notification.informativeText = [NSString stringWithCharacters:(const unichar *)aAlertText.BeginReading()
                                                         length:aAlertText.Length()];
  notification.soundName = NSUserNotificationDefaultSoundName;
  notification.hasActionButton = NO;

  // If this is not an application/extension alert, show additional actions dealing with permissions.
  if (!nsContentUtils::IsSystemOrExpandedPrincipal(aPrincipal)
      && !aPrincipal->GetIsNullPrincipal()) {
    nsCOMPtr<nsIStringBundleService> sbs = do_GetService(NS_STRINGBUNDLE_CONTRACTID);
    nsCOMPtr<nsIStringBundle> bundle;
    nsresult rv = sbs->CreateBundle("chrome://alerts/locale/alert.properties", getter_AddRefs(bundle));
    if (NS_SUCCEEDED(rv)) {
      nsXPIDLString closeButtonTitle, actionButtonTitle, disableButtonTitle, settingsButtonTitle;
      bundle->GetStringFromName(NS_LITERAL_STRING("closeButton.title").get(),
                                getter_Copies(closeButtonTitle));
      bundle->GetStringFromName(NS_LITERAL_STRING("actionButton.label").get(),
                                getter_Copies(actionButtonTitle));
      bundle->GetStringFromName(NS_LITERAL_STRING("webActions.disable.label").get(),
                                getter_Copies(disableButtonTitle));
      bundle->GetStringFromName(NS_LITERAL_STRING("webActions.settings.label").get(),
                                getter_Copies(settingsButtonTitle));

      notification.hasActionButton = YES;
      notification.otherButtonTitle = nsCocoaUtils::ToNSString(closeButtonTitle);
      notification.actionButtonTitle = nsCocoaUtils::ToNSString(actionButtonTitle);
      [(NSObject*)notification setValue:@(YES) forKey:@"_showsButtons"];
      [(NSObject*)notification setValue:@(YES) forKey:@"_alwaysShowAlternateActionMenu"];
      [(NSObject*)notification setValue:@[
                                          nsCocoaUtils::ToNSString(disableButtonTitle),
                                          nsCocoaUtils::ToNSString(settingsButtonTitle)
                                          ]
                               forKey:@"_alternateActionButtonTitles"];
    }
  }
  NSString *alertName = [NSString stringWithCharacters:(const unichar *)aAlertName.BeginReading() length:aAlertName.Length()];
  if (!alertName) {
    return NS_ERROR_FAILURE;
  }
  notification.userInfo = [NSDictionary dictionaryWithObjects:[NSArray arrayWithObjects:alertName, nil]
                                                      forKeys:[NSArray arrayWithObjects:@"name", nil]];

  OSXNotificationInfo *osxni = new OSXNotificationInfo(alertName, aAlertListener, aAlertCookie);

  // Show the notification without waiting for an image if there is no icon URL or
  // notification icons are not supported on this version of OS X.
  if (aImageUrl.IsEmpty() || ![unClass instancesRespondToSelector:@selector(setContentImage:)]) {
    CloseAlertCocoaString(alertName);
    mActiveAlerts.AppendElement(osxni);
    [GetNotificationCenter() deliverNotification:notification];
    [notification release];
    if (aAlertListener) {
      aAlertListener->Observe(nullptr, "alertshow", PromiseFlatString(aAlertCookie).get());
    }
  } else {
    mPendingAlerts.AppendElement(osxni);
    osxni->mPendingNotifiction = notification;
    nsRefPtr<imgLoader> il = imgLoader::GetInstance();
    if (il) {
      nsCOMPtr<nsIURI> imageUri;
      NS_NewURI(getter_AddRefs(imageUri), aImageUrl);
      if (imageUri) {
        nsresult rv = il->LoadImage(imageUri, nullptr, nullptr,
                                    mozilla::net::RP_Default,
                                    aPrincipal, nullptr,
                                    this, nullptr,
                                    aInPrivateBrowsing ? nsIRequest::LOAD_ANONYMOUS :
                                                         nsIRequest::LOAD_NORMAL,
                                    nullptr, nsIContentPolicy::TYPE_INTERNAL_IMAGE,
                                    EmptyString(),
                                    getter_AddRefs(osxni->mIconRequest));
        if (NS_SUCCEEDED(rv)) {
          // Set a timer for six seconds. If we don't have an icon by the time this
          // goes off then we go ahead without an icon.
          nsCOMPtr<nsITimer> timer = do_CreateInstance(NS_TIMER_CONTRACTID);
          osxni->mIconTimeoutTimer = timer;
          timer->InitWithCallback(this, 6000, nsITimer::TYPE_ONE_SHOT);
          return NS_OK;
        }
      }
    }
    ShowPendingNotification(osxni);
  }

  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

NS_IMETHODIMP
OSXNotificationCenter::CloseAlert(const nsAString& aAlertName,
                                  nsIPrincipal* aPrincipal)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  NSString *alertName = [NSString stringWithCharacters:(const unichar *)aAlertName.BeginReading() length:aAlertName.Length()];
  CloseAlertCocoaString(alertName);
  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

void
OSXNotificationCenter::CloseAlertCocoaString(NSString *aAlertName)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  if (!aAlertName) {
    return; // Can't do anything without a name
  }

  NSArray *notifications = [GetNotificationCenter() deliveredNotifications];
  for (id<FakeNSUserNotification> notification in notifications) {
    NSString *name = [[notification userInfo] valueForKey:@"name"];
    if ([name isEqualToString:aAlertName]) {
      [GetNotificationCenter() removeDeliveredNotification:notification];
      [GetNotificationCenter() _removeDisplayedNotification:notification];
      break;
    }
  }

  for (unsigned int i = 0; i < mActiveAlerts.Length(); i++) {
    OSXNotificationInfo *osxni = mActiveAlerts[i];
    if ([aAlertName isEqualToString:osxni->mName]) {
      if (osxni->mObserver) {
        osxni->mObserver->Observe(nullptr, "alertfinished", osxni->mCookie.get());
      }
      mActiveAlerts.RemoveElementAt(i);
      break;
    }
  }

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

void
OSXNotificationCenter::OnActivate(NSString *aAlertName,
                                  NSUserNotificationActivationType aActivationType,
                                  unsigned long long aAdditionalActionIndex)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  if (!aAlertName) {
    return; // Can't do anything without a name
  }

  for (unsigned int i = 0; i < mActiveAlerts.Length(); i++) {
    OSXNotificationInfo *osxni = mActiveAlerts[i];
    if ([aAlertName isEqualToString:osxni->mName]) {
      if (osxni->mObserver) {
        switch (aActivationType) {
          case NSUserNotificationActivationTypeAdditionalActionClicked:
          case NSUserNotificationActivationTypeActionButtonClicked:
            switch (aAdditionalActionIndex) {
              case OSXNotificationActionDisable:
                osxni->mObserver->Observe(nullptr, "alertdisablecallback", osxni->mCookie.get());
                break;
              case OSXNotificationActionSettings:
                osxni->mObserver->Observe(nullptr, "alertsettingscallback", osxni->mCookie.get());
                break;
              default:
                NS_WARNING("Unknown NSUserNotification additional action clicked");
                break;
            }
            break;
          default:
            osxni->mObserver->Observe(nullptr, "alertclickcallback", osxni->mCookie.get());
            break;
        }
      }
      return;
    }
  }

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

void
OSXNotificationCenter::ShowPendingNotification(OSXNotificationInfo *osxni)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  if (osxni->mIconTimeoutTimer) {
    osxni->mIconTimeoutTimer->Cancel();
    osxni->mIconTimeoutTimer = nullptr;
  }

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

  [GetNotificationCenter() deliverNotification:osxni->mPendingNotifiction];

  if (osxni->mObserver) {
    osxni->mObserver->Observe(nullptr, "alertshow", osxni->mCookie.get());
  }

  [osxni->mPendingNotifiction release];
  osxni->mPendingNotifiction = nil;

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

NS_IMETHODIMP
OSXNotificationCenter::Notify(imgIRequest *aRequest, int32_t aType, const nsIntRect* aData)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  if (aType == imgINotificationObserver::LOAD_COMPLETE) {
    OSXNotificationInfo *osxni = nullptr;
    for (unsigned int i = 0; i < mPendingAlerts.Length(); i++) {
      if (aRequest == mPendingAlerts[i]->mIconRequest) {
        osxni = mPendingAlerts[i];
        break;
      }
    }
    if (!osxni || !osxni->mPendingNotifiction) {
      return NS_ERROR_FAILURE;
    }
    NSImage *cocoaImage = nil;
    uint32_t imgStatus = imgIRequest::STATUS_ERROR;
    nsresult rv = aRequest->GetImageStatus(&imgStatus);
    if (NS_SUCCEEDED(rv) && !(imgStatus & imgIRequest::STATUS_ERROR)) {
      nsCOMPtr<imgIContainer> image;
      rv = aRequest->GetImage(getter_AddRefs(image));
      if (NS_SUCCEEDED(rv)) {
        nsCocoaUtils::CreateNSImageFromImageContainer(image, imgIContainer::FRAME_FIRST, &cocoaImage, 1.0f);
      }
    }
    (osxni->mPendingNotifiction).contentImage = cocoaImage;
    [cocoaImage release];
    ShowPendingNotification(osxni);
  }
  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

NS_IMETHODIMP
OSXNotificationCenter::Notify(nsITimer *timer)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  if (!timer) {
    return NS_ERROR_FAILURE;
  }

  for (unsigned int i = 0; i < mPendingAlerts.Length(); i++) {
    OSXNotificationInfo *osxni = mPendingAlerts[i];
    if (timer == osxni->mIconTimeoutTimer.get()) {
      osxni->mIconTimeoutTimer = nullptr;
      if (osxni->mPendingNotifiction) {
        ShowPendingNotification(osxni);
        break;
      }
    }
  }
  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

} // namespace mozilla

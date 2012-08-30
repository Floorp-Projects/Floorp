/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NS_NOTIFICATION_CENTER_COMPAT_H
#define NS_NOTIFICATION_CENTER_COMPAT_H

#import <Cocoa/Cocoa.h>

#if !defined(MAC_OS_X_VERSION_10_8) || (MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_8)
@protocol NSUserNotificationCenterDelegate
@end

static NSString * const NSUserNotificationDefaultSoundName = @"DefaultSoundName";

enum {
  NSUserNotificationActivationTypeNone = 0,
  NSUserNotificationActivationTypeContentsClicked = 1,
  NSUserNotificationActivationTypeActionButtonClicked = 2
};
typedef NSInteger NSUserNotificationActivationType;
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
@end

#endif

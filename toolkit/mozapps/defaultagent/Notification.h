/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __DEFAULT_BROWSER_NOTIFICATION_H__
#define __DEFAULT_BROWSER_NOTIFICATION_H__

#include "DefaultBrowser.h"

namespace mozilla::default_agent {

enum class NotificationType {
  Initial,
  Followup,
};

enum class NotificationShown {
  NotShown,
  Shown,
  Error,
};

enum class NotificationAction {
  DismissedByTimeout,
  DismissedToActionCenter,
  DismissedByButton,
  DismissedByApplicationHidden,
  RemindMeLater,
  MakeFirefoxDefaultButton,
  ToastClicked,
  NoAction,  // Should not be used with NotificationShown::Shown
};

struct NotificationActivities {
  NotificationType type;
  NotificationShown shown;
  NotificationAction action;
};

NotificationActivities MaybeShowNotification(
    const DefaultBrowserInfo& browserInfo, const wchar_t* aumi, bool force);

// These take enum values and get strings suitable for telemetry
std::string GetStringForNotificationType(NotificationType type);
std::string GetStringForNotificationShown(NotificationShown shown);
NotificationShown GetNotificationShownFromString(const nsAString& shown);
std::string GetStringForNotificationAction(NotificationAction action);
NotificationAction GetNotificationActionFromString(const nsAString& action);

// If actionString is a valid action string (i.e. corresponds to one of the
// NotificationAction values), this function has no effect. If actionString is
// not a valid action string, its value will be replaced with the string for
// NotificationAction::NoAction.
void EnsureValidNotificationAction(std::string& actionString);

}  // namespace mozilla::default_agent

#endif  // __DEFAULT_BROWSER_NOTIFICATION_H__

/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Notification.h"

#include <shlwapi.h>
#include <windows.h>
#include <winnt.h>

#include "nsLiteralString.h"

#define SEVEN_DAYS_IN_SECONDS (7 * 24 * 60 * 60)

// If the notification hasn't been activated or dismissed within 11 hours 55
// minutes, stop waiting for it.
#define NOTIFICATION_WAIT_TIMEOUT_MS (11 * 60 * 60 * 1000 + 55 * 60 * 1000)
// If the mutex hasn't been released within a few minutes, something is wrong
// and we should give up on it
#define MUTEX_TIMEOUT_MS (10 * 60 * 1000)

namespace mozilla::default_agent {

std::string GetStringForNotificationType(NotificationType type) {
  switch (type) {
    case NotificationType::Initial:
      return std::string("initial");
    case NotificationType::Followup:
      return std::string("followup");
  }
}

std::string GetStringForNotificationShown(NotificationShown shown) {
  switch (shown) {
    case NotificationShown::NotShown:
      return std::string("not-shown");
    case NotificationShown::Shown:
      return std::string("shown");
    case NotificationShown::Error:
      return std::string("error");
  }
}

NotificationShown GetNotificationShownFromString(const nsAString& shown) {
  if (shown == u"not-shown"_ns) {
    return NotificationShown::NotShown;
  } else if (shown == u"shown"_ns) {
    return NotificationShown::Shown;
  } else if (shown == u"error"_ns) {
    return NotificationShown::Error;
  } else {
    // Catch all.
    return NotificationShown::Error;
  }
}

std::string GetStringForNotificationAction(NotificationAction action) {
  switch (action) {
    case NotificationAction::DismissedByTimeout:
      return std::string("dismissed-by-timeout");
    case NotificationAction::DismissedToActionCenter:
      return std::string("dismissed-to-action-center");
    case NotificationAction::DismissedByButton:
      return std::string("dismissed-by-button");
    case NotificationAction::DismissedByApplicationHidden:
      return std::string("dismissed-by-application-hidden");
    case NotificationAction::RemindMeLater:
      return std::string("remind-me-later");
    case NotificationAction::MakeFirefoxDefaultButton:
      return std::string("make-firefox-default-button");
    case NotificationAction::ToastClicked:
      return std::string("toast-clicked");
    case NotificationAction::NoAction:
      return std::string("no-action");
  }
}

NotificationAction GetNotificationActionFromString(const nsAString& action) {
  if (action == u"dismissed-by-timeout"_ns) {
    return NotificationAction::DismissedByTimeout;
  } else if (action == u"dismissed-to-action-center"_ns) {
    return NotificationAction::DismissedToActionCenter;
  } else if (action == u"dismissed-by-button"_ns) {
    return NotificationAction::DismissedByButton;
  } else if (action == u"dismissed-by-application-hidden"_ns) {
    return NotificationAction::DismissedByApplicationHidden;
  } else if (action == u"remind-me-later"_ns) {
    return NotificationAction::RemindMeLater;
  } else if (action == u"make-firefox-default-button"_ns) {
    return NotificationAction::MakeFirefoxDefaultButton;
  } else if (action == u"toast-clicked"_ns) {
    return NotificationAction::ToastClicked;
  } else if (action == u"no-action"_ns) {
    return NotificationAction::NoAction;
  } else {
    // Catch all.
    return NotificationAction::NoAction;
  }
}

void EnsureValidNotificationAction(std::string& actionString) {
  if (actionString != "dismissed-by-timeout" &&
      actionString != "dismissed-to-action-center" &&
      actionString != "dismissed-by-button" &&
      actionString != "dismissed-by-application-hidden" &&
      actionString != "remind-me-later" &&
      actionString != "make-firefox-default-button" &&
      actionString != "toast-clicked" && actionString != "no-action") {
    actionString = "no-action";
  }
}

}  // namespace mozilla::default_agent

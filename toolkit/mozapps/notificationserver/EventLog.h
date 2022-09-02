/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NotificationServer_EventLog_h__
#define NotificationServer_EventLog_h__

#include "mozilla/WindowsEventLog.h"

#define LOG_ERROR_MESSAGE(format, ...) \
  MOZ_WIN_EVENT_LOG_ERROR_MESSAGE(     \
      L"" MOZ_APP_DISPLAYNAME " Notification Server", format, __VA_ARGS__)

#endif  // NotificationServer_EventLog_h__

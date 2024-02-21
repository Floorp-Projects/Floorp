/* -*- Mode: Objective-C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset:
 * 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#import "UpdateSettings/UpdateSettings.h"

#include "UpdateSettingsUtil.h"

/* static */
std::optional<std::string> UpdateSettingsUtil::GetAcceptedMARChannelsValue() {
  // `UpdateSettingsGetAcceptedMARChannels` is resolved at runtime and requires
  // the UpdateSettings framework to be loaded.
  if (UpdateSettingsGetAcceptedMARChannels) {
    NSString* marChannels = UpdateSettingsGetAcceptedMARChannels();
    return [marChannels UTF8String];
  }
  return {};
}

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "UpdateSettings.h"

#include "mozilla/HelperMacros.h"

NSString* UpdateSettingsGetAcceptedMARChannels(void) {
  return
      [NSString stringWithFormat:@"[Settings]\nACCEPTED_MAR_CHANNEL_IDS=%s\n",
                                 ACCEPTED_MAR_CHANNEL_IDS];
}

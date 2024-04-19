/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ChannelPrefs.h"

#include "mozilla/HelperMacros.h"

#ifdef MOZ_UPDATE_CHANNEL_OVERRIDE
#  define CHANNEL MOZ_UPDATE_CHANNEL_OVERRIDE
#else
#  define CHANNEL MOZ_UPDATE_CHANNEL
#endif

NSString* ChannelPrefsGetChannel() {
  return [NSString stringWithCString:MOZ_STRINGIFY(CHANNEL)
                            encoding:NSUTF8StringEncoding];
}

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef UpdateSettings_h_
#define UpdateSettings_h_

#import <Foundation/Foundation.h>

extern "C" {

// Returns the accepted MAR channels, as an autoreleased string.
extern NSString* UpdateSettingsGetAcceptedMARChannels(void)
    __attribute__((weak_import)) __attribute__((visibility("default")));
}

#endif  // UpdateSettings_h_

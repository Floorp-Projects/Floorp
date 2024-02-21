/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ChannelPrefs_h_
#define ChannelPrefs_h_

#import <Foundation/Foundation.h>

extern "C" {

// Returns the channel name, as an autoreleased string.
extern NSString* ChannelPrefsGetChannel(void) __attribute__((weak_import))
__attribute__((visibility("default")));
}

#endif  // ChannelPrefs_h_

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#pragma once

#import <Foundation/Foundation.h>

namespace floorp::shim {

NSString* FingerprintOwnedBundle(NSString* path);
bool MoveStagedBundle(NSString* stage, NSString* backup, NSString* fingerprint);
bool RetireAppBundle(NSString* live, NSString* stage, NSString* fingerprint);
bool ExchangeAppBundles(NSString* live, NSString* backup,
                        NSString* liveFingerprint, NSString* backupFingerprint);
bool RemoveStagedBundle(NSString* path, NSString* fingerprint);

}  // namespace floorp::shim

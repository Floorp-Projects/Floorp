/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BundleTransaction.h"
#include <cassert>
#include <cstdio>
#include <sys/stat.h>

using namespace floorp::shim;

static void WriteBundle(NSString* path, NSString* bytes) {
  assert([NSFileManager.defaultManager createDirectoryAtPath:path
      withIntermediateDirectories:YES attributes:@{NSFilePosixPermissions: @0700} error:nil]);
  assert([bytes writeToFile:[path stringByAppendingPathComponent:@"launcher"]
      atomically:YES encoding:NSUTF8StringEncoding error:nil]);
}

int main() {
  @autoreleasepool {
    NSString* root = [NSTemporaryDirectory() stringByAppendingPathComponent:
        [@"floorp-bundle-transaction-" stringByAppendingString:NSUUID.UUID.UUIDString]];
    NSString* live = [root stringByAppendingPathComponent:@"既存 App.app"];
    NSString* stage = [root stringByAppendingPathComponent:@".floorp-stage-test.app"];
    NSString* backup = [root stringByAppendingPathComponent:@".floorp-backup-test.app"];
    WriteBundle(live, @"old working launcher");
    WriteBundle(stage, @"new native application");
    NSString* oldHash = FingerprintOwnedBundle(live);
    NSString* newHash = FingerprintOwnedBundle(stage);
    assert(oldHash && newHash && ![oldHash isEqual:newHash]);
    assert(!ExchangeAppBundles(live, stage, oldHash, newHash));
    assert(!MoveStagedBundle(stage,
        [root stringByAppendingPathComponent:@".floorp-backup-wrong.app"], newHash));
    assert(MoveStagedBundle(stage, backup, newHash));
    assert(![NSFileManager.defaultManager fileExistsAtPath:stage]);
    assert(!ExchangeAppBundles(live, backup, newHash, oldHash));
    assert([FingerprintOwnedBundle(live) isEqual:oldHash]);
    assert(ExchangeAppBundles(live, backup, oldHash, newHash));
    assert([FingerprintOwnedBundle(live) isEqual:newHash]);
    assert([FingerprintOwnedBundle(backup) isEqual:oldHash]);
    // A recovery process can inspect these hashes after an interrupted journal
    // write and atomically restore the original without an absent live path.
    assert(ExchangeAppBundles(live, backup, newHash, oldHash));
    assert([FingerprintOwnedBundle(live) isEqual:oldHash]);
    assert(!RemoveStagedBundle(live, oldHash));
    assert(!RemoveStagedBundle(backup, oldHash));
    assert(RemoveStagedBundle(backup, newHash));

    assert(!RetireAppBundle(live, stage, newHash));
    assert(RetireAppBundle(live, stage, oldHash));
    assert(![NSFileManager.defaultManager fileExistsAtPath:live]);
    assert([FingerprintOwnedBundle(stage) isEqual:oldHash]);
    assert(!RetireAppBundle(stage, backup, oldHash));
    assert(RemoveStagedBundle(stage, oldHash));
    WriteBundle(live, @"old working launcher");

    WriteBundle(stage, @"tampered candidate");
    NSString* tamperedHash = FingerprintOwnedBundle(stage);
    NSString* link = [stage stringByAppendingPathComponent:@"outside"];
    assert([NSFileManager.defaultManager createSymbolicLinkAtPath:link
        withDestinationPath:live error:nil]);
    assert(!FingerprintOwnedBundle(stage));
    assert(!MoveStagedBundle(stage, backup, tamperedHash));
    assert([NSFileManager.defaultManager removeItemAtPath:root error:nil]);
    puts("Bundle transaction checks passed: exchange, rollback, stale hashes, reserved paths, symlinks.");
  }
}

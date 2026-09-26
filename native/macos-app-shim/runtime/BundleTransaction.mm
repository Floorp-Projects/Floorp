/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BundleTransaction.h"

#include <CommonCrypto/CommonDigest.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

namespace floorp::shim {
namespace {

constexpr NSUInteger kMaxBundleEntries = 128;
constexpr unsigned long long kMaxBundleBytes = 64 * 1024 * 1024;

bool OwnedPath(NSString* path, bool directory) {
  struct stat status;
  return path.isAbsolutePath && [path isEqual:path.stringByStandardizingPath] &&
         lstat(path.fileSystemRepresentation, &status) == 0 &&
         status.st_uid == geteuid() && !(status.st_mode & (S_IWGRP | S_IWOTH)) &&
         (directory ? S_ISDIR(status.st_mode) : S_ISREG(status.st_mode));
}

bool Sidecar(NSString* path, NSString* prefix) {
  NSString* filename = path.lastPathComponent;
  if (![filename hasPrefix:prefix] || ![filename hasSuffix:@".app"]) return false;
  NSString* token = [filename substringWithRange:
      NSMakeRange(prefix.length, filename.length - prefix.length - 4)];
  NSCharacterSet* allowed = [NSCharacterSet characterSetWithCharactersInString:
      @"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-"];
  return token.length && token.length <= 80 &&
         [token rangeOfCharacterFromSet:allowed.invertedSet].location == NSNotFound;
}

bool FingerprintMatches(NSString* path, NSString* expected) {
  return expected.length == CC_SHA256_DIGEST_LENGTH * 2 &&
         [FingerprintOwnedBundle(path) isEqual:expected];
}

int ParentDirectory(NSString* first, NSString* second) {
  NSString* parent = first.stringByDeletingLastPathComponent;
  if (![parent isEqual:second.stringByDeletingLastPathComponent] ||
      !OwnedPath(parent, true) || [first isEqual:second]) return -1;
  return open(parent.fileSystemRepresentation, O_RDONLY | O_DIRECTORY | O_NOFOLLOW);
}

}  // namespace

NSString* FingerprintOwnedBundle(NSString* path) {
  if (!OwnedPath(path, true) || ![path.pathExtension isEqual:@"app"]) return nil;
  NSFileManager* manager = NSFileManager.defaultManager;
  NSDirectoryEnumerator* enumerator = [manager enumeratorAtPath:path];
  if (!enumerator) return nil;
  NSMutableArray<NSString*>* entries = [NSMutableArray arrayWithObject:@""];
  for (NSString* entry in enumerator) {
    if (entries.count >= kMaxBundleEntries) return nil;
    [entries addObject:entry];
  }
  [entries sortUsingSelector:@selector(compare:)];
  NSMutableData* contents = [NSMutableData data];
  unsigned long long totalBytes = 0;
  for (NSString* entry in entries) {
    NSString* filename = entry.length ? [path stringByAppendingPathComponent:entry] : path;
    struct stat status;
    if (lstat(filename.fileSystemRepresentation, &status) != 0 ||
        status.st_uid != geteuid() || (status.st_mode & (S_IWGRP | S_IWOTH)) ||
        (!S_ISREG(status.st_mode) && !S_ISDIR(status.st_mode))) return nil;
    NSData* name = [entry dataUsingEncoding:NSUTF8StringEncoding];
    if (!name) return nil;
    uint64_t nameLength = CFSwapInt64HostToBig(name.length);
    uint32_t mode = CFSwapInt32HostToBig(status.st_mode & (S_IFMT | 0777));
    uint64_t fileLength = 0;
    NSData* data = nil;
    if (S_ISREG(status.st_mode)) {
      if (status.st_size < 0 || static_cast<uint64_t>(status.st_size) > kMaxBundleBytes) return nil;
      totalBytes += status.st_size;
      if (totalBytes > kMaxBundleBytes) return nil;
      data = [NSData dataWithContentsOfFile:filename options:NSDataReadingMappedIfSafe error:nil];
      if (!data || data.length != static_cast<uint64_t>(status.st_size)) return nil;
      fileLength = CFSwapInt64HostToBig(data.length);
    }
    [contents appendBytes:&nameLength length:sizeof(nameLength)];
    [contents appendData:name];
    [contents appendBytes:&mode length:sizeof(mode)];
    [contents appendBytes:&fileLength length:sizeof(fileLength)];
    if (data) [contents appendData:data];
  }
  unsigned char digest[CC_SHA256_DIGEST_LENGTH];
  CC_SHA256(contents.bytes, static_cast<CC_LONG>(contents.length), digest);
  NSMutableString* hex = [NSMutableString stringWithCapacity:sizeof(digest) * 2];
  for (unsigned char byte : digest) [hex appendFormat:@"%02x", byte];
  return hex;
}

bool MoveStagedBundle(NSString* stage, NSString* backup, NSString* fingerprint) {
  if (!Sidecar(stage, @".floorp-stage-") || !Sidecar(backup, @".floorp-backup-") ||
      ![[stage.lastPathComponent stringByReplacingOccurrencesOfString:@".floorp-stage-"
          withString:@".floorp-backup-"] isEqual:backup.lastPathComponent] ||
      !FingerprintMatches(stage, fingerprint)) return false;
  int parent = ParentDirectory(stage, backup);
  if (parent < 0) return false;
  const int result = renameatx_np(parent, stage.lastPathComponent.fileSystemRepresentation,
      parent, backup.lastPathComponent.fileSystemRepresentation, RENAME_EXCL);
  if (result == 0) fsync(parent);
  close(parent);
  return result == 0;
}

bool ExchangeAppBundles(NSString* live, NSString* backup,
                        NSString* liveFingerprint, NSString* backupFingerprint) {
  if ([live.lastPathComponent hasPrefix:@".floorp-"] ||
      !Sidecar(backup, @".floorp-backup-") ||
      !FingerprintMatches(live, liveFingerprint) ||
      !FingerprintMatches(backup, backupFingerprint)) return false;
  int parent = ParentDirectory(live, backup);
  if (parent < 0) return false;
  const int result = renameatx_np(parent, live.lastPathComponent.fileSystemRepresentation,
      parent, backup.lastPathComponent.fileSystemRepresentation, RENAME_SWAP);
  if (result == 0) fsync(parent);
  close(parent);
  return result == 0;
}

bool RetireAppBundle(NSString* live, NSString* stage, NSString* fingerprint) {
  if ([live.lastPathComponent hasPrefix:@".floorp-"] ||
      !Sidecar(stage, @".floorp-stage-") || !FingerprintMatches(live, fingerprint)) return false;
  int parent = ParentDirectory(live, stage);
  if (parent < 0) return false;
  const int result = renameatx_np(parent, live.lastPathComponent.fileSystemRepresentation,
      parent, stage.lastPathComponent.fileSystemRepresentation, RENAME_EXCL);
  if (result == 0) fsync(parent);
  close(parent);
  return result == 0;
}

bool RemoveStagedBundle(NSString* path, NSString* fingerprint) {
  if ((!Sidecar(path, @".floorp-stage-") && !Sidecar(path, @".floorp-backup-")) ||
      !FingerprintMatches(path, fingerprint)) return false;
  return [NSFileManager.defaultManager removeItemAtPath:path error:nil];
}

}  // namespace floorp::shim

/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "updaterfileutils_osx.h"

#include <Cocoa/Cocoa.h>

bool IsRecursivelyWritable(const char* aPath) {
  NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];

  NSString* rootPath = [NSString stringWithUTF8String:aPath];
  NSFileManager* fileManager = [NSFileManager defaultManager];
  NSError* error = nil;
  NSArray* subPaths = [fileManager subpathsOfDirectoryAtPath:rootPath error:&error];
  NSMutableArray* paths = [NSMutableArray arrayWithCapacity:[subPaths count] + 1];
  [paths addObject:@""];
  [paths addObjectsFromArray:subPaths];

  if (error) {
    [pool drain];
    return false;
  }

  for (NSString* currPath in paths) {
    NSString* child = [rootPath stringByAppendingPathComponent:currPath];

    NSDictionary* attributes = [fileManager attributesOfItemAtPath:child error:&error];
    if (error) {
      [pool drain];
      return false;
    }

    // Don't check for writability of files pointed to by symlinks, as they may
    // not be descendants of the root path.
    if ([attributes fileType] != NSFileTypeSymbolicLink &&
        [fileManager isWritableFileAtPath:child] == NO) {
      [pool drain];
      return false;
    }
  }

  [pool drain];
  return true;
}

/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim:set ts=2 sts=2 sw=2 et cin:
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CocoaFileUtils.h"
#include "nsCocoaUtils.h"
#include <Cocoa/Cocoa.h>
#include "nsObjCExceptions.h"
#include "nsDebug.h"

namespace CocoaFileUtils {

nsresult RevealFileInFinder(CFURLRef url)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  if (NS_WARN_IF(!url))
    return NS_ERROR_INVALID_ARG;

  NSAutoreleasePool* ap = [[NSAutoreleasePool alloc] init];
  BOOL success = [[NSWorkspace sharedWorkspace] selectFile:[(NSURL*)url path] inFileViewerRootedAtPath:@""];
  [ap release];

  return (success ? NS_OK : NS_ERROR_FAILURE);

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

nsresult OpenURL(CFURLRef url)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  if (NS_WARN_IF(!url))
    return NS_ERROR_INVALID_ARG;

  NSAutoreleasePool* ap = [[NSAutoreleasePool alloc] init];
  BOOL success = [[NSWorkspace sharedWorkspace] openURL:(NSURL*)url];
  [ap release];

  return (success ? NS_OK : NS_ERROR_FAILURE);

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

nsresult GetFileCreatorCode(CFURLRef url, OSType *creatorCode)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  if (NS_WARN_IF(!url) || NS_WARN_IF(!creatorCode))
    return NS_ERROR_INVALID_ARG;

  nsAutoreleasePool localPool;

  NSString *resolvedPath = [[(NSURL*)url path] stringByResolvingSymlinksInPath];
  if (!resolvedPath) {
    return NS_ERROR_FAILURE;
  }

  NSDictionary* dict = [[NSFileManager defaultManager] attributesOfItemAtPath:resolvedPath error:nil];
  if (!dict) {
    return NS_ERROR_FAILURE;
  }

  NSNumber* creatorNum = (NSNumber*)[dict objectForKey:NSFileHFSCreatorCode];
  if (!creatorNum) {
    return NS_ERROR_FAILURE;
  }

  *creatorCode = [creatorNum unsignedLongValue];
  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

nsresult SetFileCreatorCode(CFURLRef url, OSType creatorCode)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  if (NS_WARN_IF(!url))
    return NS_ERROR_INVALID_ARG;

  NSAutoreleasePool* ap = [[NSAutoreleasePool alloc] init];
  NSDictionary* dict = [NSDictionary dictionaryWithObject:[NSNumber numberWithUnsignedLong:creatorCode] forKey:NSFileHFSCreatorCode];
  BOOL success = [[NSFileManager defaultManager] setAttributes:dict ofItemAtPath:[(NSURL*)url path] error:nil];
  [ap release];
  return (success ? NS_OK : NS_ERROR_FAILURE);

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

nsresult GetFileTypeCode(CFURLRef url, OSType *typeCode)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  if (NS_WARN_IF(!url) || NS_WARN_IF(!typeCode))
    return NS_ERROR_INVALID_ARG;

  nsAutoreleasePool localPool;

  NSString *resolvedPath = [[(NSURL*)url path] stringByResolvingSymlinksInPath];
  if (!resolvedPath) {
    return NS_ERROR_FAILURE;
  }

  NSDictionary* dict = [[NSFileManager defaultManager] attributesOfItemAtPath:resolvedPath error:nil];
  if (!dict) {
    return NS_ERROR_FAILURE;
  }

  NSNumber* typeNum = (NSNumber*)[dict objectForKey:NSFileHFSTypeCode];
  if (!typeNum) {
    return NS_ERROR_FAILURE;
  }

  *typeCode = [typeNum unsignedLongValue];
  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

nsresult SetFileTypeCode(CFURLRef url, OSType typeCode)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  if (NS_WARN_IF(!url))
    return NS_ERROR_INVALID_ARG;

  NSAutoreleasePool* ap = [[NSAutoreleasePool alloc] init];
  NSDictionary* dict = [NSDictionary dictionaryWithObject:[NSNumber numberWithUnsignedLong:typeCode] forKey:NSFileHFSTypeCode];
  BOOL success = [[NSFileManager defaultManager] setAttributes:dict ofItemAtPath:[(NSURL*)url path] error:nil];
  [ap release];
  return (success ? NS_OK : NS_ERROR_FAILURE);

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

} // namespace CocoaFileUtils

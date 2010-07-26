/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim:set ts=2 sts=2 sw=2 et cin:
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Josh Aas <josh@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "CocoaFileUtils.h"
#include <Cocoa/Cocoa.h>
#include "nsObjCExceptions.h"
#include "nsDebug.h"

namespace CocoaFileUtils {

nsresult RevealFileInFinder(CFURLRef url)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  NS_ENSURE_ARG_POINTER(url);

  NSAutoreleasePool* ap = [[NSAutoreleasePool alloc] init];
  BOOL success = [[NSWorkspace sharedWorkspace] selectFile:[(NSURL*)url path] inFileViewerRootedAtPath:@""];
  [ap release];

  return (success ? NS_OK : NS_ERROR_FAILURE);

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

nsresult OpenURL(CFURLRef url)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  NS_ENSURE_ARG_POINTER(url);

  NSAutoreleasePool* ap = [[NSAutoreleasePool alloc] init];
  BOOL success = [[NSWorkspace sharedWorkspace] openURL:(NSURL*)url];
  [ap release];

  return (success ? NS_OK : NS_ERROR_FAILURE);

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

nsresult GetFileCreatorCode(CFURLRef url, OSType *creatorCode)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  NS_ENSURE_ARG_POINTER(url);
  NS_ENSURE_ARG_POINTER(creatorCode);

  nsresult rv = NS_ERROR_FAILURE;

  NSAutoreleasePool* ap = [[NSAutoreleasePool alloc] init];
  NSDictionary* dict = [[NSFileManager defaultManager] fileAttributesAtPath:[(NSURL*)url path] traverseLink:YES];
  NSNumber* creatorNum = (NSNumber*)[dict objectForKey:NSFileHFSCreatorCode];
  if (creatorNum) {
    *creatorCode = [creatorNum unsignedLongValue];
    rv = NS_OK;
  }
  [ap release];

  return rv;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

nsresult SetFileCreatorCode(CFURLRef url, OSType creatorCode)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  NS_ENSURE_ARG_POINTER(url);

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

  NS_ENSURE_ARG_POINTER(url);
  NS_ENSURE_ARG_POINTER(typeCode);

  nsresult rv = NS_ERROR_FAILURE;

  NSAutoreleasePool* ap = [[NSAutoreleasePool alloc] init];
  NSDictionary* dict = [[NSFileManager defaultManager] fileAttributesAtPath:[(NSURL*)url path] traverseLink:YES];
  NSNumber* typeNum = (NSNumber*)[dict objectForKey:NSFileHFSTypeCode];
  if (typeNum) {
    *typeCode = [typeNum unsignedLongValue];
    rv = NS_OK;
  }
  [ap release];

  return rv;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

nsresult SetFileTypeCode(CFURLRef url, OSType typeCode)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  NS_ENSURE_ARG_POINTER(url);

  NSAutoreleasePool* ap = [[NSAutoreleasePool alloc] init];
  NSDictionary* dict = [NSDictionary dictionaryWithObject:[NSNumber numberWithUnsignedLong:typeCode] forKey:NSFileHFSTypeCode];
  BOOL success = [[NSFileManager defaultManager] setAttributes:dict ofItemAtPath:[(NSURL*)url path] error:nil];
  [ap release];
  return (success ? NS_OK : NS_ERROR_FAILURE);

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

} // namespace CocoaFileUtils

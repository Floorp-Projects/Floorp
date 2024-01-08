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
#include "nsString.h"
#include "mozilla/MacStringHelpers.h"

namespace CocoaFileUtils {

nsresult RevealFileInFinder(CFURLRef url) {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  if (NS_WARN_IF(!url)) {
    return NS_ERROR_INVALID_ARG;
  }

  nsAutoreleasePool localPool;

  BOOL success = [[NSWorkspace sharedWorkspace] selectFile:[(NSURL*)url path]
                                  inFileViewerRootedAtPath:@""];

  return (success ? NS_OK : NS_ERROR_FAILURE);

  NS_OBJC_END_TRY_BLOCK_RETURN(NS_ERROR_FAILURE);
}

nsresult OpenURL(CFURLRef url) {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  if (NS_WARN_IF(!url)) {
    return NS_ERROR_INVALID_ARG;
  }

  nsAutoreleasePool localPool;

  BOOL success = [[NSWorkspace sharedWorkspace] openURL:(NSURL*)url];

  return (success ? NS_OK : NS_ERROR_FAILURE);

  NS_OBJC_END_TRY_BLOCK_RETURN(NS_ERROR_FAILURE);
}

nsresult GetFileCreatorCode(CFURLRef url, OSType* creatorCode) {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  if (NS_WARN_IF(!url) || NS_WARN_IF(!creatorCode)) {
    return NS_ERROR_INVALID_ARG;
  }

  nsAutoreleasePool localPool;

  NSString* resolvedPath = [[(NSURL*)url path] stringByResolvingSymlinksInPath];
  if (!resolvedPath) {
    return NS_ERROR_FAILURE;
  }

  NSDictionary* dict =
      [[NSFileManager defaultManager] attributesOfItemAtPath:resolvedPath
                                                       error:nil];
  if (!dict) {
    return NS_ERROR_FAILURE;
  }

  NSNumber* creatorNum = (NSNumber*)[dict objectForKey:NSFileHFSCreatorCode];
  if (!creatorNum) {
    return NS_ERROR_FAILURE;
  }

  *creatorCode = [creatorNum unsignedLongValue];
  return NS_OK;

  NS_OBJC_END_TRY_BLOCK_RETURN(NS_ERROR_FAILURE);
}

nsresult SetFileCreatorCode(CFURLRef url, OSType creatorCode) {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  if (NS_WARN_IF(!url)) {
    return NS_ERROR_INVALID_ARG;
  }

  nsAutoreleasePool localPool;

  NSDictionary* dict = [NSDictionary
      dictionaryWithObject:[NSNumber numberWithUnsignedLong:creatorCode]
                    forKey:NSFileHFSCreatorCode];
  BOOL success =
      [[NSFileManager defaultManager] setAttributes:dict
                                       ofItemAtPath:[(NSURL*)url path]
                                              error:nil];

  return (success ? NS_OK : NS_ERROR_FAILURE);

  NS_OBJC_END_TRY_BLOCK_RETURN(NS_ERROR_FAILURE);
}

nsresult GetFileTypeCode(CFURLRef url, OSType* typeCode) {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  if (NS_WARN_IF(!url) || NS_WARN_IF(!typeCode)) {
    return NS_ERROR_INVALID_ARG;
  }

  nsAutoreleasePool localPool;

  NSString* resolvedPath = [[(NSURL*)url path] stringByResolvingSymlinksInPath];
  if (!resolvedPath) {
    return NS_ERROR_FAILURE;
  }

  NSDictionary* dict =
      [[NSFileManager defaultManager] attributesOfItemAtPath:resolvedPath
                                                       error:nil];
  if (!dict) {
    return NS_ERROR_FAILURE;
  }

  NSNumber* typeNum = (NSNumber*)[dict objectForKey:NSFileHFSTypeCode];
  if (!typeNum) {
    return NS_ERROR_FAILURE;
  }

  *typeCode = [typeNum unsignedLongValue];
  return NS_OK;

  NS_OBJC_END_TRY_BLOCK_RETURN(NS_ERROR_FAILURE);
}

nsresult SetFileTypeCode(CFURLRef url, OSType typeCode) {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  if (NS_WARN_IF(!url)) {
    return NS_ERROR_INVALID_ARG;
  }

  nsAutoreleasePool localPool;

  NSDictionary* dict = [NSDictionary
      dictionaryWithObject:[NSNumber numberWithUnsignedLong:typeCode]
                    forKey:NSFileHFSTypeCode];
  BOOL success =
      [[NSFileManager defaultManager] setAttributes:dict
                                       ofItemAtPath:[(NSURL*)url path]
                                              error:nil];

  return (success ? NS_OK : NS_ERROR_FAILURE);

  NS_OBJC_END_TRY_BLOCK_RETURN(NS_ERROR_FAILURE);
}

// Can be called off of the main thread.
void AddOriginMetadataToFile(const CFStringRef filePath,
                             const CFURLRef sourceURL,
                             const CFURLRef referrerURL) {
  nsAutoreleasePool localPool;

  typedef OSStatus (*MDItemSetAttribute_type)(MDItemRef, CFStringRef,
                                              CFTypeRef);
  static MDItemSetAttribute_type mdItemSetAttributeFunc = NULL;

  static bool did_symbol_lookup = false;
  if (!did_symbol_lookup) {
    did_symbol_lookup = true;

    CFBundleRef metadata_bundle =
        ::CFBundleGetBundleWithIdentifier(CFSTR("com.apple.Metadata"));
    if (!metadata_bundle) {
      return;
    }

    mdItemSetAttributeFunc =
        (MDItemSetAttribute_type)::CFBundleGetFunctionPointerForName(
            metadata_bundle, CFSTR("MDItemSetAttribute"));
  }
  if (!mdItemSetAttributeFunc) {
    return;
  }

  MDItemRef mdItem = ::MDItemCreate(NULL, filePath);
  if (!mdItem) {
    return;
  }

  CFMutableArrayRef list = ::CFArrayCreateMutable(kCFAllocatorDefault, 2, NULL);
  if (!list) {
    ::CFRelease(mdItem);
    return;
  }

  // The first item in the list is the source URL of the downloaded file.
  if (sourceURL) {
    ::CFArrayAppendValue(list, ::CFURLGetString(sourceURL));
  }

  // If the referrer is known, store that in the second position.
  if (referrerURL) {
    ::CFArrayAppendValue(list, ::CFURLGetString(referrerURL));
  }

  mdItemSetAttributeFunc(mdItem, kMDItemWhereFroms, list);

  ::CFRelease(list);
  ::CFRelease(mdItem);
}

// Can be called off of the main thread.
static CFMutableDictionaryRef CreateQuarantineDictionary(
    const CFURLRef aFileURL, const bool aCreateProps) {
  nsAutoreleasePool localPool;

  CFDictionaryRef quarantineProps = NULL;
  if (aCreateProps) {
    quarantineProps = ::CFDictionaryCreate(NULL, NULL, NULL, 0,
                                           &kCFTypeDictionaryKeyCallBacks,
                                           &kCFTypeDictionaryValueCallBacks);
  } else {
    Boolean success = ::CFURLCopyResourcePropertyForKey(
        aFileURL, kCFURLQuarantinePropertiesKey, &quarantineProps, NULL);
    // If there aren't any quarantine properties then the user probably
    // set up an exclusion and we don't need to add metadata.
    if (!success || !quarantineProps) {
      return NULL;
    }
  }

  // We don't know what to do if the props aren't a dictionary.
  if (::CFGetTypeID(quarantineProps) != ::CFDictionaryGetTypeID()) {
    ::CFRelease(quarantineProps);
    return NULL;
  }

  // Make a mutable copy of the properties.
  CFMutableDictionaryRef mutQuarantineProps = ::CFDictionaryCreateMutableCopy(
      kCFAllocatorDefault, 0, (CFDictionaryRef)quarantineProps);
  ::CFRelease(quarantineProps);

  return mutQuarantineProps;
}

// Can be called off of the main thread.
void AddQuarantineMetadataToFile(const CFStringRef filePath,
                                 const CFURLRef sourceURL,
                                 const CFURLRef referrerURL,
                                 const bool isFromWeb,
                                 const bool createProps /* = false */) {
  nsAutoreleasePool localPool;

  CFURLRef fileURL = ::CFURLCreateWithFileSystemPath(
      kCFAllocatorDefault, filePath, kCFURLPOSIXPathStyle, false);

  CFMutableDictionaryRef mutQuarantineProps =
      CreateQuarantineDictionary(fileURL, createProps);
  if (!mutQuarantineProps) {
    ::CFRelease(fileURL);
    return;
  }

  // Add metadata that the OS couldn't infer.

  if (!::CFDictionaryGetValue(mutQuarantineProps, kLSQuarantineTypeKey)) {
    CFStringRef type = isFromWeb ? kLSQuarantineTypeWebDownload
                                 : kLSQuarantineTypeOtherDownload;
    ::CFDictionarySetValue(mutQuarantineProps, kLSQuarantineTypeKey, type);
  }

  if (!::CFDictionaryGetValue(mutQuarantineProps, kLSQuarantineOriginURLKey) &&
      referrerURL) {
    ::CFDictionarySetValue(mutQuarantineProps, kLSQuarantineOriginURLKey,
                           referrerURL);
  }

  if (!::CFDictionaryGetValue(mutQuarantineProps, kLSQuarantineDataURLKey) &&
      sourceURL) {
    ::CFDictionarySetValue(mutQuarantineProps, kLSQuarantineDataURLKey,
                           sourceURL);
  }

  // Set quarantine properties on file.
  ::CFURLSetResourcePropertyForKey(fileURL, kCFURLQuarantinePropertiesKey,
                                   mutQuarantineProps, NULL);

  ::CFRelease(fileURL);
  ::CFRelease(mutQuarantineProps);
}

// Can be called off of the main thread.
void CopyQuarantineReferrerUrl(const CFStringRef aFilePath,
                               nsAString& aReferrer) {
  nsAutoreleasePool localPool;

  CFURLRef fileURL = ::CFURLCreateWithFileSystemPath(
      kCFAllocatorDefault, aFilePath, kCFURLPOSIXPathStyle, false);

  CFMutableDictionaryRef mutQuarantineProps =
      CreateQuarantineDictionary(fileURL, false);
  ::CFRelease(fileURL);
  if (!mutQuarantineProps) {
    return;
  }

  CFTypeRef referrerRef =
      ::CFDictionaryGetValue(mutQuarantineProps, kLSQuarantineOriginURLKey);
  if (referrerRef && ::CFGetTypeID(referrerRef) == ::CFURLGetTypeID()) {
    // URL string must be copied prior to releasing the dictionary.
    mozilla::CopyCocoaStringToXPCOMString(
        (NSString*)::CFURLGetString(static_cast<CFURLRef>(referrerRef)),
        aReferrer);
  }

  ::CFRelease(mutQuarantineProps);
}

CFTypeRefPtr<CFURLRef> GetTemporaryFolder() {
  nsAutoreleasePool localPool;

  NSString* tempDir = ::NSTemporaryDirectory();
  return tempDir == nil ? NULL
                        : CFTypeRefPtr<CFURLRef>::WrapUnderGetRule(
                              (__bridge CFURLRef)[NSURL fileURLWithPath:tempDir
                                                            isDirectory:YES]);
}

CFTypeRefPtr<CFURLRef> GetProductDirectory(bool aLocal) {
  nsAutoreleasePool localPool;

  NSSearchPathDirectory folderType =
      aLocal ? NSCachesDirectory : NSLibraryDirectory;
  NSFileManager* manager = [NSFileManager defaultManager];
  return CFTypeRefPtr<CFURLRef>::WrapUnderGetRule((__bridge CFURLRef)[[manager
      URLsForDirectory:folderType
             inDomains:NSUserDomainMask] firstObject]);
}

}  // namespace CocoaFileUtils

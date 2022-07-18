/* -*- Mode: C++; tab-width: 3; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <sys/types.h>
#include <sys/stat.h>
#include "mozilla/net/NeckoCommon.h"
#include "mozilla/StaticPrefs_browser.h"
#include "nsComponentManagerUtils.h"
#include "nsOSHelperAppService.h"
#include "nsObjCExceptions.h"
#include "nsISupports.h"
#include "nsString.h"
#include "nsTArray.h"
#include "nsIFile.h"
#include "nsILocalFileMac.h"
#include "nsMimeTypes.h"
#include "nsMemory.h"
#include "nsCRT.h"
#include "nsMIMEInfoMac.h"
#include "nsEmbedCID.h"

#import <CoreFoundation/CoreFoundation.h>
#import <ApplicationServices/ApplicationServices.h>

// chrome URL's
#define HELPERAPPLAUNCHER_BUNDLE_URL "chrome://global/locale/helperAppLauncher.properties"
#define BRAND_BUNDLE_URL "chrome://branding/locale/brand.properties"

nsresult GetDefaultBundleURL(const nsACString& aScheme, CFURLRef* aBundleURL) {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  nsresult rv = NS_ERROR_NOT_AVAILABLE;

  CFStringRef schemeCFString = ::CFStringCreateWithBytes(
      kCFAllocatorDefault, (const UInt8*)PromiseFlatCString(aScheme).get(), aScheme.Length(),
      kCFStringEncodingUTF8, false);

  if (schemeCFString) {
    CFStringRef lookupCFString =
        ::CFStringCreateWithFormat(NULL, NULL, CFSTR("%@:"), schemeCFString);

    if (lookupCFString) {
      CFURLRef lookupCFURL = ::CFURLCreateWithString(NULL, lookupCFString, NULL);

      if (lookupCFURL) {
        if (@available(macOS 10.10, *)) {
          *aBundleURL = ::LSCopyDefaultApplicationURLForURL(lookupCFURL, kLSRolesAll, NULL);
          if (*aBundleURL) {
            rv = NS_OK;
          }
        } else {
          OSStatus theErr = ::LSGetApplicationForURL(lookupCFURL, kLSRolesAll, NULL, aBundleURL);
          if (theErr == noErr && *aBundleURL) {
            rv = NS_OK;
          }
        }

        ::CFRelease(lookupCFURL);
      }

      ::CFRelease(lookupCFString);
    }

    ::CFRelease(schemeCFString);
  }

  return rv;

  NS_OBJC_END_TRY_BLOCK_RETURN(NS_ERROR_FAILURE);
}

using mozilla::LogLevel;

/* This is an undocumented interface (in the Foundation framework) that has
 * been stable since at least 10.2.8 and is still present on SnowLeopard.
 * Furthermore WebKit has three public methods (in WebKitSystemInterface.h)
 * that are thin wrappers around this interface's last three methods.  So
 * it's unlikely to change anytime soon.  Now that we're no longer using
 * Internet Config Services, this is the only way to look up a MIME type
 * from an extension, or vice versa.
 */
@class NSURLFileTypeMappingsInternal;

@interface NSURLFileTypeMappings : NSObject {
  NSURLFileTypeMappingsInternal* _internal;
}

+ (NSURLFileTypeMappings*)sharedMappings;
- (NSString*)MIMETypeForExtension:(NSString*)aString;
- (NSString*)preferredExtensionForMIMEType:(NSString*)aString;
- (NSArray*)extensionsForMIMEType:(NSString*)aString;
@end

nsOSHelperAppService::~nsOSHelperAppService() {}

nsresult nsOSHelperAppService::OSProtocolHandlerExists(const char* aProtocolScheme,
                                                       bool* aHandlerExists) {
  // CFStringCreateWithBytes() can fail even if we're not out of memory --
  // for example if the 'bytes' parameter is something very weird (like
  // "\xFF\xFF~"), or possibly if it can't be interpreted as using what's
  // specified in the 'encoding' parameter.  See bug 548719.
  CFStringRef schemeString =
      ::CFStringCreateWithBytes(kCFAllocatorDefault, (const UInt8*)aProtocolScheme,
                                strlen(aProtocolScheme), kCFStringEncodingUTF8, false);
  if (schemeString) {
    // LSCopyDefaultHandlerForURLScheme() can fail to find the default handler
    // for aProtocolScheme when it's never been explicitly set (using
    // LSSetDefaultHandlerForURLScheme()).  For example, Safari is the default
    // handler for the "http" scheme on a newly installed copy of OS X.  But
    // this (presumably) wasn't done using LSSetDefaultHandlerForURLScheme(),
    // so LSCopyDefaultHandlerForURLScheme() will fail to find Safari.  To get
    // around this we use LSCopyAllHandlersForURLScheme() instead -- which seems
    // never to fail.
    // http://lists.apple.com/archives/Carbon-dev/2007/May/msg00349.html
    // http://www.realsoftware.com/listarchives/realbasic-nug/2008-02/msg00119.html
    CFArrayRef handlerArray = ::LSCopyAllHandlersForURLScheme(schemeString);
    *aHandlerExists = !!handlerArray;
    if (handlerArray) ::CFRelease(handlerArray);
    ::CFRelease(schemeString);
  } else {
    *aHandlerExists = false;
  }
  return NS_OK;
}

NS_IMETHODIMP nsOSHelperAppService::GetApplicationDescription(const nsACString& aScheme,
                                                              nsAString& _retval) {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  nsresult rv = NS_ERROR_NOT_AVAILABLE;

  CFURLRef handlerBundleURL;
  rv = GetDefaultBundleURL(aScheme, &handlerBundleURL);

  if (NS_SUCCEEDED(rv) && handlerBundleURL) {
    CFBundleRef handlerBundle = CFBundleCreate(NULL, handlerBundleURL);
    if (!handlerBundle) {
      ::CFRelease(handlerBundleURL);
      return NS_ERROR_OUT_OF_MEMORY;
    }

    // Get the human-readable name of the bundle
    CFStringRef bundleName =
        (CFStringRef)::CFBundleGetValueForInfoDictionaryKey(handlerBundle, kCFBundleNameKey);

    if (bundleName) {
      AutoTArray<UniChar, 255> buffer;
      CFIndex bundleNameLength = ::CFStringGetLength(bundleName);
      buffer.SetLength(bundleNameLength);
      ::CFStringGetCharacters(bundleName, CFRangeMake(0, bundleNameLength), buffer.Elements());
      _retval.Assign(reinterpret_cast<char16_t*>(buffer.Elements()), bundleNameLength);
      rv = NS_OK;
    }
    ::CFRelease(handlerBundle);
    ::CFRelease(handlerBundleURL);
  }

  return rv;

  NS_OBJC_END_TRY_BLOCK_RETURN(NS_ERROR_FAILURE);
}

NS_IMETHODIMP nsOSHelperAppService::IsCurrentAppOSDefaultForProtocol(const nsACString& aScheme,
                                                                     bool* _retval) {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  nsresult rv = NS_ERROR_NOT_AVAILABLE;

  CFURLRef handlerBundleURL;
  rv = GetDefaultBundleURL(aScheme, &handlerBundleURL);
  if (NS_SUCCEEDED(rv) && handlerBundleURL) {
    // Ensure we don't accidentally return success if we can't get an app bundle.
    rv = NS_ERROR_NOT_AVAILABLE;
    CFBundleRef appBundle = ::CFBundleGetMainBundle();
    if (appBundle) {
      CFURLRef selfURL = ::CFBundleCopyBundleURL(appBundle);
      *_retval = ::CFEqual(selfURL, handlerBundleURL);
      rv = NS_OK;
      ::CFRelease(selfURL);
    }
    ::CFRelease(handlerBundleURL);
  }

  return rv;

  NS_OBJC_END_TRY_BLOCK_RETURN(NS_ERROR_FAILURE);
}

nsresult nsOSHelperAppService::GetFileTokenForPath(const char16_t* aPlatformAppPath,
                                                   nsIFile** aFile) {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  nsresult rv;
  nsCOMPtr<nsILocalFileMac> localFile(do_CreateInstance(NS_LOCAL_FILE_CONTRACTID, &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  CFURLRef pathAsCFURL;
  CFStringRef pathAsCFString = ::CFStringCreateWithCharacters(
      NULL, reinterpret_cast<const UniChar*>(aPlatformAppPath), NS_strlen(aPlatformAppPath));
  if (!pathAsCFString) return NS_ERROR_OUT_OF_MEMORY;

  if (::CFStringGetCharacterAtIndex(pathAsCFString, 0) == '/') {
    // we have a Posix path
    pathAsCFURL =
        ::CFURLCreateWithFileSystemPath(nullptr, pathAsCFString, kCFURLPOSIXPathStyle, false);
    if (!pathAsCFURL) {
      ::CFRelease(pathAsCFString);
      return NS_ERROR_OUT_OF_MEMORY;
    }
  } else {
    // if it doesn't start with a / it's not an absolute Posix path
    // let's check if it's a HFS path left over from old preferences

    // If it starts with a ':' char, it's not an absolute HFS path
    // so bail for that, and also if it's empty
    if (::CFStringGetLength(pathAsCFString) == 0 ||
        ::CFStringGetCharacterAtIndex(pathAsCFString, 0) == ':') {
      ::CFRelease(pathAsCFString);
      return NS_ERROR_FILE_UNRECOGNIZED_PATH;
    }

    pathAsCFURL =
        ::CFURLCreateWithFileSystemPath(nullptr, pathAsCFString, kCFURLHFSPathStyle, false);
    if (!pathAsCFURL) {
      ::CFRelease(pathAsCFString);
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }

  rv = localFile->InitWithCFURL(pathAsCFURL);
  ::CFRelease(pathAsCFString);
  ::CFRelease(pathAsCFURL);
  if (NS_FAILED(rv)) return rv;
  *aFile = localFile;
  NS_IF_ADDREF(*aFile);

  return NS_OK;

  NS_OBJC_END_TRY_BLOCK_RETURN(NS_ERROR_FAILURE);
}

// Returns the MIME types an application bundle explicitly claims to handle.
// Returns NULL if aAppRef doesn't explicitly claim to handle any MIME types.
// If the return value is non-NULL, the caller is responsible for freeing it.
// This isn't necessarily the same as the MIME types the application bundle
// is registered to handle in the Launch Services database.  (For example
// the Preview application is normally registered to handle the application/pdf
// MIME type, even though it doesn't explicitly claim to handle *any* MIME
// types in its Info.plist.  This is probably because Preview does explicitly
// claim to handle the com.adobe.pdf UTI, and Launch Services somehow
// translates this into a claim to support the application/pdf MIME type.
// Launch Services doesn't provide any APIs (documented or undocumented) to
// query which MIME types a given application is registered to handle.  So any
// app that wants this information (e.g. the Default Apps pref pane) needs to
// iterate through the entire Launch Services database -- a process which can
// take several seconds.)
static CFArrayRef GetMIMETypesHandledByApp(FSRef* aAppRef) {
  CFURLRef appURL = ::CFURLCreateFromFSRef(kCFAllocatorDefault, aAppRef);
  if (!appURL) {
    return NULL;
  }
  CFDictionaryRef infoDict = ::CFBundleCopyInfoDictionaryForURL(appURL);
  ::CFRelease(appURL);
  if (!infoDict) {
    return NULL;
  }
  CFTypeRef cfObject = ::CFDictionaryGetValue(infoDict, CFSTR("CFBundleDocumentTypes"));
  if (!cfObject || (::CFGetTypeID(cfObject) != ::CFArrayGetTypeID())) {
    ::CFRelease(infoDict);
    return NULL;
  }

  CFArrayRef docTypes = static_cast<CFArrayRef>(cfObject);
  CFIndex docTypesCount = ::CFArrayGetCount(docTypes);
  if (docTypesCount == 0) {
    ::CFRelease(infoDict);
    return NULL;
  }

  CFMutableArrayRef mimeTypes =
      ::CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
  for (CFIndex i = 0; i < docTypesCount; ++i) {
    cfObject = ::CFArrayGetValueAtIndex(docTypes, i);
    if (!cfObject || (::CFGetTypeID(cfObject) != ::CFDictionaryGetTypeID())) {
      continue;
    }
    CFDictionaryRef typeDict = static_cast<CFDictionaryRef>(cfObject);

    // When this key is present (on OS X 10.5 and later), its contents
    // take precedence over CFBundleTypeMIMETypes (and CFBundleTypeExtensions
    // and CFBundleTypeOSTypes).
    cfObject = ::CFDictionaryGetValue(typeDict, CFSTR("LSItemContentTypes"));
    if (cfObject && (::CFGetTypeID(cfObject) == ::CFArrayGetTypeID())) {
      continue;
    }

    cfObject = ::CFDictionaryGetValue(typeDict, CFSTR("CFBundleTypeMIMETypes"));
    if (!cfObject || (::CFGetTypeID(cfObject) != ::CFArrayGetTypeID())) {
      continue;
    }
    CFArrayRef mimeTypeHolder = static_cast<CFArrayRef>(cfObject);
    CFArrayAppendArray(mimeTypes, mimeTypeHolder,
                       ::CFRangeMake(0, ::CFArrayGetCount(mimeTypeHolder)));
  }

  ::CFRelease(infoDict);
  if (!::CFArrayGetCount(mimeTypes)) {
    ::CFRelease(mimeTypes);
    mimeTypes = NULL;
  }
  return mimeTypes;
}

nsresult nsOSHelperAppService::GetMIMEInfoFromOS(const nsACString& aMIMEType,
                                                 const nsACString& aFileExt, bool* aFound,
                                                 nsIMIMEInfo** aMIMEInfo) {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;
  MOZ_ASSERT(XRE_IsParentProcess());

  *aFound = false;

  const nsCString& flatType = PromiseFlatCString(aMIMEType);
  const nsCString& flatExt = PromiseFlatCString(aFileExt);

  MOZ_LOG(sLog, LogLevel::Debug,
          ("Mac: HelperAppService lookup for type '%s' ext '%s'\n", flatType.get(), flatExt.get()));

  // Create a Mac-specific MIME info so we can use Mac-specific members.
  RefPtr<nsMIMEInfoMac> mimeInfoMac = new nsMIMEInfoMac(aMIMEType);

  NSAutoreleasePool* localPool = [[NSAutoreleasePool alloc] init];

  OSStatus err;
  bool haveAppForType = false;
  bool haveAppForExt = false;
  bool typeIsOctetStream = false;
  bool typeAppIsDefault = false;
  bool extAppIsDefault = false;
  FSRef typeAppFSRef;
  FSRef extAppFSRef;

  CFStringRef cfMIMEType = NULL;

  if (!aMIMEType.IsEmpty()) {
    typeIsOctetStream = aMIMEType.LowerCaseEqualsLiteral(APPLICATION_OCTET_STREAM);
    CFURLRef appURL = NULL;
    // CFStringCreateWithCString() can fail even if we're not out of memory --
    // for example if the 'cStr' parameter is something very weird (like
    // "\xFF\xFF~"), or possibly if it can't be interpreted as using what's
    // specified in the 'encoding' parameter.  See bug 548719.
    cfMIMEType = ::CFStringCreateWithCString(NULL, flatType.get(), kCFStringEncodingUTF8);
    if (cfMIMEType) {
      err = ::LSCopyApplicationForMIMEType(cfMIMEType, kLSRolesAll, &appURL);
      if ((err == noErr) && appURL && ::CFURLGetFSRef(appURL, &typeAppFSRef)) {
        haveAppForType = true;
        MOZ_LOG(sLog, LogLevel::Debug,
                ("LSCopyApplicationForMIMEType found a default application\n"));
      }
      if (appURL) {
        ::CFRelease(appURL);
      }
    }
  }
  if (!aFileExt.IsEmpty()) {
    // CFStringCreateWithCString() can fail even if we're not out of memory --
    // for example if the 'cStr' parameter is something very weird (like
    // "\xFF\xFF~"), or possibly if it can't be interpreted as using what's
    // specified in the 'encoding' parameter.  See bug 548719.
    CFStringRef cfExt = ::CFStringCreateWithCString(NULL, flatExt.get(), kCFStringEncodingUTF8);
    if (cfExt) {
      err = ::LSGetApplicationForInfo(kLSUnknownType, kLSUnknownCreator, cfExt, kLSRolesAll,
                                      &extAppFSRef, nullptr);
      if (err == noErr) {
        haveAppForExt = true;
        MOZ_LOG(sLog, LogLevel::Debug, ("LSGetApplicationForInfo found a default application\n"));
      }
      ::CFRelease(cfExt);
    }
  }

  if (haveAppForType && haveAppForExt) {
    // Do aMIMEType and aFileExt match?
    if (::FSCompareFSRefs((const FSRef*)&typeAppFSRef, (const FSRef*)&extAppFSRef) == noErr) {
      typeAppIsDefault = true;
      *aFound = true;
    }
  } else if (haveAppForType) {
    // If aFileExt isn't empty, it doesn't match aMIMEType.
    if (aFileExt.IsEmpty()) {
      typeAppIsDefault = true;
      *aFound = true;
    }
  }

  // If we have an app for the extension, and either don't have one for the
  // type, or the type is application/octet-stream (ie "binary blob"), rely
  // on the file extension.
  if ((!haveAppForType || (!*aFound && typeIsOctetStream)) && haveAppForExt) {
    // If aMIMEType isn't empty, it doesn't match aFileExt, which should mean
    // that we haven't found a matching app.  But make an exception for an app
    // that also explicitly claims to handle aMIMEType, or which doesn't claim
    // to handle any MIME types.  This helps work around the following Apple
    // design flaw:
    //
    // Launch Services is somewhat unreliable about registering Apple apps to
    // handle MIME types.  Probably this is because Apple has officially
    // deprecated support for MIME types (in favor of UTIs).  As a result,
    // most of Apple's own apps don't explicitly claim to handle any MIME
    // types (instead they claim to handle one or more UTIs).  So Launch
    // Services must contain logic to translate support for a given UTI into
    // support for one or more MIME types, and it doesn't always do this
    // correctly.  For example DiskImageMounter isn't (by default) registered
    // to handle the application/x-apple-diskimage MIME type.  See bug 675356.
    //
    // Apple has also deprecated support for file extensions, and Apple apps
    // also don't register to handle them.  But for some reason Launch Services
    // is (apparently) better about translating support for a given UTI into
    // support for one or more file extensions.  It's not at all clear why.
    if (aMIMEType.IsEmpty() || typeIsOctetStream) {
      extAppIsDefault = true;
      *aFound = true;
    } else {
      CFArrayRef extAppMIMETypes = GetMIMETypesHandledByApp(&extAppFSRef);
      if (extAppMIMETypes) {
        if (cfMIMEType) {
          if (::CFArrayContainsValue(extAppMIMETypes,
                                     ::CFRangeMake(0, ::CFArrayGetCount(extAppMIMETypes)),
                                     cfMIMEType)) {
            extAppIsDefault = true;
            *aFound = true;
          }
        }
        ::CFRelease(extAppMIMETypes);
      } else {
        extAppIsDefault = true;
        *aFound = true;
      }
    }
  }

  if (cfMIMEType) {
    ::CFRelease(cfMIMEType);
  }

  if (aMIMEType.IsEmpty()) {
    if (haveAppForExt) {
      // If aMIMEType is empty and we've found a default app for aFileExt, try
      // to get the MIME type from aFileExt.  (It might also be worth doing
      // this when aMIMEType isn't empty but haveAppForType is false -- but
      // the doc for this method says that if we have a MIME type (in
      // aMIMEType), we need to give it preference.)
      NSURLFileTypeMappings* map = [NSURLFileTypeMappings sharedMappings];
      NSString* extStr = [NSString stringWithCString:flatExt.get() encoding:NSASCIIStringEncoding];
      NSString* typeStr = map ? [map MIMETypeForExtension:extStr] : NULL;
      if (typeStr) {
        nsAutoCString mimeType;
        mimeType.Assign((char*)[typeStr cStringUsingEncoding:NSASCIIStringEncoding]);
        mimeInfoMac->SetMIMEType(mimeType);
        haveAppForType = true;
      } else {
        // Sometimes the OS won't give us a MIME type for an extension that's
        // registered with Launch Services and has a default app:  For example
        // Real Player registers itself for the "ogg" extension and for the
        // audio/x-ogg and application/x-ogg MIME types, but
        // MIMETypeForExtension returns nil for the "ogg" extension even on
        // systems where Real Player is installed.  This is probably an Apple
        // bug.  But bad things happen if we return an nsIMIMEInfo structure
        // with an empty MIME type and set *aFound to true.  So in this
        // case we need to set it to false here.
        haveAppForExt = false;
        extAppIsDefault = false;
        *aFound = false;
      }
    } else {
      // Otherwise set the MIME type to a reasonable fallback.
      mimeInfoMac->SetMIMEType(nsLiteralCString(APPLICATION_OCTET_STREAM));
    }
  }

  if (typeAppIsDefault || extAppIsDefault) {
    if (haveAppForExt) mimeInfoMac->AppendExtension(aFileExt);

    nsresult rv;
    nsCOMPtr<nsILocalFileMac> app(do_CreateInstance(NS_LOCAL_FILE_CONTRACTID, &rv));
    if (NS_FAILED(rv)) {
      [localPool release];
      return rv;
    }

    CFStringRef cfAppName = NULL;
    if (typeAppIsDefault) {
      app->InitWithFSRef(&typeAppFSRef);
      ::LSCopyItemAttribute((const FSRef*)&typeAppFSRef, kLSRolesAll, kLSItemDisplayName,
                            (CFTypeRef*)&cfAppName);
    } else {
      app->InitWithFSRef(&extAppFSRef);
      ::LSCopyItemAttribute((const FSRef*)&extAppFSRef, kLSRolesAll, kLSItemDisplayName,
                            (CFTypeRef*)&cfAppName);
    }
    if (cfAppName) {
      AutoTArray<UniChar, 255> buffer;
      CFIndex appNameLength = ::CFStringGetLength(cfAppName);
      buffer.SetLength(appNameLength);
      ::CFStringGetCharacters(cfAppName, CFRangeMake(0, appNameLength), buffer.Elements());
      nsAutoString appName;
      appName.Assign(reinterpret_cast<char16_t*>(buffer.Elements()), appNameLength);
      mimeInfoMac->SetDefaultDescription(appName);
      ::CFRelease(cfAppName);
    }

    mimeInfoMac->SetDefaultApplication(app);

    mozilla::StaticPrefs::browser_download_improvements_to_download_panel()
        ? mimeInfoMac->SetPreferredAction(nsIMIMEInfo::saveToDisk)
        : mimeInfoMac->SetPreferredAction(nsIMIMEInfo::useSystemDefault);

  } else {
    mimeInfoMac->SetPreferredAction(nsIMIMEInfo::saveToDisk);
  }

  nsAutoCString mimeType;
  mimeInfoMac->GetMIMEType(mimeType);
  if (*aFound && !mimeType.IsEmpty()) {
    // If we have a MIME type, make sure its extension list is included in our
    // list.
    NSURLFileTypeMappings* map = [NSURLFileTypeMappings sharedMappings];
    NSString* typeStr = [NSString stringWithCString:mimeType.get() encoding:NSASCIIStringEncoding];
    NSArray* extensionsList = map ? [map extensionsForMIMEType:typeStr] : NULL;
    if (extensionsList) {
      for (NSString* extension in extensionsList) {
        nsAutoCString ext;
        ext.Assign((char*)[extension cStringUsingEncoding:NSASCIIStringEncoding]);
        mimeInfoMac->AppendExtension(ext);
      }
    }

    if (CFStringRef cfType =
            ::CFStringCreateWithCString(NULL, mimeType.get(), kCFStringEncodingUTF8)) {
      if (CFStringRef cfTypeDesc = ::UTTypeCopyDescription(cfType)) {
        AutoTArray<UniChar, 255> buffer;
        CFIndex typeDescLength = ::CFStringGetLength(cfTypeDesc);
        buffer.SetLength(typeDescLength);
        ::CFStringGetCharacters(cfTypeDesc, CFRangeMake(0, typeDescLength), buffer.Elements());
        nsAutoString typeDesc;
        typeDesc.Assign(reinterpret_cast<char16_t*>(buffer.Elements()), typeDescLength);
        mimeInfoMac->SetDescription(typeDesc);
        ::CFRelease(cfTypeDesc);
      }
      ::CFRelease(cfType);
    }
  }

  MOZ_LOG(sLog, LogLevel::Debug, ("OS gave us: type '%s' found '%i'\n", mimeType.get(), *aFound));

  [localPool release];
  mimeInfoMac.forget(aMIMEInfo);
  return NS_OK;

  NS_OBJC_END_TRY_BLOCK_RETURN(NS_ERROR_FAILURE);
}

NS_IMETHODIMP
nsOSHelperAppService::GetProtocolHandlerInfoFromOS(const nsACString& aScheme, bool* found,
                                                   nsIHandlerInfo** _retval) {
  NS_ASSERTION(!aScheme.IsEmpty(), "No scheme was specified!");

  nsresult rv = OSProtocolHandlerExists(nsPromiseFlatCString(aScheme).get(), found);
  if (NS_FAILED(rv)) return rv;

  nsMIMEInfoMac* handlerInfo = new nsMIMEInfoMac(aScheme, nsMIMEInfoBase::eProtocolInfo);
  NS_ENSURE_TRUE(handlerInfo, NS_ERROR_OUT_OF_MEMORY);
  NS_ADDREF(*_retval = handlerInfo);

  if (!*found) {
    // Code that calls this requires an object regardless if the OS has
    // something for us, so we return the empty object.
    return NS_OK;
  }

  // As a workaround for the OS X problem described in bug 1391186, don't
  // attempt to get/set the application description from the child process.
  if (!mozilla::net::IsNeckoChild()) {
    nsAutoString desc;
    rv = GetApplicationDescription(aScheme, desc);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "GetApplicationDescription failed");
    handlerInfo->SetDefaultDescription(desc);
  }

  return NS_OK;
}

/* -*- Mode: C++; tab-width: 3; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is the Mozilla browser.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications, Inc.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Scott MacGregor <mscott@netscape.com>
 *   Dan Mosedale <dmose@mozilla.org>
 *   Stan Shebs <stanshebs@earthlink.net>
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

#include <sys/types.h>
#include <sys/stat.h>
#include "nsOSHelperAppService.h"
#include "nsObjCExceptions.h"
#include "nsISupports.h"
#include "nsString.h"
#include "nsTArray.h"
#include "nsXPIDLString.h"
#include "nsIURL.h"
#include "nsILocalFile.h"
#include "nsILocalFileMac.h"
#include "nsMimeTypes.h"
#include "nsIStringBundle.h"
#include "nsIPromptService.h"
#include "nsMemory.h"
#include "nsCRT.h"
#include "nsMIMEInfoMac.h"
#include "nsEmbedCID.h"

#import <CoreFoundation/CoreFoundation.h>
#import <ApplicationServices/ApplicationServices.h>

// chrome URL's
#define HELPERAPPLAUNCHER_BUNDLE_URL "chrome://global/locale/helperAppLauncher.properties"
#define BRAND_BUNDLE_URL "chrome://branding/locale/brand.properties"

extern "C" {
  // Returns the CFURL for application currently set as the default opener for
  // the given URL scheme. appURL must be released by the caller.
  extern OSStatus _LSCopyDefaultSchemeHandlerURL(CFStringRef scheme,
                                                 CFURLRef *appURL);
}

/* This is an undocumented interface (in the Foundation framework) that has
 * been stable since at least 10.2.8 and is still present on SnowLeopard.
 * Furthermore WebKit has three public methods (in WebKitSystemInterface.h)
 * that are thin wrappers around this interface's last three methods.  So
 * it's unlikely to change anytime soon.  Now that we're no longer using
 * Internet Config Services, this is the only way to look up a MIME type
 * from an extension, or vice versa.
 */
@class NSURLFileTypeMappingsInternal;

@interface NSURLFileTypeMappings : NSObject
{
    NSURLFileTypeMappingsInternal *_internal;
}

+ (NSURLFileTypeMappings*)sharedMappings;
- (NSString*)MIMETypeForExtension:(NSString*)aString;
- (NSString*)preferredExtensionForMIMEType:(NSString*)aString;
- (NSArray*)extensionsForMIMEType:(NSString*)aString;
@end

nsOSHelperAppService::nsOSHelperAppService() : nsExternalHelperAppService()
{
  mode_t mask = umask(0777);
  umask(mask);
  mPermissions = 0666 & ~mask;
}

nsOSHelperAppService::~nsOSHelperAppService()
{}

nsresult nsOSHelperAppService::OSProtocolHandlerExists(const char * aProtocolScheme, bool * aHandlerExists)
{
  // CFStringCreateWithBytes() can fail even if we're not out of memory --
  // for example if the 'bytes' parameter is something very wierd (like "ÿÿ~"
  // aka "\xFF\xFF~"), or possibly if it can't be interpreted as using what's
  // specified in the 'encoding' parameter.  See bug 548719.
  CFStringRef schemeString = ::CFStringCreateWithBytes(kCFAllocatorDefault,
                                                       (const UInt8*)aProtocolScheme,
                                                       strlen(aProtocolScheme),
                                                       kCFStringEncodingUTF8,
                                                       false);
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
    if (handlerArray)
      ::CFRelease(handlerArray);
    ::CFRelease(schemeString);
  } else {
    *aHandlerExists = PR_FALSE;
  }
  return NS_OK;
}

NS_IMETHODIMP nsOSHelperAppService::GetApplicationDescription(const nsACString& aScheme, nsAString& _retval)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  nsresult rv = NS_ERROR_NOT_AVAILABLE;

  CFStringRef schemeCFString = 
    ::CFStringCreateWithBytes(kCFAllocatorDefault,
                              (const UInt8 *)PromiseFlatCString(aScheme).get(),
                              aScheme.Length(),
                              kCFStringEncodingUTF8,
                              false);
  if (schemeCFString) {
    // Since the public API (LSGetApplicationForURL) fails every now and then,
    // we're using undocumented _LSCopyDefaultSchemeHandlerURL
    CFURLRef handlerBundleURL;
    OSStatus err = ::_LSCopyDefaultSchemeHandlerURL(schemeCFString,
                                                    &handlerBundleURL);
    if (err == noErr) {
      CFBundleRef handlerBundle = ::CFBundleCreate(NULL, handlerBundleURL);
      if (handlerBundle) {
        // Get the human-readable name of the default handler bundle
        CFStringRef bundleName =
          (CFStringRef)::CFBundleGetValueForInfoDictionaryKey(handlerBundle,
                                                              kCFBundleNameKey);
        if (bundleName) {
          nsAutoTArray<UniChar, 255> buffer;
          CFIndex bundleNameLength = ::CFStringGetLength(bundleName);
          buffer.SetLength(bundleNameLength);
          ::CFStringGetCharacters(bundleName, CFRangeMake(0, bundleNameLength),
                                  buffer.Elements());
          _retval.Assign(buffer.Elements(), bundleNameLength);
          rv = NS_OK;
        }

        ::CFRelease(handlerBundle);
      }

      ::CFRelease(handlerBundleURL);
    }

    ::CFRelease(schemeCFString);
  }

  return rv;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

nsresult nsOSHelperAppService::GetFileTokenForPath(const PRUnichar * aPlatformAppPath, nsIFile ** aFile)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  nsresult rv;
  nsCOMPtr<nsILocalFileMac> localFile (do_CreateInstance(NS_LOCAL_FILE_CONTRACTID, &rv));
  NS_ENSURE_SUCCESS(rv,rv);

  CFURLRef pathAsCFURL;
  CFStringRef pathAsCFString = ::CFStringCreateWithCharacters(NULL,
                                                              aPlatformAppPath,
                                                              nsCRT::strlen(aPlatformAppPath));
  if (!pathAsCFString)
    return NS_ERROR_OUT_OF_MEMORY;

  if (::CFStringGetCharacterAtIndex(pathAsCFString, 0) == '/') {
    // we have a Posix path
    pathAsCFURL = ::CFURLCreateWithFileSystemPath(nsnull, pathAsCFString,
                                                  kCFURLPOSIXPathStyle, PR_FALSE);
    if (!pathAsCFURL) {
      ::CFRelease(pathAsCFString);
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }
  else {
    // if it doesn't start with a / it's not an absolute Posix path
    // let's check if it's a HFS path left over from old preferences

    // If it starts with a ':' char, it's not an absolute HFS path
    // so bail for that, and also if it's empty
    if (::CFStringGetLength(pathAsCFString) == 0 ||
        ::CFStringGetCharacterAtIndex(pathAsCFString, 0) == ':')
    {
      ::CFRelease(pathAsCFString);
      return NS_ERROR_FILE_UNRECOGNIZED_PATH;
    }

    pathAsCFURL = ::CFURLCreateWithFileSystemPath(nsnull, pathAsCFString,
                                                  kCFURLHFSPathStyle, PR_FALSE);
    if (!pathAsCFURL) {
      ::CFRelease(pathAsCFString);
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }

  rv = localFile->InitWithCFURL(pathAsCFURL);
  ::CFRelease(pathAsCFString);
  ::CFRelease(pathAsCFURL);
  if (NS_FAILED(rv))
    return rv;
  *aFile = localFile;
  NS_IF_ADDREF(*aFile);

  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

NS_IMETHODIMP nsOSHelperAppService::GetFromTypeAndExtension(const nsACString& aType, const nsACString& aFileExt, nsIMIMEInfo ** aMIMEInfo)
{
  return nsExternalHelperAppService::GetFromTypeAndExtension(aType, aFileExt, aMIMEInfo);
}

// aMIMEType and aFileExt might not match,  If they don't we set *aFound to
// PR_FALSE and return a minimal nsIMIMEInfo structure.
already_AddRefed<nsIMIMEInfo>
nsOSHelperAppService::GetMIMEInfoFromOS(const nsACString& aMIMEType,
                                        const nsACString& aFileExt,
                                        bool * aFound)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSNULL;

  *aFound = PR_FALSE;

  const nsCString& flatType = PromiseFlatCString(aMIMEType);
  const nsCString& flatExt = PromiseFlatCString(aFileExt);

  PR_LOG(mLog, PR_LOG_DEBUG, ("Mac: HelperAppService lookup for type '%s' ext '%s'\n",
                              flatType.get(), flatExt.get()));

  // Create a Mac-specific MIME info so we can use Mac-specific members.
  nsMIMEInfoMac* mimeInfoMac = new nsMIMEInfoMac(aMIMEType);
  if (!mimeInfoMac)
    return nsnull;
  NS_ADDREF(mimeInfoMac);

  NSAutoreleasePool *localPool = [[NSAutoreleasePool alloc] init];

  OSStatus err;
  bool haveAppForType = false;
  bool haveAppForExt = false;
  bool typeAppIsDefault = false;
  bool extAppIsDefault = false;
  FSRef typeAppFSRef;
  FSRef extAppFSRef;

  if (!aMIMEType.IsEmpty()) {
    CFURLRef appURL = NULL;
    // CFStringCreateWithCString() can fail even if we're not out of memory --
    // for example if the 'cStr' parameter is something very wierd (like "ÿÿ~"
    // aka "\xFF\xFF~"), or possibly if it can't be interpreted as using what's
    // specified in the 'encoding' parameter.  See bug 548719.
    CFStringRef CFType = ::CFStringCreateWithCString(NULL, flatType.get(), kCFStringEncodingUTF8);
    if (CFType) {
      err = ::LSCopyApplicationForMIMEType(CFType, kLSRolesAll, &appURL);
      if ((err == noErr) && appURL && ::CFURLGetFSRef(appURL, &typeAppFSRef)) {
        haveAppForType = PR_TRUE;
        PR_LOG(mLog, PR_LOG_DEBUG, ("LSCopyApplicationForMIMEType found a default application\n"));
      }
      if (appURL)
        ::CFRelease(appURL);
      ::CFRelease(CFType);
    }
  }
  if (!aFileExt.IsEmpty()) {
    // CFStringCreateWithCString() can fail even if we're not out of memory --
    // for example if the 'cStr' parameter is something very wierd (like "ÿÿ~"
    // aka "\xFF\xFF~"), or possibly if it can't be interpreted as using what's
    // specified in the 'encoding' parameter.  See bug 548719.
    CFStringRef CFExt = ::CFStringCreateWithCString(NULL, flatExt.get(), kCFStringEncodingUTF8);
    if (CFExt) {
      err = ::LSGetApplicationForInfo(kLSUnknownType, kLSUnknownCreator, CFExt,
                                      kLSRolesAll, &extAppFSRef, nsnull);
      if (err == noErr) {
        haveAppForExt = PR_TRUE;
        PR_LOG(mLog, PR_LOG_DEBUG, ("LSGetApplicationForInfo found a default application\n"));
      }
      ::CFRelease(CFExt);
    }
  }

  if (haveAppForType && haveAppForExt) {
    // Do aMIMEType and aFileExt match?
    if (::FSCompareFSRefs((const FSRef *) &typeAppFSRef, (const FSRef *) &extAppFSRef) == noErr) {
      typeAppIsDefault = PR_TRUE;
      *aFound = PR_TRUE;
    }
  } else if (haveAppForType) {
    // If aFileExt isn't empty, it doesn't match aMIMEType.
    if (aFileExt.IsEmpty()) {
      typeAppIsDefault = PR_TRUE;
      *aFound = PR_TRUE;
    }
  } else if (haveAppForExt) {
    // If aMIMEType isn't empty, it doesn't match aFileExt.
    if (aMIMEType.IsEmpty()) {
      extAppIsDefault = PR_TRUE;
      *aFound = PR_TRUE;
    }
  }

  if (aMIMEType.IsEmpty()) {
    if (haveAppForExt) {
      // If aMIMEType is empty and we've found a default app for aFileExt, try
      // to get the MIME type from aFileExt.  (It might also be worth doing
      // this when aMIMEType isn't empty but haveAppForType is false -- but
      // the doc for this method says that if we have a MIME type (in
      // aMIMEType), we need to give it preference.)
      NSURLFileTypeMappings *map = [NSURLFileTypeMappings sharedMappings];
      NSString *extStr = [NSString stringWithCString:flatExt.get() encoding:NSASCIIStringEncoding];
      NSString *typeStr = map ? [map MIMETypeForExtension:extStr] : NULL;
      if (typeStr) {
        nsCAutoString mimeType;
        mimeType.Assign((char *)[typeStr cStringUsingEncoding:NSASCIIStringEncoding]);
        mimeInfoMac->SetMIMEType(mimeType);
        haveAppForType = PR_TRUE;
      } else {
        // Sometimes the OS won't give us a MIME type for an extension that's
        // registered with Launch Services and has a default app:  For example
        // Real Player registers itself for the "ogg" extension and for the
        // audio/x-ogg and application/x-ogg MIME types, but
        // MIMETypeForExtension returns nil for the "ogg" extension even on
        // systems where Real Player is installed.  This is probably an Apple
        // bug.  But bad things happen if we return an nsIMIMEInfo structure
        // with an empty MIME type and set *aFound to PR_TRUE.  So in this
        // case we need to set it to PR_FALSE here.
        haveAppForExt = PR_FALSE;
        extAppIsDefault = PR_FALSE;
        *aFound = PR_FALSE;
      }
    } else {
      // Otherwise set the MIME type to a reasonable fallback.
      mimeInfoMac->SetMIMEType(NS_LITERAL_CSTRING(APPLICATION_OCTET_STREAM));
    }
  }

  if (typeAppIsDefault || extAppIsDefault) {
    if (haveAppForExt)
      mimeInfoMac->AppendExtension(aFileExt);

    nsCOMPtr<nsILocalFileMac> app(do_CreateInstance(NS_LOCAL_FILE_CONTRACTID));
    if (!app) {
      NS_RELEASE(mimeInfoMac);
      [localPool release];
      return nsnull;
    }

    CFStringRef CFAppName = NULL;
    if (typeAppIsDefault) {
      app->InitWithFSRef(&typeAppFSRef);
      ::LSCopyItemAttribute((const FSRef *) &typeAppFSRef, kLSRolesAll,
                            kLSItemDisplayName, (CFTypeRef *) &CFAppName);
    } else {
      app->InitWithFSRef(&extAppFSRef);
      ::LSCopyItemAttribute((const FSRef *) &extAppFSRef, kLSRolesAll,
                            kLSItemDisplayName, (CFTypeRef *) &CFAppName);
    }
    if (CFAppName) {
      nsAutoTArray<UniChar, 255> buffer;
      CFIndex appNameLength = ::CFStringGetLength(CFAppName);
      buffer.SetLength(appNameLength);
      ::CFStringGetCharacters(CFAppName, CFRangeMake(0, appNameLength),
                              buffer.Elements());
      nsAutoString appName;
      appName.Assign(buffer.Elements(), appNameLength);
      mimeInfoMac->SetDefaultDescription(appName);
      ::CFRelease(CFAppName);
    }

    mimeInfoMac->SetDefaultApplication(app);
    mimeInfoMac->SetPreferredAction(nsIMIMEInfo::useSystemDefault);
  } else {
    mimeInfoMac->SetPreferredAction(nsIMIMEInfo::saveToDisk);
  }

  nsCAutoString mimeType;
  mimeInfoMac->GetMIMEType(mimeType);
  if (*aFound && !mimeType.IsEmpty()) {
    // If we have a MIME type, make sure its preferred extension is included
    // in our list.
    NSURLFileTypeMappings *map = [NSURLFileTypeMappings sharedMappings];
    NSString *typeStr = [NSString stringWithCString:mimeType.get() encoding:NSASCIIStringEncoding];
    NSString *extStr = map ? [map preferredExtensionForMIMEType:typeStr] : NULL;
    if (extStr) {
      nsCAutoString preferredExt;
      preferredExt.Assign((char *)[extStr cStringUsingEncoding:NSASCIIStringEncoding]);
      mimeInfoMac->AppendExtension(preferredExt);
    }

    CFStringRef CFType = ::CFStringCreateWithCString(NULL, mimeType.get(), kCFStringEncodingUTF8);
    CFStringRef CFTypeDesc = NULL;
    if (::LSCopyKindStringForMIMEType(CFType, &CFTypeDesc) == noErr) {
      nsAutoTArray<UniChar, 255> buffer;
      CFIndex typeDescLength = ::CFStringGetLength(CFTypeDesc);
      buffer.SetLength(typeDescLength);
      ::CFStringGetCharacters(CFTypeDesc, CFRangeMake(0, typeDescLength),
                              buffer.Elements());
      nsAutoString typeDesc;
      typeDesc.Assign(buffer.Elements(), typeDescLength);
      mimeInfoMac->SetDescription(typeDesc);
    }
    if (CFTypeDesc)
      ::CFRelease(CFTypeDesc);
    ::CFRelease(CFType);
  }

  PR_LOG(mLog, PR_LOG_DEBUG, ("OS gave us: type '%s' found '%i'\n", mimeType.get(), *aFound));

  [localPool release];
  return mimeInfoMac;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSNULL;
}

NS_IMETHODIMP
nsOSHelperAppService::GetProtocolHandlerInfoFromOS(const nsACString &aScheme,
                                                   bool *found,
                                                   nsIHandlerInfo **_retval)
{
  NS_ASSERTION(!aScheme.IsEmpty(), "No scheme was specified!");

  nsresult rv = OSProtocolHandlerExists(nsPromiseFlatCString(aScheme).get(),
                                        found);
  if (NS_FAILED(rv))
    return rv;

  nsMIMEInfoMac *handlerInfo =
    new nsMIMEInfoMac(aScheme, nsMIMEInfoBase::eProtocolInfo);
  NS_ENSURE_TRUE(handlerInfo, NS_ERROR_OUT_OF_MEMORY);
  NS_ADDREF(*_retval = handlerInfo);

  if (!*found) {
    // Code that calls this requires an object regardless if the OS has
    // something for us, so we return the empty object.
    return NS_OK;
  }

  nsAutoString desc;
  GetApplicationDescription(aScheme, desc);
  handlerInfo->SetDefaultDescription(desc);

  return NS_OK;
}

void
nsOSHelperAppService::FixFilePermissions(nsILocalFile* aFile)
{
  aFile->SetPermissions(mPermissions);
}

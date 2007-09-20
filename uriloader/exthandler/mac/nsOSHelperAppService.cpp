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

#include "nsOSHelperAppService.h"
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

#include "nsIInternetConfigService.h"
#include "nsEmbedCID.h"
#include <LaunchServices.h>

// chrome URL's
#define HELPERAPPLAUNCHER_BUNDLE_URL "chrome://global/locale/helperAppLauncher.properties"
#define BRAND_BUNDLE_URL "chrome://branding/locale/brand.properties"

extern "C" {
  // Returns the CFURL for application currently set as the default opener for
  // the given URL scheme. appURL must be released by the caller.
  extern OSStatus _LSCopyDefaultSchemeHandlerURL(CFStringRef scheme,
                                                 CFURLRef *appURL);
}

nsOSHelperAppService::nsOSHelperAppService() : nsExternalHelperAppService()
{
}

nsOSHelperAppService::~nsOSHelperAppService()
{}

nsresult nsOSHelperAppService::OSProtocolHandlerExists(const char * aProtocolScheme, PRBool * aHandlerExists)
{
  // look up the protocol scheme in Internet Config....if we find a match then we have a handler for it...
  *aHandlerExists = PR_FALSE;
  // ask the internet config service to look it up for us...
  nsresult rv = NS_OK;
  nsCOMPtr<nsIInternetConfigService> icService (do_GetService(NS_INTERNETCONFIGSERVICE_CONTRACTID));
  if (icService)
  {
    rv = icService->HasProtocolHandler(aProtocolScheme, aHandlerExists);
    if (rv == NS_ERROR_NOT_AVAILABLE)
    {
      // There is a protocol handler, but it's the current app!  We can't let
      // the current app handle the protocol, as that'll get us into an infinite
      // loop, so we just pretend there's no protocol handler available.
      *aHandlerExists = PR_FALSE;
      rv = NS_OK;

      // FIXME: instead of pretending there's no protocol handler available,
      // let the caller know about the loop so it can deal with the problem
      // (i.e. either fix it automatically, if there's some way to do that,
      // or just provide the user with options for fixing it manually).
    }
  }
  return rv;
}

NS_IMETHODIMP nsOSHelperAppService::GetApplicationDescription(const nsACString& aScheme, nsAString& _retval)
{
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
}

nsresult nsOSHelperAppService::GetFileTokenForPath(const PRUnichar * aPlatformAppPath, nsIFile ** aFile)
{
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
}
///////////////////////////
// method overrides --> use internet config information for mime type lookup.
///////////////////////////

NS_IMETHODIMP nsOSHelperAppService::GetFromTypeAndExtension(const nsACString& aType, const nsACString& aFileExt, nsIMIMEInfo ** aMIMEInfo)
{
  // first, ask our base class....
  nsresult rv = nsExternalHelperAppService::GetFromTypeAndExtension(aType, aFileExt, aMIMEInfo);
  if (NS_SUCCEEDED(rv) && *aMIMEInfo) 
  {
    UpdateCreatorInfo(*aMIMEInfo);
  }
  return rv;
}

already_AddRefed<nsIMIMEInfo>
nsOSHelperAppService::GetMIMEInfoFromOS(const nsACString& aMIMEType,
                                        const nsACString& aFileExt,
                                        PRBool * aFound)
{
  nsIMIMEInfo* mimeInfo = nsnull;
  *aFound = PR_TRUE;

  const nsCString& flatType = PromiseFlatCString(aMIMEType);
  const nsCString& flatExt = PromiseFlatCString(aFileExt);

  // ask the internet config service to look it up for us...
  nsCOMPtr<nsIInternetConfigService> icService (do_GetService(NS_INTERNETCONFIGSERVICE_CONTRACTID));
  PR_LOG(mLog, PR_LOG_DEBUG, ("Mac: HelperAppService lookup for type '%s' ext '%s' (IC: 0x%p)\n",
                              flatType.get(), flatExt.get(), icService.get()));
  if (icService)
  {
    nsCOMPtr<nsIMIMEInfo> miByType, miByExt;
    if (!aMIMEType.IsEmpty())
      icService->FillInMIMEInfo(flatType.get(), flatExt.get(), getter_AddRefs(miByType));

    PRBool hasDefault = PR_FALSE;
    if (miByType)
      miByType->GetHasDefaultHandler(&hasDefault);

    if (!aFileExt.IsEmpty() && (!hasDefault || !miByType)) {
      icService->GetMIMEInfoFromExtension(flatExt.get(), getter_AddRefs(miByExt));
      if (miByExt && !aMIMEType.IsEmpty()) {
        // XXX see XXX comment below
        nsIMIMEInfo* pByExt = miByExt.get();
        nsMIMEInfoBase* byExt = static_cast<nsMIMEInfoBase*>(pByExt);
        byExt->SetMIMEType(aMIMEType);
      }
    }
    PR_LOG(mLog, PR_LOG_DEBUG, ("OS gave us: By Type: 0x%p By Ext: 0x%p type has default: %s\n",
                                miByType.get(), miByExt.get(), hasDefault ? "true" : "false"));

    // If we got two matches, and the type has no default app, copy default app
    if (!hasDefault && miByType && miByExt) {
      // IC currently always uses nsMIMEInfoBase-derived classes.
      // When it stops doing that, this code will need changing.
      // XXX This assumes that IC will give os an nsMIMEInfoBase. I'd use
      // dynamic_cast but that crashes.
      // XXX these pBy* variables are needed because .get() returns an
      // nsDerivedSafe thingy that can't be cast to nsMIMEInfoBase*
      nsIMIMEInfo* pByType = miByType.get();
      nsIMIMEInfo* pByExt = miByExt.get();
      nsMIMEInfoBase* byType = static_cast<nsMIMEInfoBase*>(pByType);
      nsMIMEInfoBase* byExt = static_cast<nsMIMEInfoBase*>(pByExt);
      if (!byType || !byExt) {
        NS_ERROR("IC gave us an nsIMIMEInfo that's no nsMIMEInfoBase! Fix nsOSHelperAppService.");
        return nsnull;
      }
      // Copy the attributes of miByType onto miByExt
      byType->CopyBasicDataTo(byExt);
      miByType = miByExt;
    }
    if (miByType)
      miByType.swap(mimeInfo);
    else if (miByExt)
      miByExt.swap(mimeInfo);
  }

  if (!mimeInfo) {
    *aFound = PR_FALSE;
    PR_LOG(mLog, PR_LOG_DEBUG, ("Creating new mimeinfo\n"));
    mimeInfo = new nsMIMEInfoMac(aMIMEType);
    if (!mimeInfo)
      return nsnull;
    NS_ADDREF(mimeInfo);

    if (!aFileExt.IsEmpty())
      mimeInfo->AppendExtension(aFileExt);
  }
  
  return mimeInfo;
}

already_AddRefed<nsIHandlerInfo>
nsOSHelperAppService::GetProtocolInfoFromOS(const nsACString &aScheme,
                                            PRBool *found)
{
  NS_ASSERTION(!aScheme.IsEmpty(), "No scheme was specified!");

  nsresult rv = OSProtocolHandlerExists(nsPromiseFlatCString(aScheme).get(),
                                        found);
  if (NS_FAILED(rv))
    return nsnull;

  nsMIMEInfoMac *handlerInfo =
    new nsMIMEInfoMac(aScheme, nsMIMEInfoBase::eProtocolInfo);
  NS_ENSURE_TRUE(handlerInfo, nsnull);
  NS_ADDREF(handlerInfo);

  if (!*found) {
    // Code that calls this requires an object regardless if the OS has
    // something for us, so we return the empty object.
    return handlerInfo;
  }

  nsAutoString desc;
  GetApplicationDescription(aScheme, desc);
  handlerInfo->SetDefaultDescription(desc);

  return handlerInfo;
}

// we never want to use a hard coded value for the creator and file type for the mac. always look these values up
// from internet config.
void nsOSHelperAppService::UpdateCreatorInfo(nsIMIMEInfo * aMIMEInfo)
{
  PRUint32 macCreatorType;
  PRUint32 macFileType;
  aMIMEInfo->GetMacType(&macFileType);
  aMIMEInfo->GetMacCreator(&macCreatorType);
  
  if (macFileType == 0 || macCreatorType == 0)
  {
    // okay these values haven't been initialized yet so fetch a mime object from internet config.
    nsCAutoString mimeType;
    aMIMEInfo->GetMIMEType(mimeType);
    nsCOMPtr<nsIInternetConfigService> icService (do_GetService(NS_INTERNETCONFIGSERVICE_CONTRACTID));
    if (icService)
    {
      nsCOMPtr<nsIMIMEInfo> osMimeObject;
      icService->FillInMIMEInfo(mimeType.get(), nsnull, getter_AddRefs(osMimeObject));
      if (osMimeObject)
      {
        osMimeObject->GetMacType(&macFileType);
        osMimeObject->GetMacCreator(&macCreatorType);
        aMIMEInfo->SetMacCreator(macCreatorType);
        aMIMEInfo->SetMacType(macFileType);
      } // if we got an os object
    } // if we got the ic service
  } // if the creator or file type hasn't been initialized yet
} 

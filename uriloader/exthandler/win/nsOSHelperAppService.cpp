/* -*- Mode: C++; tab-width: 3; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim:set ts=2 sts=2 sw=2 et cin:
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
#include "nsXPIDLString.h"
#include "nsIURL.h"
#include "nsIMIMEInfo.h"
#include "nsMIMEInfoWin.h"
#include "nsMimeTypes.h"
#include "nsILocalFileWin.h"
#include "nsIProcess.h"
#include "plstr.h"
#include "nsAutoPtr.h"
#include "nsNativeCharsetUtils.h"
#include "nsIWindowsRegKey.h"

// we need windows.h to read out registry information...
#include <windows.h>

// shellapi.h is needed to build with WIN32_LEAN_AND_MEAN
#include <shellapi.h>

#define LOG(args) PR_LOG(mLog, PR_LOG_DEBUG, args)

// helper methods: forward declarations...
static nsresult GetExtensionFrom4xRegistryInfo(const nsACString& aMimeType, 
                                               nsString& aFileExtension);
static nsresult GetExtensionFromWindowsMimeDatabase(const nsACString& aMimeType,
                                                    nsString& aFileExtension);

nsOSHelperAppService::nsOSHelperAppService() : nsExternalHelperAppService()
{}

nsOSHelperAppService::~nsOSHelperAppService()
{}

// The windows registry provides a mime database key which lists a set of mime types and corresponding "Extension" values. 
// we can use this to look up our mime type to see if there is a preferred extension for the mime type.
static nsresult GetExtensionFromWindowsMimeDatabase(const nsACString& aMimeType,
                                                    nsString& aFileExtension)
{
  nsAutoString mimeDatabaseKey;
  mimeDatabaseKey.AssignLiteral("MIME\\Database\\Content Type\\");

  AppendASCIItoUTF16(aMimeType, mimeDatabaseKey);

  nsCOMPtr<nsIWindowsRegKey> regKey = 
    do_CreateInstance("@mozilla.org/windows-registry-key;1");
  if (!regKey) 
    return NS_ERROR_NOT_AVAILABLE;

  nsresult rv = regKey->Open(nsIWindowsRegKey::ROOT_KEY_CLASSES_ROOT,
                             mimeDatabaseKey,
                             nsIWindowsRegKey::ACCESS_QUERY_VALUE);
  
  if (NS_SUCCEEDED(rv))
     regKey->ReadStringValue(NS_LITERAL_STRING("Extension"), aFileExtension);

  return NS_OK;
}

// We have a serious problem!! I have this content type and the windows registry only gives me
// helper apps based on extension. Right now, we really don't have a good place to go for 
// trying to figure out the extension for a particular mime type....One short term hack is to look
// this information in 4.x (it's stored in the windows regsitry). 
static nsresult GetExtensionFrom4xRegistryInfo(const nsACString& aMimeType,
                                               nsString& aFileExtension)
{
  nsCOMPtr<nsIWindowsRegKey> regKey = 
    do_CreateInstance("@mozilla.org/windows-registry-key;1");
  if (!regKey) 
    return NS_ERROR_NOT_AVAILABLE;

  nsresult rv = regKey->
    Open(nsIWindowsRegKey::ROOT_KEY_CURRENT_USER,
         NS_LITERAL_STRING("Software\\Netscape\\Netscape Navigator\\Suffixes"),
         nsIWindowsRegKey::ACCESS_QUERY_VALUE);
  if (NS_FAILED(rv))
    return NS_ERROR_NOT_AVAILABLE;
   
  rv = regKey->ReadStringValue(NS_ConvertASCIItoUTF16(aMimeType),
                               aFileExtension);
  if (NS_FAILED(rv))
    return NS_OK;

  aFileExtension.Insert(PRUnichar('.'), 0);
      
  // this may be a comma separated list of extensions...just take the 
  // first one for now...

  PRInt32 pos = aFileExtension.FindChar(PRUnichar(','));
  if (pos > 0) {
    // we have a comma separated list of types...
    // truncate everything after the first comma (including the comma)
    aFileExtension.Truncate(pos); 
  }
   
  return NS_OK;
}

nsresult nsOSHelperAppService::OSProtocolHandlerExists(const char * aProtocolScheme, PRBool * aHandlerExists)
{
  // look up the protocol scheme in the windows registry....if we find a match then we have a handler for it...
  *aHandlerExists = PR_FALSE;
  if (aProtocolScheme && *aProtocolScheme)
  {
     HKEY hKey;
     LONG err = ::RegOpenKeyEx(HKEY_CLASSES_ROOT, aProtocolScheme, 0,
                               KEY_QUERY_VALUE, &hKey);
     if (err == ERROR_SUCCESS)
     {
       err = ::RegQueryValueEx(hKey, "URL Protocol", NULL, NULL, NULL, NULL);
       *aHandlerExists = (err == ERROR_SUCCESS);
       // close the key
       ::RegCloseKey(hKey);
     }
  }

  return NS_OK;
}

NS_IMETHODIMP nsOSHelperAppService::GetApplicationDescription(const nsACString& aScheme, nsAString& _retval)
{
  nsCOMPtr<nsIWindowsRegKey> regKey = 
    do_CreateInstance("@mozilla.org/windows-registry-key;1");
  if (!regKey) 
    return NS_ERROR_NOT_AVAILABLE;

  NS_ConvertASCIItoUTF16 buf(aScheme);

  nsCOMPtr<nsIFile> app;
  GetDefaultAppInfo(buf, _retval, getter_AddRefs(app));

  if (!_retval.Equals(buf))
    return NS_OK;

  // Fall back to full path
  buf.AppendLiteral("\\shell\\open\\command");
  nsresult rv = regKey->Open(nsIWindowsRegKey::ROOT_KEY_CLASSES_ROOT,
                             buf,
                             nsIWindowsRegKey::ACCESS_QUERY_VALUE);
  if (NS_FAILED(rv))
    return NS_ERROR_NOT_AVAILABLE;   
   
  rv = regKey->ReadStringValue(EmptyString(), _retval); 

  return NS_SUCCEEDED(rv) ? NS_OK : NS_ERROR_NOT_AVAILABLE;
}

// GetMIMEInfoFromRegistry: This function obtains the values of some of the nsIMIMEInfo
// attributes for the mimeType/extension associated with the input registry key.  The default
// entry for that key is the name of a registry key under HKEY_CLASSES_ROOT.  The default
// value for *that* key is the descriptive name of the type.  The EditFlags value is a binary
// value; the low order bit of the third byte of which indicates that the user does not need
// to be prompted.
//
// This function sets only the Description attribute of the input nsIMIMEInfo.
/* static */
nsresult nsOSHelperAppService::GetMIMEInfoFromRegistry(const nsAFlatString& fileType, nsIMIMEInfo *pInfo)
{
  nsresult rv = NS_OK;

  NS_ENSURE_ARG(pInfo);
  nsCOMPtr<nsIWindowsRegKey> regKey = 
    do_CreateInstance("@mozilla.org/windows-registry-key;1");
  if (!regKey) 
    return NS_ERROR_NOT_AVAILABLE;

  rv = regKey->Open(nsIWindowsRegKey::ROOT_KEY_CLASSES_ROOT,
                    fileType, nsIWindowsRegKey::ACCESS_QUERY_VALUE);
  if (NS_FAILED(rv))
    return NS_ERROR_FAILURE;
 
  // OK, the default value here is the description of the type.
  nsAutoString description;
  rv = regKey->ReadStringValue(EmptyString(), description);
  if (NS_SUCCEEDED(rv))
    pInfo->SetDescription(description);

  return NS_OK;
}

/////////////////////////////////////////////////////////////////////////////////////////////////
// method overrides used to gather information from the windows registry for
// various mime types. 
////////////////////////////////////////////////////////////////////////////////////////////////

/// Looks up the type for the extension aExt and compares it to aType
/* static */ PRBool
nsOSHelperAppService::typeFromExtEquals(const PRUnichar* aExt, const char *aType)
{
  if (!aType)
    return PR_FALSE;
  nsAutoString fileExtToUse;
  if (aExt[0] != PRUnichar('.'))
    fileExtToUse = PRUnichar('.');

  fileExtToUse.Append(aExt);

  PRBool eq = PR_FALSE;
  nsCOMPtr<nsIWindowsRegKey> regKey = 
    do_CreateInstance("@mozilla.org/windows-registry-key;1");
  if (!regKey) 
    return eq;

  nsresult rv = regKey->Open(nsIWindowsRegKey::ROOT_KEY_CLASSES_ROOT,
                             fileExtToUse,
                             nsIWindowsRegKey::ACCESS_QUERY_VALUE);
  if (NS_FAILED(rv))
      return eq;
   
  nsAutoString type;
  rv = regKey->ReadStringValue(NS_LITERAL_STRING("Content Type"), type);
  if (NS_SUCCEEDED(rv))
     eq = type.EqualsASCII(aType);

  return eq;
}

static void RemoveParameters(nsString& aPath)
{
  // Command Strings stored in the Windows registry with parameters look like 
  // this:
  //
  // 1) "C:\Program Files\Company Name\product.exe" -foo -bar  (long version)
  //                      -- OR --
  // 2) C:\PROGRA~1\COMPAN~2\product.exe -foo -bar             (short version)
  //
  // For 1), the path is the first "" quoted string. (quotes are used to 
  //         prevent parameter parsers from choking)
  // For 2), the path is the string up until the first space (spaces are 
  //         illegal in short DOS-style paths)
  //
  if (aPath.First() == PRUnichar('"')) {
    aPath = Substring(aPath, 1, aPath.Length() - 1);
    PRInt32 nextQuote = aPath.FindChar(PRUnichar('"'));
    if (nextQuote != kNotFound)
      aPath.Truncate(nextQuote);
  }
  else {
    PRInt32 firstSpace = aPath.FindChar(PRUnichar(' '));
    if (firstSpace != kNotFound) 
      aPath.Truncate(firstSpace);
  }
}

//
// The "real" name of a given helper app (as specified by the path to the 
// executable file held in various registry keys) is stored n the VERSIONINFO
// block in the file's resources. We need to find the path to the executable
// and then retrieve the "FileDescription" field value from the file. 
//
// For a given extension, we find the file handler like so:
//
// HKCR
//     \.ext\                           <type key>     <-- default value
//     \<type key>\   
//                \shell\open\command\  <path+params>  <-- default value
//
// We need to do some parsing on the <path+params> to strip off params and
// deal with some Windows quirks (like the fact that many Shell "applications"
// are actually DLLs invoked via rundll32.exe) 
//
nsresult
nsOSHelperAppService::GetDefaultAppInfo(const nsAString& aTypeName,
                                        nsAString& aDefaultDescription, 
                                        nsIFile** aDefaultApplication)
{
  // If all else fails, use the file type key name, which will be 
  // something like "pngfile" for .pngs, "WMVFile" for .wmvs, etc. 
  aDefaultDescription = aTypeName;
  *aDefaultApplication = nsnull;

  nsAutoString handlerKeyName(aTypeName);
  handlerKeyName.AppendLiteral("\\shell\\open\\command");
  nsCOMPtr<nsIWindowsRegKey> regKey = 
    do_CreateInstance("@mozilla.org/windows-registry-key;1");
  if (!regKey) 
    return NS_OK;

  nsresult rv = regKey->Open(nsIWindowsRegKey::ROOT_KEY_CLASSES_ROOT,
                             handlerKeyName, 
                             nsIWindowsRegKey::ACCESS_QUERY_VALUE);
  if (NS_FAILED(rv))
    return NS_OK;
   
  // OK, the default value here is the description of the type.
  nsAutoString handlerCommand;
  rv = regKey->ReadStringValue(EmptyString(), handlerCommand);
  if (NS_FAILED(rv))
    return NS_OK;

  if (!CleanupCmdHandlerPath(handlerCommand))
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsILocalFile> lf;
  NS_NewLocalFile(handlerCommand, PR_TRUE, getter_AddRefs(lf));
  if (!lf)
    return NS_ERROR_OUT_OF_MEMORY;

  nsILocalFileWin* lfw = nsnull;
  CallQueryInterface(lf, &lfw);
  if (lfw) {
    // The "FileDescription" field contains the actual name of the application.
    lfw->GetVersionInfoField("FileDescription", aDefaultDescription);
    // QI addref'ed for us.
    *aDefaultApplication = lfw;
  }

  return NS_OK;
}

// Returns the dll path of a rundll command handler. 
/* static */ PRBool nsOSHelperAppService::CleanupCmdHandlerPath(nsAString& aCommandHandler)
{
  nsAutoString handlerFilePath;
  nsAutoString handlerCommand(aCommandHandler);

  // %SystemRoot%\system32\NOTEPAD.EXE %1
  //
  // rundll32.exe "%ProgramFiles%\Windows Photo Gallery\
  //   PhotoViewer.dll", ImageView_Fullscreen %1
  //
  // rundll32.exe "%ProgramFiles%\Windows Photo Gallery\PhotoViewer.dll"
  //
  // %SystemRoot%\System32\rundll32.exe "%ProgramFiles%\
  //   Windows Photo Gallery\PhotoViewer.dll", ImageView_Fullscreen %1

  // Replace embedded environment variables.
  PRUint32 bufLength = ::ExpandEnvironmentStringsW(handlerCommand.get(),
                                                   L"", 0);
  if (bufLength == 0) // Error
    return PR_FALSE;

  nsAutoArrayPtr<PRUnichar> destination(new PRUnichar[bufLength]);
  if (!destination)
    return PR_FALSE;
  if (!::ExpandEnvironmentStringsW(handlerCommand.get(), destination,
                                   bufLength))
    return PR_FALSE;

  handlerCommand = destination;

  // Look to see if we're invoking a Windows shell service, such as 
  // the Picture & Fax Viewer, which are invoked through rundll32.exe.
  //
  // What we want is the DLL - since that's where the real application name
  // is stored, e.g. zipfldr.dll, shimgvw.dll, etc. 
  // 
  // Working from the end of the registry value, the path begins at the last
  // comma in the string (stripping off Function and args) to the position
  // just after the first space (the space after rundll32.exe).

  NS_NAMED_LITERAL_STRING(rundllSegment, "rundll32.exe ");

  PRInt32 index = handlerCommand.Find(rundllSegment);
  if (index != kNotFound) {
    // Strip off the rundll command handler
    PRInt32 lastCommaPos = handlerCommand.RFindChar(',');
    PRUint32 rundllSegmentLength = index + rundllSegment.Length();
    PRUint32 len;

    if (lastCommaPos == kNotFound) {
      // C:\Windows\System32\rundll32.exe "C:\Program Files\
      //   Windows Photo Gallery\PhotoViewer.dll"
      // rundll32.exe "C:\Program Files\Windows Photo Gallery\PhotoViewer.dll"
      len = handlerCommand.Length() - rundllSegmentLength;
    } else {
      // C:\Windows\System32\rundll32.exe "C:\Program Files\
      //   Windows Photo Gallery\PhotoViewer.dll", ImageView_Fullscreen %1
      // rundll32.exe "C:\Program Files\Windows Photo Gallery\
      //   PhotoViewer.dll", ImageView_Fullscreen %1
      len = lastCommaPos - rundllSegmentLength;
    }

    // Clip off 'C:\Windows\System32\rundll32.exe ' or 'rundll32.exe '
    handlerFilePath = Substring(handlerCommand, rundllSegmentLength, len);
  } else {
    handlerFilePath = handlerCommand;
  }

  // Trim any command parameters so that we have a native path we can
  // initialize a local file with.
  RemoveParameters(handlerFilePath);

  aCommandHandler = handlerFilePath;
  return PR_TRUE;
}

already_AddRefed<nsMIMEInfoWin> nsOSHelperAppService::GetByExtension(const nsAFlatString& aFileExt, const char *aTypeHint)
{
  if (aFileExt.IsEmpty())
    return nsnull;

  // windows registry assumes your file extension is going to include the '.'.
  // so make sure it's there...
  nsAutoString fileExtToUse;
  if (aFileExt.First() != PRUnichar('.'))
    fileExtToUse = PRUnichar('.');

  fileExtToUse.Append(aFileExt);

  // o.t. try to get an entry from the windows registry.
  nsCOMPtr<nsIWindowsRegKey> regKey = 
    do_CreateInstance("@mozilla.org/windows-registry-key;1");
  if (!regKey) 
    return nsnull; 

  nsresult rv = regKey->Open(nsIWindowsRegKey::ROOT_KEY_CLASSES_ROOT,
                             fileExtToUse,
                             nsIWindowsRegKey::ACCESS_QUERY_VALUE);
  if (NS_FAILED(rv))
    return nsnull; 

  nsCAutoString typeToUse;
  if (aTypeHint && *aTypeHint) {
    typeToUse.Assign(aTypeHint);
  }
  else {
    nsAutoString temp;
    if (NS_FAILED(regKey->ReadStringValue(NS_LITERAL_STRING("Content Type"),
                  temp))) {
      return nsnull; 
    }
    // Content-Type is always in ASCII
    LossyAppendUTF16toASCII(temp, typeToUse);
  }

  nsMIMEInfoWin* mimeInfo = new nsMIMEInfoWin(typeToUse);
  if (!mimeInfo)
    return nsnull; // out of memory

  NS_ADDREF(mimeInfo);

  // don't append the '.'
  mimeInfo->AppendExtension(NS_ConvertUTF16toUTF8(Substring(fileExtToUse, 1)));

  mimeInfo->SetPreferredAction(nsIMIMEInfo::useSystemDefault);

  nsAutoString description;
  PRBool found = NS_SUCCEEDED(regKey->ReadStringValue(EmptyString(),
                                                      description));

  // Bug 358297 - ignore the default handler, force the user to choose app
  if (description.EqualsLiteral("XPSViewer.Document")) {
    NS_IF_RELEASE(mimeInfo);
    return nsnull;
  }

  nsAutoString defaultDescription;
  nsCOMPtr<nsIFile> defaultApplication;
  GetDefaultAppInfo(description, defaultDescription,
                    getter_AddRefs(defaultApplication));

  mimeInfo->SetDefaultDescription(defaultDescription);
  mimeInfo->SetDefaultApplicationHandler(defaultApplication);

  // Get other nsIMIMEInfo fields from registry, if possible.
  if (found)
  {
      GetMIMEInfoFromRegistry(description, mimeInfo);
  }
  else {
    NS_IF_RELEASE(mimeInfo); // we failed to really find an entry in the registry
  }

  return mimeInfo;
}

already_AddRefed<nsIMIMEInfo> nsOSHelperAppService::GetMIMEInfoFromOS(const nsACString& aMIMEType, const nsACString& aFileExt, PRBool *aFound)
{
  *aFound = PR_TRUE;

  const nsCString& flatType = PromiseFlatCString(aMIMEType);
  const nsCString& flatExt = PromiseFlatCString(aFileExt);

  nsAutoString fileExtension;
  /* XXX The Equals is a gross hack to wallpaper over the most common Win32
   * extension issues caused by the fix for bug 116938.  See bug
   * 120327, comment 271 for why this is needed.  Not even sure we
   * want to remove this once we have fixed all this stuff to work
   * right; any info we get from the OS on this type is pretty much
   * useless....
   * We'll do extension-based lookup for this type later in this function.
   */
  if (!aMIMEType.LowerCaseEqualsLiteral(APPLICATION_OCTET_STREAM)) {
    // (1) try to use the windows mime database to see if there is a mapping to a file extension
    // (2) try to see if we have some left over 4.x registry info we can peek at...
    GetExtensionFromWindowsMimeDatabase(aMIMEType, fileExtension);
    LOG(("Windows mime database: extension '%s'\n", fileExtension.get()));
    if (fileExtension.IsEmpty()) {
      GetExtensionFrom4xRegistryInfo(aMIMEType, fileExtension);
      LOG(("4.x Registry: extension '%s'\n", fileExtension.get()));
    }
  }
  // If we found an extension for the type, do the lookup
  nsMIMEInfoWin* mi = nsnull;
  if (!fileExtension.IsEmpty())
    mi = GetByExtension(fileExtension, flatType.get()).get();
  LOG(("Extension lookup on '%s' found: 0x%p\n", fileExtension.get(), mi));

  PRBool hasDefault = PR_FALSE;
  if (mi) {
    mi->GetHasDefaultHandler(&hasDefault);
    // OK. We might have the case that |aFileExt| is a valid extension for the
    // mimetype we were given. In that case, we do want to append aFileExt
    // to the mimeinfo that we have. (E.g.: We are asked for video/mpeg and
    // .mpg, but the primary extension for video/mpeg is .mpeg. But because
    // .mpg is an extension for video/mpeg content, we want to append it)
    if (!aFileExt.IsEmpty() && typeFromExtEquals(NS_ConvertUTF8toUTF16(flatExt).get(), flatType.get())) {
      LOG(("Appending extension '%s' to mimeinfo, because its mimetype is '%s'\n",
           flatExt.get(), flatType.get()));
      PRBool extExist = PR_FALSE;
      mi->ExtensionExists(aFileExt, &extExist);
      if (!extExist)
        mi->AppendExtension(aFileExt);
    }
  }
  if (!mi || !hasDefault) {
    nsRefPtr<nsMIMEInfoWin> miByExt =
      GetByExtension(NS_ConvertUTF8toUTF16(aFileExt), flatType.get());
    LOG(("Ext. lookup for '%s' found 0x%p\n", flatExt.get(), miByExt.get()));
    if (!miByExt && mi)
      return mi;
    if (miByExt && !mi) {
      miByExt.swap(mi);
      return mi;
    }
    if (!miByExt && !mi) {
      *aFound = PR_FALSE;
      mi = new nsMIMEInfoWin(flatType);
      if (mi) {
        NS_ADDREF(mi);
        if (!aFileExt.IsEmpty())
          mi->AppendExtension(aFileExt);
      }
      
      return mi;
    }

    // if we get here, mi has no default app. copy from extension lookup.
    nsCOMPtr<nsIFile> defaultApp;
    nsAutoString desc;
    miByExt->GetDefaultDescription(desc);

    mi->SetDefaultDescription(desc);
  }
  return mi;
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

  nsMIMEInfoWin *handlerInfo =
    new nsMIMEInfoWin(aScheme, nsMIMEInfoBase::eProtocolInfo);
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


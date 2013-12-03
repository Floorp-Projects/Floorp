/* -*- Mode: C++; tab-width: 3; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim:set ts=2 sts=2 sw=2 et cin:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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

// shellapi.h is needed to build with WIN32_LEAN_AND_MEAN
#include <shellapi.h>

#define LOG(args) PR_LOG(mLog, PR_LOG_DEBUG, args)

// helper methods: forward declarations...
static nsresult GetExtensionFrom4xRegistryInfo(const nsACString& aMimeType, 
                                               nsString& aFileExtension);
static nsresult GetExtensionFromWindowsMimeDatabase(const nsACString& aMimeType,
                                                    nsString& aFileExtension);

nsOSHelperAppService::nsOSHelperAppService() : 
  nsExternalHelperAppService()
  , mAppAssoc(nullptr)
{
  CoInitialize(nullptr);
  CoCreateInstance(CLSID_ApplicationAssociationRegistration, nullptr,
                   CLSCTX_INPROC, IID_IApplicationAssociationRegistration,
                   (void**)&mAppAssoc);
}

nsOSHelperAppService::~nsOSHelperAppService()
{
  if (mAppAssoc)
    mAppAssoc->Release();
  mAppAssoc = nullptr;
  CoUninitialize();
}

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

  int32_t pos = aFileExtension.FindChar(PRUnichar(','));
  if (pos > 0) {
    // we have a comma separated list of types...
    // truncate everything after the first comma (including the comma)
    aFileExtension.Truncate(pos); 
  }
   
  return NS_OK;
}

nsresult nsOSHelperAppService::OSProtocolHandlerExists(const char * aProtocolScheme, bool * aHandlerExists)
{
  // look up the protocol scheme in the windows registry....if we find a match then we have a handler for it...
  *aHandlerExists = false;
  if (aProtocolScheme && *aProtocolScheme)
  {
    // Vista: use new application association interface
    if (mAppAssoc) {
      wchar_t * pResult = nullptr;
      NS_ConvertASCIItoUTF16 scheme(aProtocolScheme);
      // We are responsible for freeing returned strings.
      HRESULT hr = mAppAssoc->QueryCurrentDefault(scheme.get(),
                                                  AT_URLPROTOCOL, AL_EFFECTIVE,
                                                  &pResult);
      if (SUCCEEDED(hr)) {
        CoTaskMemFree(pResult);
        *aHandlerExists = true;
      }
      return NS_OK;
    }

    HKEY hKey;
    LONG err = ::RegOpenKeyExW(HKEY_CLASSES_ROOT,
                               NS_ConvertASCIItoUTF16(aProtocolScheme).get(),
                               0,
                               KEY_QUERY_VALUE,
                               &hKey);
    if (err == ERROR_SUCCESS)
    {
      err = ::RegQueryValueExW(hKey, L"URL Protocol",
                               nullptr, nullptr, nullptr, nullptr);
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

  // Vista: use new application association interface
  if (mAppAssoc) {
    wchar_t * pResult = nullptr;
    // We are responsible for freeing returned strings.
    HRESULT hr = mAppAssoc->QueryCurrentDefault(buf.get(),
                                                AT_URLPROTOCOL, AL_EFFECTIVE,
                                                &pResult);
    if (SUCCEEDED(hr)) {
      nsCOMPtr<nsIFile> app;
      nsAutoString appInfo(pResult);
      CoTaskMemFree(pResult);
      if (NS_SUCCEEDED(GetDefaultAppInfo(appInfo, _retval, getter_AddRefs(app))))
        return NS_OK;
    }
    return NS_ERROR_NOT_AVAILABLE;
  }

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
/* static */ bool
nsOSHelperAppService::typeFromExtEquals(const PRUnichar* aExt, const char *aType)
{
  if (!aType)
    return false;
  nsAutoString fileExtToUse;
  if (aExt[0] != PRUnichar('.'))
    fileExtToUse = PRUnichar('.');

  fileExtToUse.Append(aExt);

  bool eq = false;
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

// Strip a handler command string of its quotes and parameters.
static void CleanupHandlerPath(nsString& aPath)
{
  // Example command strings passed into this routine:

  // 1) C:\Program Files\Company\some.exe -foo -bar
  // 2) C:\Program Files\Company\some.dll
  // 3) C:\Windows\some.dll,-foo -bar
  // 4) C:\Windows\some.cpl,-foo -bar

  int32_t lastCommaPos = aPath.RFindChar(',');
  if (lastCommaPos != kNotFound)
    aPath.Truncate(lastCommaPos);

  aPath.AppendLiteral(" ");

  // case insensitive
  uint32_t index = aPath.Find(".exe ", true);
  if (index == kNotFound)
    index = aPath.Find(".dll ", true);
  if (index == kNotFound)
    index = aPath.Find(".cpl ", true);

  if (index != kNotFound)
    aPath.Truncate(index + 4);
  aPath.Trim(" ", true, true);
}

// Strip the windows host process bootstrap executable rundll32.exe
// from a handler's command string if it exists.
static void StripRundll32(nsString& aCommandString)
{
  // Example rundll formats:
  // C:\Windows\System32\rundll32.exe "path to dll"
  // rundll32.exe "path to dll"
  // C:\Windows\System32\rundll32.exe "path to dll", var var
  // rundll32.exe "path to dll", var var

  NS_NAMED_LITERAL_STRING(rundllSegment, "rundll32.exe ");
  NS_NAMED_LITERAL_STRING(rundllSegmentShort, "rundll32 ");

  // case insensitive
  int32_t strLen = rundllSegment.Length();
  int32_t index = aCommandString.Find(rundllSegment, true);
  if (index == kNotFound) {
    strLen = rundllSegmentShort.Length();
    index = aCommandString.Find(rundllSegmentShort, true);
  }

  if (index != kNotFound) {
    uint32_t rundllSegmentLength = index + strLen;
    aCommandString.Cut(0, rundllSegmentLength);
  }
}

// Returns the fully qualified path to an application handler based on
// a parameterized command string. Note this routine should not be used
// to launch the associated application as it strips parameters and
// rundll.exe from the string. Designed for retrieving display information
// on a particular handler.   
/* static */ bool nsOSHelperAppService::CleanupCmdHandlerPath(nsAString& aCommandHandler)
{
  nsAutoString handlerCommand(aCommandHandler);

  // Straight command path:
  //
  // %SystemRoot%\system32\NOTEPAD.EXE var
  // "C:\Program Files\iTunes\iTunes.exe" var var
  // C:\Program Files\iTunes\iTunes.exe var var
  //
  // Example rundll handlers:
  //
  // rundll32.exe "%ProgramFiles%\Win...ery\PhotoViewer.dll", var var
  // rundll32.exe "%ProgramFiles%\Windows Photo Gallery\PhotoViewer.dll"
  // C:\Windows\System32\rundll32.exe "path to dll", var var
  // %SystemRoot%\System32\rundll32.exe "%ProgramFiles%\Win...ery\Photo
  //    Viewer.dll", var var

  // Expand environment variables so we have full path strings.
  uint32_t bufLength = ::ExpandEnvironmentStringsW(handlerCommand.get(),
                                                   L"", 0);
  if (bufLength == 0) // Error
    return false;

  nsAutoArrayPtr<wchar_t> destination(new wchar_t[bufLength]);
  if (!destination)
    return false;
  if (!::ExpandEnvironmentStringsW(handlerCommand.get(), destination,
                                   bufLength))
    return false;

  handlerCommand = static_cast<const wchar_t*>(destination);

  // Remove quotes around paths
  handlerCommand.StripChars("\"");

  // Strip windows host process bootstrap so we can get to the actual
  // handler.
  StripRundll32(handlerCommand);

  // Trim any command parameters so that we have a native path we can
  // initialize a local file with.
  CleanupHandlerPath(handlerCommand);

  aCommandHandler.Assign(handlerCommand);
  return true;
}

// The "real" name of a given helper app (as specified by the path to the 
// executable file held in various registry keys) is stored n the VERSIONINFO
// block in the file's resources. We need to find the path to the executable
// and then retrieve the "FileDescription" field value from the file. 
nsresult
nsOSHelperAppService::GetDefaultAppInfo(const nsAString& aAppInfo,
                                        nsAString& aDefaultDescription, 
                                        nsIFile** aDefaultApplication)
{
  nsAutoString handlerCommand;

  // If all else fails, use the file type key name, which will be 
  // something like "pngfile" for .pngs, "WMVFile" for .wmvs, etc. 
  aDefaultDescription = aAppInfo;
  *aDefaultApplication = nullptr;

  if (aAppInfo.IsEmpty())
    return NS_ERROR_FAILURE;

  // aAppInfo may be a file, file path, program id, or
  // Applications reference -
  // c:\dir\app.exe
  // Applications\appfile.exe/dll (shell\open...)
  // ProgID.progid (shell\open...)

  nsAutoString handlerKeyName(aAppInfo);

  nsCOMPtr<nsIWindowsRegKey> chkKey = 
    do_CreateInstance("@mozilla.org/windows-registry-key;1");
  if (!chkKey) 
    return NS_ERROR_FAILURE;
      
  nsresult rv = chkKey->Open(nsIWindowsRegKey::ROOT_KEY_CLASSES_ROOT,
                             handlerKeyName, 
                             nsIWindowsRegKey::ACCESS_QUERY_VALUE);
  if (NS_FAILED(rv)) {
    // It's a file system path to a handler 
    handlerCommand.Assign(aAppInfo);
  }
  else {
    handlerKeyName.AppendLiteral("\\shell\\open\\command");
    nsCOMPtr<nsIWindowsRegKey> regKey = 
      do_CreateInstance("@mozilla.org/windows-registry-key;1");
    if (!regKey) 
      return NS_ERROR_FAILURE;

    nsresult rv = regKey->Open(nsIWindowsRegKey::ROOT_KEY_CLASSES_ROOT,
                               handlerKeyName, 
                               nsIWindowsRegKey::ACCESS_QUERY_VALUE);
    if (NS_FAILED(rv))
      return NS_ERROR_FAILURE;
     
    // OK, the default value here is the description of the type.
    rv = regKey->ReadStringValue(EmptyString(), handlerCommand);
    if (NS_FAILED(rv)) {

      // Check if there is a DelegateExecute string
      nsAutoString delegateExecute;
      rv = regKey->ReadStringValue(NS_LITERAL_STRING("DelegateExecute"), delegateExecute);
      NS_ENSURE_SUCCESS(rv, rv);

      // Look for InProcServer32
      nsAutoString delegateExecuteRegPath;
      delegateExecuteRegPath.AssignLiteral("CLSID\\");
      delegateExecuteRegPath.Append(delegateExecute);
      delegateExecuteRegPath.AppendLiteral("\\InProcServer32");
      rv = chkKey->Open(nsIWindowsRegKey::ROOT_KEY_CLASSES_ROOT,
                        delegateExecuteRegPath, 
                        nsIWindowsRegKey::ACCESS_QUERY_VALUE);
      if (NS_SUCCEEDED(rv)) {
        rv = chkKey->ReadStringValue(EmptyString(), handlerCommand);
      }

      if (NS_FAILED(rv)) {
        // Look for LocalServer32
        delegateExecuteRegPath.AssignLiteral("CLSID\\");
        delegateExecuteRegPath.Append(delegateExecute);
        delegateExecuteRegPath.AppendLiteral("\\LocalServer32");
        rv = chkKey->Open(nsIWindowsRegKey::ROOT_KEY_CLASSES_ROOT,
                          delegateExecuteRegPath, 
                          nsIWindowsRegKey::ACCESS_QUERY_VALUE);
        NS_ENSURE_SUCCESS(rv, rv);
        rv = chkKey->ReadStringValue(EmptyString(), handlerCommand);
        NS_ENSURE_SUCCESS(rv, rv);
      }
    }
  }

  if (!CleanupCmdHandlerPath(handlerCommand))
    return NS_ERROR_FAILURE;

  // XXX FIXME: If this fails, the UI will display the full command
  // string.
  // There are some rare cases this can happen - ["url.dll" -foo]
  // for example won't resolve correctly to the system dir. The 
  // subsequent launch of the helper app will work though.
  nsCOMPtr<nsIFile> lf;
  NS_NewLocalFile(handlerCommand, true, getter_AddRefs(lf));
  if (!lf)
    return NS_ERROR_FILE_NOT_FOUND;

  nsILocalFileWin* lfw = nullptr;
  CallQueryInterface(lf, &lfw);

  if (lfw) {
    // The "FileDescription" field contains the actual name of the application.
    lfw->GetVersionInfoField("FileDescription", aDefaultDescription);
    // QI addref'ed for us.
    *aDefaultApplication = lfw;
  }

  return NS_OK;
}

already_AddRefed<nsMIMEInfoWin> nsOSHelperAppService::GetByExtension(const nsAFlatString& aFileExt, const char *aTypeHint)
{
  if (aFileExt.IsEmpty())
    return nullptr;

  // windows registry assumes your file extension is going to include the '.'.
  // so make sure it's there...
  nsAutoString fileExtToUse;
  if (aFileExt.First() != PRUnichar('.'))
    fileExtToUse = PRUnichar('.');

  fileExtToUse.Append(aFileExt);

  // Try to get an entry from the windows registry.
  nsCOMPtr<nsIWindowsRegKey> regKey = 
    do_CreateInstance("@mozilla.org/windows-registry-key;1");
  if (!regKey) 
    return nullptr; 

  nsresult rv = regKey->Open(nsIWindowsRegKey::ROOT_KEY_CLASSES_ROOT,
                             fileExtToUse,
                             nsIWindowsRegKey::ACCESS_QUERY_VALUE);
  if (NS_FAILED(rv))
    return nullptr; 

  nsAutoCString typeToUse;
  if (aTypeHint && *aTypeHint) {
    typeToUse.Assign(aTypeHint);
  }
  else {
    nsAutoString temp;
    if (NS_FAILED(regKey->ReadStringValue(NS_LITERAL_STRING("Content Type"),
                  temp)) || temp.IsEmpty()) {
      return nullptr; 
    }
    // Content-Type is always in ASCII
    LossyAppendUTF16toASCII(temp, typeToUse);
  }

  nsRefPtr<nsMIMEInfoWin> mimeInfo = new nsMIMEInfoWin(typeToUse);

  // don't append the '.'
  mimeInfo->AppendExtension(NS_ConvertUTF16toUTF8(Substring(fileExtToUse, 1)));
  mimeInfo->SetPreferredAction(nsIMIMEInfo::useSystemDefault);

  nsAutoString appInfo;
  bool found;

  // Retrieve the default application for this extension
  if (mAppAssoc) {
    // Vista: use the new application association COM interfaces
    // for resolving helpers.
    nsString assocType(fileExtToUse);
    wchar_t * pResult = nullptr;
    HRESULT hr = mAppAssoc->QueryCurrentDefault(assocType.get(),
                                                AT_FILEEXTENSION, AL_EFFECTIVE,
                                                &pResult);
    if (SUCCEEDED(hr)) {
      found = true;
      appInfo.Assign(pResult);
      CoTaskMemFree(pResult);
    } 
    else {
      found = false;
    }
  } 
  else
  {
    found = NS_SUCCEEDED(regKey->ReadStringValue(EmptyString(), 
                                                 appInfo));
  }

  // Bug 358297 - ignore the default handler, force the user to choose app
  if (appInfo.EqualsLiteral("XPSViewer.Document"))
    found = false;

  if (!found) {
    return nullptr;
  }

  // Get other nsIMIMEInfo fields from registry, if possible.
  nsAutoString defaultDescription;
  nsCOMPtr<nsIFile> defaultApplication;
  
  if (NS_FAILED(GetDefaultAppInfo(appInfo, defaultDescription,
                                  getter_AddRefs(defaultApplication)))) {
    return nullptr;
  }

  mimeInfo->SetDefaultDescription(defaultDescription);
  mimeInfo->SetDefaultApplicationHandler(defaultApplication);

  // Grab the general description
  GetMIMEInfoFromRegistry(appInfo, mimeInfo);

  return mimeInfo.forget();
}

already_AddRefed<nsIMIMEInfo> nsOSHelperAppService::GetMIMEInfoFromOS(const nsACString& aMIMEType, const nsACString& aFileExt, bool *aFound)
{
  *aFound = true;

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
  nsRefPtr<nsMIMEInfoWin> mi;
  if (!fileExtension.IsEmpty())
    mi = GetByExtension(fileExtension, flatType.get());
  LOG(("Extension lookup on '%s' found: 0x%p\n", fileExtension.get(), mi.get()));

  bool hasDefault = false;
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
      bool extExist = false;
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
      return mi.forget();
    if (miByExt && !mi) {
      return miByExt.forget();
    }
    if (!miByExt && !mi) {
      *aFound = false;
      mi = new nsMIMEInfoWin(flatType);
      if (!aFileExt.IsEmpty()) {
        mi->AppendExtension(aFileExt);
      }
      
      return mi.forget();
    }

    // if we get here, mi has no default app. copy from extension lookup.
    nsCOMPtr<nsIFile> defaultApp;
    nsAutoString desc;
    miByExt->GetDefaultDescription(desc);

    mi->SetDefaultDescription(desc);
  }
  return mi.forget();
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

  nsMIMEInfoWin *handlerInfo =
    new nsMIMEInfoWin(aScheme, nsMIMEInfoBase::eProtocolInfo);
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


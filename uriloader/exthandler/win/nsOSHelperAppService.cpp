/* -*- Mode: C++; tab-width: 3; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim:set ts=2 sts=2 sw=2 et cin:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsComponentManagerUtils.h"
#include "nsOSHelperAppService.h"
#include "nsISupports.h"
#include "nsString.h"
#include "nsIMIMEInfo.h"
#include "nsMIMEInfoWin.h"
#include "nsMimeTypes.h"
#include "plstr.h"
#include "nsNativeCharsetUtils.h"
#include "nsLocalFile.h"
#include "nsIWindowsRegKey.h"
#include "nsXULAppAPI.h"
#include "mozilla/UniquePtrExtensions.h"
#include "mozilla/WindowsVersion.h"

// shellapi.h is needed to build with WIN32_LEAN_AND_MEAN
#include <shellapi.h>
#include <shlwapi.h>

#define LOG(args) MOZ_LOG(mLog, mozilla::LogLevel::Debug, args)

// helper methods: forward declarations...
static nsresult GetExtensionFromWindowsMimeDatabase(const nsACString& aMimeType,
                                                    nsString& aFileExtension);

nsOSHelperAppService::nsOSHelperAppService()
    : nsExternalHelperAppService(), mAppAssoc(nullptr) {
  CoInitialize(nullptr);
  CoCreateInstance(CLSID_ApplicationAssociationRegistration, nullptr,
                   CLSCTX_INPROC, IID_IApplicationAssociationRegistration,
                   (void**)&mAppAssoc);
}

nsOSHelperAppService::~nsOSHelperAppService() {
  if (mAppAssoc) mAppAssoc->Release();
  mAppAssoc = nullptr;
  CoUninitialize();
}

// The windows registry provides a mime database key which lists a set of mime
// types and corresponding "Extension" values. we can use this to look up our
// mime type to see if there is a preferred extension for the mime type.
static nsresult GetExtensionFromWindowsMimeDatabase(const nsACString& aMimeType,
                                                    nsString& aFileExtension) {
  nsAutoString mimeDatabaseKey;
  mimeDatabaseKey.AssignLiteral("MIME\\Database\\Content Type\\");

  AppendASCIItoUTF16(aMimeType, mimeDatabaseKey);

  nsCOMPtr<nsIWindowsRegKey> regKey =
      do_CreateInstance("@mozilla.org/windows-registry-key;1");
  if (!regKey) return NS_ERROR_NOT_AVAILABLE;

  nsresult rv =
      regKey->Open(nsIWindowsRegKey::ROOT_KEY_CLASSES_ROOT, mimeDatabaseKey,
                   nsIWindowsRegKey::ACCESS_QUERY_VALUE);

  if (NS_SUCCEEDED(rv))
    regKey->ReadStringValue(NS_LITERAL_STRING("Extension"), aFileExtension);

  return NS_OK;
}

nsresult nsOSHelperAppService::OSProtocolHandlerExists(
    const char* aProtocolScheme, bool* aHandlerExists) {
  // look up the protocol scheme in the windows registry....if we find a match
  // then we have a handler for it...
  *aHandlerExists = false;
  if (aProtocolScheme && *aProtocolScheme) {
    NS_ENSURE_TRUE(mAppAssoc, NS_ERROR_NOT_AVAILABLE);
    wchar_t* pResult = nullptr;
    NS_ConvertASCIItoUTF16 scheme(aProtocolScheme);
    // We are responsible for freeing returned strings.
    HRESULT hr = mAppAssoc->QueryCurrentDefault(scheme.get(), AT_URLPROTOCOL,
                                                AL_EFFECTIVE, &pResult);
    if (SUCCEEDED(hr)) {
      CoTaskMemFree(pResult);
      nsCOMPtr<nsIWindowsRegKey> regKey =
          do_CreateInstance("@mozilla.org/windows-registry-key;1");
      if (!regKey) {
        return NS_ERROR_NOT_AVAILABLE;
      }

      nsresult rv = regKey->Open(nsIWindowsRegKey::ROOT_KEY_CLASSES_ROOT,
                                 nsDependentString(scheme.get()),
                                 nsIWindowsRegKey::ACCESS_QUERY_VALUE);
      if (NS_FAILED(rv)) {
        // Open will fail if the registry key path doesn't exist.
        return NS_OK;
      }

      bool hasValue;
      rv = regKey->HasValue(NS_LITERAL_STRING("URL Protocol"), &hasValue);
      if (NS_FAILED(rv)) {
        return NS_ERROR_FAILURE;
      }
      if (!hasValue) {
        return NS_OK;
      }

      *aHandlerExists = true;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP nsOSHelperAppService::GetApplicationDescription(
    const nsACString& aScheme, nsAString& _retval) {
  nsCOMPtr<nsIWindowsRegKey> regKey =
      do_CreateInstance("@mozilla.org/windows-registry-key;1");
  if (!regKey) return NS_ERROR_NOT_AVAILABLE;

  NS_ConvertASCIItoUTF16 buf(aScheme);

  if (mozilla::IsWin8OrLater()) {
    wchar_t result[1024];
    DWORD resultSize = 1024;
    HRESULT hr = AssocQueryString(0x1000 /* ASSOCF_IS_PROTOCOL */,
                                  ASSOCSTR_FRIENDLYAPPNAME, buf.get(), NULL,
                                  result, &resultSize);
    if (SUCCEEDED(hr)) {
      _retval = result;
      return NS_OK;
    }
  }

  NS_ENSURE_TRUE(mAppAssoc, NS_ERROR_NOT_AVAILABLE);
  wchar_t* pResult = nullptr;
  // We are responsible for freeing returned strings.
  HRESULT hr = mAppAssoc->QueryCurrentDefault(buf.get(), AT_URLPROTOCOL,
                                              AL_EFFECTIVE, &pResult);
  if (SUCCEEDED(hr)) {
    nsCOMPtr<nsIFile> app;
    nsAutoString appInfo(pResult);
    CoTaskMemFree(pResult);
    if (NS_SUCCEEDED(GetDefaultAppInfo(appInfo, _retval, getter_AddRefs(app))))
      return NS_OK;
  }
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP nsOSHelperAppService::IsCurrentAppOSDefaultForProtocol(
    const nsACString& aScheme, bool* _retval) {
  *_retval = false;

  NS_ENSURE_TRUE(mAppAssoc, NS_ERROR_NOT_AVAILABLE);

  NS_ConvertASCIItoUTF16 buf(aScheme);

  // Find the progID
  wchar_t* pResult = nullptr;
  HRESULT hr = mAppAssoc->QueryCurrentDefault(buf.get(), AT_URLPROTOCOL,
                                              AL_EFFECTIVE, &pResult);
  if (FAILED(hr)) {
    return NS_ERROR_FAILURE;
  }
  nsAutoString progID(pResult);
  // We are responsible for freeing returned strings.
  CoTaskMemFree(pResult);

  // Find the default executable.
  nsAutoString description;
  nsCOMPtr<nsIFile> appFile;
  nsresult rv = GetDefaultAppInfo(progID, description, getter_AddRefs(appFile));
  if (NS_FAILED(rv)) {
    return rv;
  }
  // Determine if the default executable is our executable.
  nsCOMPtr<nsIFile> ourBinary;
  XRE_GetBinaryPath(getter_AddRefs(ourBinary));
  bool isSame = false;
  rv = appFile->Equals(ourBinary, &isSame);
  if (NS_FAILED(rv)) {
    return rv;
  }
  *_retval = isSame;
  return NS_OK;
}

// GetMIMEInfoFromRegistry: This function obtains the values of some of the
// nsIMIMEInfo attributes for the mimeType/extension associated with the input
// registry key.  The default entry for that key is the name of a registry key
// under HKEY_CLASSES_ROOT.  The default value for *that* key is the descriptive
// name of the type.  The EditFlags value is a binary value; the low order bit
// of the third byte of which indicates that the user does not need to be
// prompted.
//
// This function sets only the Description attribute of the input nsIMIMEInfo.
/* static */
nsresult nsOSHelperAppService::GetMIMEInfoFromRegistry(const nsString& fileType,
                                                       nsIMIMEInfo* pInfo) {
  nsresult rv = NS_OK;

  NS_ENSURE_ARG(pInfo);
  nsCOMPtr<nsIWindowsRegKey> regKey =
      do_CreateInstance("@mozilla.org/windows-registry-key;1");
  if (!regKey) return NS_ERROR_NOT_AVAILABLE;

  rv = regKey->Open(nsIWindowsRegKey::ROOT_KEY_CLASSES_ROOT, fileType,
                    nsIWindowsRegKey::ACCESS_QUERY_VALUE);
  if (NS_FAILED(rv)) return NS_ERROR_FAILURE;

  // OK, the default value here is the description of the type.
  nsAutoString description;
  rv = regKey->ReadStringValue(EmptyString(), description);
  if (NS_SUCCEEDED(rv)) pInfo->SetDescription(description);

  return NS_OK;
}

/////////////////////////////////////////////////////////////////////////////////////////////////
// method overrides used to gather information from the windows registry for
// various mime types.
////////////////////////////////////////////////////////////////////////////////////////////////

/// Looks up the type for the extension aExt and compares it to aType
/* static */
bool nsOSHelperAppService::typeFromExtEquals(const char16_t* aExt,
                                             const char* aType) {
  if (!aType) return false;
  nsAutoString fileExtToUse;
  if (aExt[0] != char16_t('.')) fileExtToUse = char16_t('.');

  fileExtToUse.Append(aExt);

  bool eq = false;
  nsCOMPtr<nsIWindowsRegKey> regKey =
      do_CreateInstance("@mozilla.org/windows-registry-key;1");
  if (!regKey) return eq;

  nsresult rv =
      regKey->Open(nsIWindowsRegKey::ROOT_KEY_CLASSES_ROOT, fileExtToUse,
                   nsIWindowsRegKey::ACCESS_QUERY_VALUE);
  if (NS_FAILED(rv)) return eq;

  nsAutoString type;
  rv = regKey->ReadStringValue(NS_LITERAL_STRING("Content Type"), type);
  if (NS_SUCCEEDED(rv)) eq = type.EqualsASCII(aType);

  return eq;
}

// The "real" name of a given helper app (as specified by the path to the
// executable file held in various registry keys) is stored n the VERSIONINFO
// block in the file's resources. We need to find the path to the executable
// and then retrieve the "FileDescription" field value from the file.
nsresult nsOSHelperAppService::GetDefaultAppInfo(
    const nsAString& aAppInfo, nsAString& aDefaultDescription,
    nsIFile** aDefaultApplication) {
  nsAutoString handlerCommand;

  // If all else fails, use the file type key name, which will be
  // something like "pngfile" for .pngs, "WMVFile" for .wmvs, etc.
  aDefaultDescription = aAppInfo;
  *aDefaultApplication = nullptr;

  if (aAppInfo.IsEmpty()) return NS_ERROR_FAILURE;

  // aAppInfo may be a file, file path, program id, or
  // Applications reference -
  // c:\dir\app.exe
  // Applications\appfile.exe/dll (shell\open...)
  // ProgID.progid (shell\open...)

  nsAutoString handlerKeyName(aAppInfo);

  nsCOMPtr<nsIWindowsRegKey> chkKey =
      do_CreateInstance("@mozilla.org/windows-registry-key;1");
  if (!chkKey) return NS_ERROR_FAILURE;

  nsresult rv =
      chkKey->Open(nsIWindowsRegKey::ROOT_KEY_CLASSES_ROOT, handlerKeyName,
                   nsIWindowsRegKey::ACCESS_QUERY_VALUE);
  if (NS_FAILED(rv)) {
    // It's a file system path to a handler
    handlerCommand.Assign(aAppInfo);
  } else {
    handlerKeyName.AppendLiteral("\\shell\\open\\command");
    nsCOMPtr<nsIWindowsRegKey> regKey =
        do_CreateInstance("@mozilla.org/windows-registry-key;1");
    if (!regKey) return NS_ERROR_FAILURE;

    nsresult rv =
        regKey->Open(nsIWindowsRegKey::ROOT_KEY_CLASSES_ROOT, handlerKeyName,
                     nsIWindowsRegKey::ACCESS_QUERY_VALUE);
    if (NS_FAILED(rv)) return NS_ERROR_FAILURE;

    // OK, the default value here is the description of the type.
    rv = regKey->ReadStringValue(EmptyString(), handlerCommand);
    if (NS_FAILED(rv)) {
      // Check if there is a DelegateExecute string
      nsAutoString delegateExecute;
      rv = regKey->ReadStringValue(NS_LITERAL_STRING("DelegateExecute"),
                                   delegateExecute);
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

  // XXX FIXME: If this fails, the UI will display the full command
  // string.
  // There are some rare cases this can happen - ["url.dll" -foo]
  // for example won't resolve correctly to the system dir. The
  // subsequent launch of the helper app will work though.
  nsCOMPtr<nsILocalFileWin> lf = new nsLocalFile();
  rv = lf->InitWithCommandLine(handlerCommand);
  NS_ENSURE_SUCCESS(rv, rv);
  lf.forget(aDefaultApplication);

  wchar_t friendlyName[1024];
  DWORD friendlyNameSize = 1024;
  HRESULT hr = AssocQueryString(ASSOCF_NONE, ASSOCSTR_FRIENDLYAPPNAME,
                                PromiseFlatString(aAppInfo).get(), NULL,
                                friendlyName, &friendlyNameSize);
  if (SUCCEEDED(hr) && friendlyNameSize > 1) {
    aDefaultDescription.Assign(friendlyName, friendlyNameSize - 1);
  }

  return NS_OK;
}

already_AddRefed<nsMIMEInfoWin> nsOSHelperAppService::GetByExtension(
    const nsString& aFileExt, const char* aTypeHint) {
  if (aFileExt.IsEmpty()) return nullptr;

  // Determine the mime type.
  nsAutoCString typeToUse;
  if (aTypeHint && *aTypeHint) {
    typeToUse.Assign(aTypeHint);
  } else if (!GetMIMETypeFromOSForExtension(NS_ConvertUTF16toUTF8(aFileExt),
                                            typeToUse)) {
    return nullptr;
  }

  RefPtr<nsMIMEInfoWin> mimeInfo = new nsMIMEInfoWin(typeToUse);

  // windows registry assumes your file extension is going to include the '.',
  // but our APIs expect it to not be there, so make sure we normalize that bit.
  nsAutoString fileExtToUse;
  if (aFileExt.First() != char16_t('.')) fileExtToUse = char16_t('.');

  fileExtToUse.Append(aFileExt);

  // don't append the '.' for our APIs.
  mimeInfo->AppendExtension(NS_ConvertUTF16toUTF8(Substring(fileExtToUse, 1)));
  mimeInfo->SetPreferredAction(nsIMIMEInfo::useSystemDefault);

  nsAutoString appInfo;
  bool found;

  // Retrieve the default application for this extension
  NS_ENSURE_TRUE(mAppAssoc, nullptr);
  nsString assocType(fileExtToUse);
  wchar_t* pResult = nullptr;
  HRESULT hr = mAppAssoc->QueryCurrentDefault(assocType.get(), AT_FILEEXTENSION,
                                              AL_EFFECTIVE, &pResult);
  if (SUCCEEDED(hr)) {
    found = true;
    appInfo.Assign(pResult);
    CoTaskMemFree(pResult);
  } else {
    found = false;
  }

  // Bug 358297 - ignore the default handler, force the user to choose app
  if (appInfo.EqualsLiteral("XPSViewer.Document")) found = false;

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

NS_IMETHODIMP
nsOSHelperAppService::GetMIMEInfoFromOS(const nsACString& aMIMEType,
                                        const nsACString& aFileExt,
                                        bool* aFound, nsIMIMEInfo** aMIMEInfo) {
  *aFound = false;

  const nsCString& flatType = PromiseFlatCString(aMIMEType);
  nsAutoString fileExtension;
  CopyUTF8toUTF16(aFileExt, fileExtension);

  /* XXX The octet-stream check is a gross hack to wallpaper over the most
   * common Win32 extension issues caused by the fix for bug 116938.  See bug
   * 120327, comment 271 for why this is needed.  Not even sure we
   * want to remove this once we have fixed all this stuff to work
   * right; any info we get from the OS on this type is pretty much
   * useless....
   */
  bool haveMeaningfulMimeType =
      !aMIMEType.IsEmpty() &&
      !aMIMEType.LowerCaseEqualsLiteral(APPLICATION_OCTET_STREAM);
  LOG(("Extension lookup on '%s' with mimetype '%s'%s\n", fileExtension.get(),
       flatType.get(),
       haveMeaningfulMimeType ? " (treated as meaningful)" : ""));

  RefPtr<nsMIMEInfoWin> mi;

  // We should have *something* to go on here.
  nsAutoString extensionFromMimeType;
  if (haveMeaningfulMimeType) {
    GetExtensionFromWindowsMimeDatabase(aMIMEType, extensionFromMimeType);
  }
  if (fileExtension.IsEmpty() && extensionFromMimeType.IsEmpty()) {
    // Without an extension from the mimetype or the file, we can't
    // do anything here.
    mi = new nsMIMEInfoWin(flatType.get());
    if (!aFileExt.IsEmpty()) {
      mi->AppendExtension(aFileExt);
    }
    mi.forget(aMIMEInfo);
    return NS_OK;
  }

  // Either fileExtension or extensionFromMimeType must now be non-empty.

  *aFound = true;

  // On Windows, we prefer the file extension for lookups over the mimetype,
  // because that's how windows does things.
  // If we have no file extension or it doesn't match the mimetype, use the
  // mime type's default file extension instead.
  bool usedMimeTypeExtensionForLookup = false;
  if (fileExtension.IsEmpty() ||
      (!extensionFromMimeType.IsEmpty() &&
       !typeFromExtEquals(fileExtension.get(), flatType.get()))) {
    usedMimeTypeExtensionForLookup = true;
    fileExtension = extensionFromMimeType;
    LOG(("Now using '%s' mimetype's default file extension '%s' for lookup\n",
         flatType.get(), fileExtension.get()));
  }

  // If we have an extension, use it for lookup:
  mi = GetByExtension(fileExtension, flatType.get());
  LOG(("Extension lookup on '%s' found: 0x%p\n", fileExtension.get(),
       mi.get()));

  if (mi) {
    bool hasDefault = false;
    mi->GetHasDefaultHandler(&hasDefault);
    // If we don't find a default handler description, see if we can find one
    // using the mimetype.
    if (!hasDefault && !usedMimeTypeExtensionForLookup) {
      RefPtr<nsMIMEInfoWin> miFromMimeType =
          GetByExtension(extensionFromMimeType, flatType.get());
      LOG(("Mime-based ext. lookup for '%s' found 0x%p\n",
           extensionFromMimeType.get(), miFromMimeType.get()));
      if (miFromMimeType) {
        nsAutoString desc;
        miFromMimeType->GetDefaultDescription(desc);
        mi->SetDefaultDescription(desc);
      }
    }
    mi.forget(aMIMEInfo);
    return NS_OK;
  }

  // The extension didn't work. Try the extension from the mimetype if
  // different:
  if (!extensionFromMimeType.IsEmpty() && !usedMimeTypeExtensionForLookup) {
    mi = GetByExtension(extensionFromMimeType, flatType.get());
    LOG(("Mime-based ext. lookup for '%s' found 0x%p\n",
         extensionFromMimeType.get(), mi.get()));
  }
  if (mi) {
    mi.forget(aMIMEInfo);
    return NS_OK;
  }
  // This didn't work either, so just return an empty dummy mimeinfo.
  *aFound = false;
  mi = new nsMIMEInfoWin(flatType.get());
  // If we didn't resort to the mime type's extension, we must have had a
  // valid extension, so stick it on the mime info.
  if (!usedMimeTypeExtensionForLookup) {
    mi->AppendExtension(aFileExt);
  }
  mi.forget(aMIMEInfo);
  return NS_OK;
}

NS_IMETHODIMP
nsOSHelperAppService::GetProtocolHandlerInfoFromOS(const nsACString& aScheme,
                                                   bool* found,
                                                   nsIHandlerInfo** _retval) {
  NS_ASSERTION(!aScheme.IsEmpty(), "No scheme was specified!");

  nsresult rv =
      OSProtocolHandlerExists(nsPromiseFlatCString(aScheme).get(), found);
  if (NS_FAILED(rv)) return rv;

  nsMIMEInfoWin* handlerInfo =
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

bool nsOSHelperAppService::GetMIMETypeFromOSForExtension(
    const nsACString& aExtension, nsACString& aMIMEType) {
  if (aExtension.IsEmpty()) return false;

  // windows registry assumes your file extension is going to include the '.'.
  // so make sure it's there...
  nsAutoString fileExtToUse;
  if (aExtension.First() != '.') fileExtToUse = char16_t('.');

  AppendUTF8toUTF16(aExtension, fileExtToUse);

  // Try to get an entry from the windows registry.
  nsCOMPtr<nsIWindowsRegKey> regKey =
      do_CreateInstance("@mozilla.org/windows-registry-key;1");
  if (!regKey) return false;

  nsresult rv =
      regKey->Open(nsIWindowsRegKey::ROOT_KEY_CLASSES_ROOT, fileExtToUse,
                   nsIWindowsRegKey::ACCESS_QUERY_VALUE);
  if (NS_FAILED(rv)) return false;

  nsAutoString mimeType;
  if (NS_FAILED(regKey->ReadStringValue(NS_LITERAL_STRING("Content Type"),
                                        mimeType)) ||
      mimeType.IsEmpty()) {
    return false;
  }
  // Content-Type is always in ASCII
  aMIMEType.Truncate();
  LossyAppendUTF16toASCII(mimeType, aMIMEType);
  return true;
}

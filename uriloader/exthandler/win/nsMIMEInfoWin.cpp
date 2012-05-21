/* -*- Mode: C++; tab-width: 3; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsArrayEnumerator.h"
#include "nsCOMArray.h"
#include "nsILocalFile.h"
#include "nsIVariant.h"
#include "nsMIMEInfoWin.h"
#include "nsNetUtil.h"
#include <windows.h>
#include <shellapi.h>
#include "nsAutoPtr.h"
#include "nsIMutableArray.h"
#include "nsTArray.h"
#include "shlobj.h"
#include "windows.h"
#include "nsIWindowsRegKey.h"
#include "nsIProcess.h"
#include "nsOSHelperAppService.h"
#include "nsUnicharUtils.h"
#include "nsITextToSubURI.h"

#define RUNDLL32_EXE L"\\rundll32.exe"


NS_IMPL_ISUPPORTS_INHERITED1(nsMIMEInfoWin, nsMIMEInfoBase, nsIPropertyBag)

nsMIMEInfoWin::~nsMIMEInfoWin()
{
}

nsresult
nsMIMEInfoWin::LaunchDefaultWithFile(nsIFile* aFile)
{
  // Launch the file, unless it is an executable.
  nsCOMPtr<nsILocalFile> local(do_QueryInterface(aFile));
  if (!local)
    return NS_ERROR_FAILURE;

  bool executable = true;
  local->IsExecutable(&executable);
  if (executable)
    return NS_ERROR_FAILURE;

  return local->Launch();
}

NS_IMETHODIMP
nsMIMEInfoWin::LaunchWithFile(nsIFile* aFile)
{
  nsresult rv;

  // it doesn't make any sense to call this on protocol handlers
  NS_ASSERTION(mClass == eMIMEInfo,
               "nsMIMEInfoBase should have mClass == eMIMEInfo");

  if (mPreferredAction == useSystemDefault) {
    return LaunchDefaultWithFile(aFile);
  }

  if (mPreferredAction == useHelperApp) {
    if (!mPreferredApplication)
      return NS_ERROR_FILE_NOT_FOUND;

    // at the moment, we only know how to hand files off to local handlers
    nsCOMPtr<nsILocalHandlerApp> localHandler = 
      do_QueryInterface(mPreferredApplication, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIFile> executable;
    rv = localHandler->GetExecutable(getter_AddRefs(executable));
    NS_ENSURE_SUCCESS(rv, rv);

    nsAutoString path;
    aFile->GetPath(path);

    // Deal with local dll based handlers
    nsCString filename;
    executable->GetNativeLeafName(filename);
    if (filename.Length() > 4) {
      nsCString extension(Substring(filename, filename.Length() - 4, 4));

      if (extension.LowerCaseEqualsLiteral(".dll")) {
        nsAutoString args;

        // executable is rundll32, everything else is a list of parameters, 
        // including the dll handler.
        nsCOMPtr<nsILocalFile> locFile(do_QueryInterface(aFile));

        if (!GetDllLaunchInfo(executable, locFile, args, false))
          return NS_ERROR_INVALID_ARG;

        WCHAR rundll32Path[MAX_PATH + sizeof(RUNDLL32_EXE) / sizeof(WCHAR) + 1] = {L'\0'};
        if (!GetSystemDirectoryW(rundll32Path, MAX_PATH)) {
          return NS_ERROR_FILE_NOT_FOUND;
        }
        lstrcatW(rundll32Path, RUNDLL32_EXE);

        SHELLEXECUTEINFOW seinfo;
        memset(&seinfo, 0, sizeof(seinfo));
        seinfo.cbSize = sizeof(SHELLEXECUTEINFOW);
        seinfo.fMask  = NULL;
        seinfo.hwnd   = NULL;
        seinfo.lpVerb = NULL;
        seinfo.lpFile = rundll32Path;
        seinfo.lpParameters =  args.get();
        seinfo.lpDirectory  = NULL;
        seinfo.nShow  = SW_SHOWNORMAL;
        if (ShellExecuteExW(&seinfo))
          return NS_OK;

        switch ((LONG_PTR)seinfo.hInstApp) {
          case 0:
          case SE_ERR_OOM:
            return NS_ERROR_OUT_OF_MEMORY;
          case SE_ERR_ACCESSDENIED:
            return NS_ERROR_FILE_ACCESS_DENIED;
          case SE_ERR_ASSOCINCOMPLETE:
          case SE_ERR_NOASSOC:
            return NS_ERROR_UNEXPECTED;
          case SE_ERR_DDEBUSY:
          case SE_ERR_DDEFAIL:
          case SE_ERR_DDETIMEOUT:
            return NS_ERROR_NOT_AVAILABLE;
          case SE_ERR_DLLNOTFOUND:
            return NS_ERROR_FAILURE;
          case SE_ERR_SHARE:
            return NS_ERROR_FILE_IS_LOCKED;
          default:
            switch(GetLastError()) {
              case ERROR_FILE_NOT_FOUND:
                return NS_ERROR_FILE_NOT_FOUND;
              case ERROR_PATH_NOT_FOUND:
                return NS_ERROR_FILE_UNRECOGNIZED_PATH;
              case ERROR_BAD_FORMAT:
                return NS_ERROR_FILE_CORRUPTED;
            }

        }
        return NS_ERROR_FILE_EXECUTION_FAILED;
      }
    }
    return LaunchWithIProcess(executable, path);
  }

  return NS_ERROR_INVALID_ARG;
}

NS_IMETHODIMP
nsMIMEInfoWin::GetHasDefaultHandler(bool * _retval)
{
  // We have a default application if we have a description
  // We can ShellExecute anything; however, callers are probably interested if
  // there is really an application associated with this type of file
  *_retval = !mDefaultAppDescription.IsEmpty();
  return NS_OK;
}

NS_IMETHODIMP
nsMIMEInfoWin::GetEnumerator(nsISimpleEnumerator* *_retval)
{
  nsCOMArray<nsIVariant> properties;

  nsCOMPtr<nsIVariant> variant;
  GetProperty(NS_LITERAL_STRING("defaultApplicationIconURL"), getter_AddRefs(variant));
  if (variant)
    properties.AppendObject(variant);

  GetProperty(NS_LITERAL_STRING("customApplicationIconURL"), getter_AddRefs(variant));
  if (variant)
    properties.AppendObject(variant);

  return NS_NewArrayEnumerator(_retval, properties);
}

static nsresult GetIconURLVariant(nsIFile* aApplication, nsIVariant* *_retval)
{
  nsresult rv = CallCreateInstance("@mozilla.org/variant;1", _retval);
  if (NS_FAILED(rv))
    return rv;
  nsCAutoString fileURLSpec;
  NS_GetURLSpecFromFile(aApplication, fileURLSpec);
  nsCAutoString iconURLSpec; iconURLSpec.AssignLiteral("moz-icon://");
  iconURLSpec += fileURLSpec;
  nsCOMPtr<nsIWritableVariant> writable(do_QueryInterface(*_retval));
  writable->SetAsAUTF8String(iconURLSpec);
  return NS_OK;
}

NS_IMETHODIMP
nsMIMEInfoWin::GetProperty(const nsAString& aName, nsIVariant* *_retval)
{
  nsresult rv;
  if (mDefaultApplication && aName.EqualsLiteral(PROPERTY_DEFAULT_APP_ICON_URL)) {
    rv = GetIconURLVariant(mDefaultApplication, _retval);
    NS_ENSURE_SUCCESS(rv, rv);    
  } else if (mPreferredApplication && 
             aName.EqualsLiteral(PROPERTY_CUSTOM_APP_ICON_URL)) {
    nsCOMPtr<nsILocalHandlerApp> localHandler =
      do_QueryInterface(mPreferredApplication, &rv);
    NS_ENSURE_SUCCESS(rv, rv);
    
    nsCOMPtr<nsIFile> executable;
    rv = localHandler->GetExecutable(getter_AddRefs(executable));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = GetIconURLVariant(executable, _retval);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

// this implementation was pretty much copied verbatime from 
// Tony Robinson's code in nsExternalProtocolWin.cpp
nsresult
nsMIMEInfoWin::LoadUriInternal(nsIURI * aURL)
{
  nsresult rv = NS_OK;

  // 1. Find the default app for this protocol
  // 2. Set up the command line
  // 3. Launch the app.

  // For now, we'll just cheat essentially, check for the command line
  // then just call ShellExecute()!

  if (aURL)
  {
    // extract the url spec from the url
    nsCAutoString urlSpec;
    aURL->GetAsciiSpec(urlSpec);
 
    // Unescape non-ASCII characters in the URL
    nsCAutoString urlCharset;
    nsAutoString utf16Spec;
    rv = aURL->GetOriginCharset(urlCharset);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsITextToSubURI> textToSubURI = do_GetService(NS_ITEXTTOSUBURI_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = textToSubURI->UnEscapeNonAsciiURI(urlCharset, urlSpec, utf16Spec);
    NS_ENSURE_SUCCESS(rv, rv);

    static const PRUnichar cmdVerb[] = L"open";
    SHELLEXECUTEINFOW sinfo;
    memset(&sinfo, 0, sizeof(sinfo));
    sinfo.cbSize   = sizeof(sinfo);
    sinfo.fMask    = SEE_MASK_FLAG_DDEWAIT |
      SEE_MASK_FLAG_NO_UI;
    sinfo.hwnd     = NULL;
    sinfo.lpVerb   = (LPWSTR)&cmdVerb;
    sinfo.nShow    = SW_SHOWNORMAL;
    
    LPITEMIDLIST pidl = NULL;
    SFGAOF sfgao;
    
    // Bug 394974
    if (SUCCEEDED(SHParseDisplayName(utf16Spec.get(),NULL, &pidl, 0, &sfgao))) {
      sinfo.lpIDList = pidl;
      sinfo.fMask |= SEE_MASK_INVOKEIDLIST;
    } else {
      // SHParseDisplayName failed. Bailing out as work around for
      // Microsoft Security Bulletin MS07-061
      rv = NS_ERROR_FAILURE;
    }
    if (NS_SUCCEEDED(rv)) {
      BOOL result = ShellExecuteExW(&sinfo);
      if (!result || ((LONG_PTR)sinfo.hInstApp) < 32)
        rv = NS_ERROR_FAILURE;
    }
    if (pidl)
      CoTaskMemFree(pidl);
  }
  
  return rv;
}

// Given a path to a local file, return its nsILocalHandlerApp instance.
bool nsMIMEInfoWin::GetLocalHandlerApp(const nsAString& aCommandHandler,
                                         nsCOMPtr<nsILocalHandlerApp>& aApp)
{
  nsCOMPtr<nsILocalFile> locfile;
  nsresult rv = 
    NS_NewLocalFile(aCommandHandler, true, getter_AddRefs(locfile));
  if (NS_FAILED(rv))
    return false;

  aApp = do_CreateInstance("@mozilla.org/uriloader/local-handler-app;1");
  if (!aApp) 
    return false;

  aApp->SetExecutable(locfile);
  return true;
}

// Return the cleaned up file path associated with a command verb 
// located in root/Applications.
bool nsMIMEInfoWin::GetAppsVerbCommandHandler(const nsAString& appExeName,
                                                nsAString& applicationPath,
                                                bool edit)
{
  nsCOMPtr<nsIWindowsRegKey> appKey = 
    do_CreateInstance("@mozilla.org/windows-registry-key;1");
  if (!appKey) 
    return false; 

  // HKEY_CLASSES_ROOT\Applications\iexplore.exe
  nsAutoString applicationsPath;
  applicationsPath.AppendLiteral("Applications\\");
  applicationsPath.Append(appExeName);

  nsresult rv = appKey->Open(nsIWindowsRegKey::ROOT_KEY_CLASSES_ROOT,
                             applicationsPath,
                             nsIWindowsRegKey::ACCESS_QUERY_VALUE);
  if (NS_FAILED(rv)) 
    return false;

  // Check for the NoOpenWith flag, if it exists
  PRUint32 value;
  if (NS_SUCCEEDED(appKey->ReadIntValue(
      NS_LITERAL_STRING("NoOpenWith"), &value)) &&
      value == 1)
    return false;

  nsAutoString dummy;
  if (NS_SUCCEEDED(appKey->ReadStringValue(
        NS_LITERAL_STRING("NoOpenWith"), dummy)))
    return false;

  appKey->Close();

  // HKEY_CLASSES_ROOT\Applications\iexplore.exe\shell\open\command
  applicationsPath.AssignLiteral("Applications\\");
  applicationsPath.Append(appExeName);
  if (!edit)
    applicationsPath.AppendLiteral("\\shell\\open\\command");
  else
    applicationsPath.AppendLiteral("\\shell\\edit\\command");


  rv = appKey->Open(nsIWindowsRegKey::ROOT_KEY_CLASSES_ROOT,
                    applicationsPath,
                    nsIWindowsRegKey::ACCESS_QUERY_VALUE);
  if (NS_FAILED(rv)) 
    return false;

  nsAutoString appFilesystemCommand;
  if (NS_SUCCEEDED(appKey->ReadStringValue(EmptyString(), 
                                           appFilesystemCommand))) {
    
    // Expand environment vars, clean up any misc.
    if (!nsOSHelperAppService::CleanupCmdHandlerPath(appFilesystemCommand))
      return false;
    
    applicationPath = appFilesystemCommand;
    return true;
  }
  return false;
}

// Return a fully populated command string based on
// passing information. Used in launchWithFile to trace
// back to the full handler path based on the dll.
// (dll, targetfile, return args, open/edit)
bool nsMIMEInfoWin::GetDllLaunchInfo(nsIFile * aDll,
                                       nsILocalFile * aFile,
                                       nsAString& args,
                                       bool edit)
{
  if (!aDll || !aFile) 
    return false;

  nsCOMPtr<nsILocalFile> localDll(do_QueryInterface(aDll));
  if (!localDll)
    return false;

  nsString appExeName;
  localDll->GetLeafName(appExeName);

  nsCOMPtr<nsIWindowsRegKey> appKey = 
    do_CreateInstance("@mozilla.org/windows-registry-key;1");
  if (!appKey) 
    return false; 

  // HKEY_CLASSES_ROOT\Applications\iexplore.exe
  nsAutoString applicationsPath;
  applicationsPath.AppendLiteral("Applications\\");
  applicationsPath.Append(appExeName);

  nsresult rv = appKey->Open(nsIWindowsRegKey::ROOT_KEY_CLASSES_ROOT,
                             applicationsPath,
                             nsIWindowsRegKey::ACCESS_QUERY_VALUE);
  if (NS_FAILED(rv))
    return false;

  // Check for the NoOpenWith flag, if it exists
  PRUint32 value;
  rv = appKey->ReadIntValue(NS_LITERAL_STRING("NoOpenWith"), &value);
  if (NS_SUCCEEDED(rv) && value == 1)
    return false;

  nsAutoString dummy;
  if (NS_SUCCEEDED(appKey->ReadStringValue(NS_LITERAL_STRING("NoOpenWith"), 
                                           dummy)))
    return false;

  appKey->Close();

  // HKEY_CLASSES_ROOT\Applications\iexplore.exe\shell\open\command
  applicationsPath.AssignLiteral("Applications\\");
  applicationsPath.Append(appExeName);
  if (!edit)
    applicationsPath.AppendLiteral("\\shell\\open\\command");
  else
    applicationsPath.AppendLiteral("\\shell\\edit\\command");

  rv = appKey->Open(nsIWindowsRegKey::ROOT_KEY_CLASSES_ROOT,
                    applicationsPath,
                    nsIWindowsRegKey::ACCESS_QUERY_VALUE);
  if (NS_FAILED(rv))
    return false;

  nsAutoString appFilesystemCommand;
  if (NS_SUCCEEDED(appKey->ReadStringValue(EmptyString(),
                                           appFilesystemCommand))) {
    // Replace embedded environment variables.
    PRUint32 bufLength = 
      ::ExpandEnvironmentStringsW(appFilesystemCommand.get(),
                                  L"", 0);
    if (bufLength == 0) // Error
      return false;

    nsAutoArrayPtr<PRUnichar> destination(new PRUnichar[bufLength]);
    if (!destination)
      return false;
    if (!::ExpandEnvironmentStringsW(appFilesystemCommand.get(),
                                     destination,
                                     bufLength))
      return false;

    appFilesystemCommand = destination;

    // C:\Windows\System32\rundll32.exe "C:\Program Files\Windows 
    // Photo Gallery\PhotoViewer.dll", ImageView_Fullscreen %1
    nsAutoString params;
    NS_NAMED_LITERAL_STRING(rundllSegment, "rundll32.exe ");
    PRInt32 index = appFilesystemCommand.Find(rundllSegment);
    if (index > kNotFound) {
      params.Append(Substring(appFilesystemCommand,
                    index + rundllSegment.Length()));
    } else {
      params.Append(appFilesystemCommand);
    }

    // check to make sure we have a %1 and fill it
    NS_NAMED_LITERAL_STRING(percentOneParam, "%1");
    index = params.Find(percentOneParam);
    if (index == kNotFound) // no parameter
      return false;

    nsString target;
    aFile->GetTarget(target);
    params.Replace(index, 2, target);

    args = params;

    return true;
  }
  return false;
}

// Return the cleaned up file path associated with a progid command 
// verb located in root.
bool nsMIMEInfoWin::GetProgIDVerbCommandHandler(const nsAString& appProgIDName,
                                                  nsAString& applicationPath,
                                                  bool edit)
{
  nsCOMPtr<nsIWindowsRegKey> appKey =
    do_CreateInstance("@mozilla.org/windows-registry-key;1");
  if (!appKey) 
    return false; 

  nsAutoString appProgId(appProgIDName);

  // HKEY_CLASSES_ROOT\Windows.XPSReachViewer\shell\open\command
  if (!edit)
    appProgId.AppendLiteral("\\shell\\open\\command");
  else
    appProgId.AppendLiteral("\\shell\\edit\\command");

  nsresult rv = appKey->Open(nsIWindowsRegKey::ROOT_KEY_CLASSES_ROOT,
                             appProgId,
                             nsIWindowsRegKey::ACCESS_QUERY_VALUE);
  if (NS_FAILED(rv))
    return false;

  nsAutoString appFilesystemCommand;
  if (NS_SUCCEEDED(appKey->ReadStringValue(EmptyString(), appFilesystemCommand))) {
    
    // Expand environment vars, clean up any misc.
    if (!nsOSHelperAppService::CleanupCmdHandlerPath(appFilesystemCommand))
      return false;
    
    applicationPath = appFilesystemCommand;
    return true;
  }
  return false;
}

// Helper routine used in tracking app lists. Converts path
// entries to lower case and stores them in the trackList array.
void nsMIMEInfoWin::ProcessPath(nsCOMPtr<nsIMutableArray>& appList,
                                nsTArray<nsString>& trackList,
                                const nsAString& appFilesystemCommand)
{
  nsAutoString lower(appFilesystemCommand);
  ToLowerCase(lower);

  // Don't include firefox.exe in the list
  WCHAR exe[MAX_PATH+1];
  PRUint32 len = GetModuleFileNameW(NULL, exe, MAX_PATH);
  if (len < MAX_PATH && len != 0) {
    PRUint32 index = lower.Find(exe);
    if (index != -1)
      return;
  }

  nsCOMPtr<nsILocalHandlerApp> aApp;
  if (!GetLocalHandlerApp(appFilesystemCommand, aApp))
    return;

  // Save in our main tracking arrays
  appList->AppendElement(aApp, false);
  trackList.AppendElement(lower);
}

// Helper routine that handles a compare between a path
// and an array of paths.
static bool IsPathInList(nsAString& appPath,
                           nsTArray<nsString>& trackList)
{
  // trackList data is always lowercase, see ProcessPath
  // above.
  nsAutoString tmp(appPath);
  ToLowerCase(tmp);

  for (PRUint32 i = 0; i < trackList.Length(); i++) {
    if (tmp.Equals(trackList[i]))
      return true;
  }
  return false;
}

/** 
 * Returns a list of nsILocalHandlerApp objects containing local 
 * handlers associated with this mimeinfo. Implemented per 
 * platform using information in this object to generate the
 * best list. Typically used for an "open with" style user 
 * option.
 * 
 * @return nsIArray of nsILocalHandlerApp
 */
NS_IMETHODIMP
nsMIMEInfoWin::GetPossibleLocalHandlers(nsIArray **_retval)
{
  nsresult rv;

  *_retval = nsnull;

  nsCOMPtr<nsIMutableArray> appList =
    do_CreateInstance("@mozilla.org/array;1");

  if (!appList)
    return NS_ERROR_FAILURE;

  nsTArray<nsString> trackList;

  nsCAutoString fileExt;
  GetPrimaryExtension(fileExt);

  nsCOMPtr<nsIWindowsRegKey> regKey =
    do_CreateInstance("@mozilla.org/windows-registry-key;1");
  if (!regKey) 
    return NS_ERROR_FAILURE; 
  nsCOMPtr<nsIWindowsRegKey> appKey =
    do_CreateInstance("@mozilla.org/windows-registry-key;1");
  if (!appKey) 
    return NS_ERROR_FAILURE; 

  nsAutoString workingRegistryPath;

  bool extKnown = false;
  if (fileExt.IsEmpty()) {
    extKnown = true;
    // Mime type discovery is possible in some cases, through 
    // HKEY_CLASSES_ROOT\MIME\Database\Content Type, however, a number
    // of file extensions related to mime type are simply not defined,
    // (application/rss+xml & application/atom+xml are good examples)
    // in which case we can only provide a generic list.
    nsCAutoString mimeType;
    GetMIMEType(mimeType);
    if (!mimeType.IsEmpty()) {
      workingRegistryPath.AppendLiteral("MIME\\Database\\Content Type\\");
      workingRegistryPath.Append(NS_ConvertASCIItoUTF16(mimeType));
            
      rv = regKey->Open(nsIWindowsRegKey::ROOT_KEY_CLASSES_ROOT,
                        workingRegistryPath,
                        nsIWindowsRegKey::ACCESS_QUERY_VALUE);
      if(NS_SUCCEEDED(rv)) {
        nsAutoString mimeFileExt;
        if (NS_SUCCEEDED(regKey->ReadStringValue(EmptyString(), mimeFileExt))) {
          CopyUTF16toUTF8(mimeFileExt, fileExt);
          extKnown = false;
        }
      }
    }
  }

  nsAutoString fileExtToUse;
  if (fileExt.First() != '.')
    fileExtToUse = PRUnichar('.');
  fileExtToUse.Append(NS_ConvertUTF8toUTF16(fileExt));

  // Note, the order in which these occur has an effect on the 
  // validity of the resulting display list.

  if (!extKnown) {
    // 1) Get the default handler if it exists
    workingRegistryPath = fileExtToUse;

    rv = regKey->Open(nsIWindowsRegKey::ROOT_KEY_CLASSES_ROOT,
                      workingRegistryPath,
                      nsIWindowsRegKey::ACCESS_QUERY_VALUE);
    if (NS_SUCCEEDED(rv)) {
      nsAutoString appProgId;
      if (NS_SUCCEEDED(regKey->ReadStringValue(EmptyString(), appProgId))) {
        // Bug 358297 - ignore the embedded internet explorer handler
        if (appProgId != NS_LITERAL_STRING("XPSViewer.Document")) {
          nsAutoString appFilesystemCommand;
          if (GetProgIDVerbCommandHandler(appProgId,
                                          appFilesystemCommand,
                                          false) &&
              !IsPathInList(appFilesystemCommand, trackList)) {
            ProcessPath(appList, trackList, appFilesystemCommand);
          }
        }
      }
      regKey->Close();
    }


    // 2) list HKEY_CLASSES_ROOT\.ext\OpenWithList\ 
    
    workingRegistryPath = fileExtToUse;
    workingRegistryPath.AppendLiteral("\\OpenWithList");

    rv = regKey->Open(nsIWindowsRegKey::ROOT_KEY_CLASSES_ROOT,
                      workingRegistryPath,
                      nsIWindowsRegKey::ACCESS_QUERY_VALUE);
    if (NS_SUCCEEDED(rv)) {
      PRUint32 count = 0;
      if (NS_SUCCEEDED(regKey->GetValueCount(&count)) && count > 0) {
        for (PRUint32 index = 0; index < count; index++) {
          nsAutoString appName;
          if (NS_FAILED(regKey->GetValueName(index, appName)))
            continue;

          // HKEY_CLASSES_ROOT\Applications\firefox.exe = "path params"
          nsAutoString appFilesystemCommand;
          if (!GetAppsVerbCommandHandler(appName,
                                         appFilesystemCommand,
                                         false) ||
              IsPathInList(appFilesystemCommand, trackList))
            continue;
          ProcessPath(appList, trackList, appFilesystemCommand);
        }
      }
      regKey->Close();
    }


    // 3) List HKEY_CLASSES_ROOT\.ext\OpenWithProgids, with the
    // different step of resolving the progids for the command handler.

    workingRegistryPath = fileExtToUse;
    workingRegistryPath.AppendLiteral("\\OpenWithProgids");

    rv = regKey->Open(nsIWindowsRegKey::ROOT_KEY_CLASSES_ROOT,
                      workingRegistryPath,
                      nsIWindowsRegKey::ACCESS_QUERY_VALUE);
    if (NS_SUCCEEDED(rv)) {
      PRUint32 count = 0;
      if (NS_SUCCEEDED(regKey->GetValueCount(&count)) && count > 0) {
        for (PRUint32 index = 0; index < count; index++) {
          // HKEY_CLASSES_ROOT\.ext\OpenWithProgids\Windows.XPSReachViewer
          nsAutoString appProgId;
          if (NS_FAILED(regKey->GetValueName(index, appProgId)))
            continue;

          nsAutoString appFilesystemCommand;
          if (!GetProgIDVerbCommandHandler(appProgId,
                                           appFilesystemCommand,
                                           false) ||
              IsPathInList(appFilesystemCommand, trackList))
            continue;
          ProcessPath(appList, trackList, appFilesystemCommand);
        }
      }
      regKey->Close();
    }


    // 4) Add any non configured applications located in the MRU list

    // HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\
    // Explorer\FileExts\.ext\OpenWithList
    workingRegistryPath =
      NS_LITERAL_STRING("Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts\\");
    workingRegistryPath += fileExtToUse;
    workingRegistryPath.AppendLiteral("\\OpenWithList");

    rv = regKey->Open(nsIWindowsRegKey::ROOT_KEY_CURRENT_USER,
                      workingRegistryPath,
                      nsIWindowsRegKey::ACCESS_QUERY_VALUE);
    if (NS_SUCCEEDED(rv)) {
      PRUint32 count = 0;
      if (NS_SUCCEEDED(regKey->GetValueCount(&count)) && count > 0) {
        for (PRUint32 index = 0; index < count; index++) {
          nsAutoString appName, appValue;
          if (NS_FAILED(regKey->GetValueName(index, appName)))
            continue;
          if (appName.EqualsLiteral("MRUList"))
            continue;
          if (NS_FAILED(regKey->ReadStringValue(appName, appValue)))
            continue;
          
          // HKEY_CLASSES_ROOT\Applications\firefox.exe = "path params"
          nsAutoString appFilesystemCommand;
          if (!GetAppsVerbCommandHandler(appValue,
                                         appFilesystemCommand,
                                         false) ||
              IsPathInList(appFilesystemCommand, trackList))
            continue;
          ProcessPath(appList, trackList, appFilesystemCommand);
        }
      }
    }
    

    // 5) Add any non configured progids in the MRU list, with the
    // different step of resolving the progids for the command handler.
    
    // HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\
    // Explorer\FileExts\.ext\OpenWithProgids
    workingRegistryPath =
      NS_LITERAL_STRING("Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts\\");
    workingRegistryPath += fileExtToUse;
    workingRegistryPath.AppendLiteral("\\OpenWithProgids");

    regKey->Open(nsIWindowsRegKey::ROOT_KEY_CURRENT_USER,
                 workingRegistryPath,
                 nsIWindowsRegKey::ACCESS_QUERY_VALUE);
    if (NS_SUCCEEDED(rv)) {
      PRUint32 count = 0;
      if (NS_SUCCEEDED(regKey->GetValueCount(&count)) && count > 0) {
        for (PRUint32 index = 0; index < count; index++) {
          nsAutoString appIndex, appProgId;
          if (NS_FAILED(regKey->GetValueName(index, appProgId)))
            continue;

          nsAutoString appFilesystemCommand;
          if (!GetProgIDVerbCommandHandler(appProgId,
                                           appFilesystemCommand,
                                           false) ||
              IsPathInList(appFilesystemCommand, trackList))
            continue;
          ProcessPath(appList, trackList, appFilesystemCommand);
        }
      }
      regKey->Close();
    }


    // 6) Check the perceived type value, and use this to lookup the perceivedtype
    // open with list.
    // http://msdn2.microsoft.com/en-us/library/aa969373.aspx

    workingRegistryPath = fileExtToUse;

    regKey->Open(nsIWindowsRegKey::ROOT_KEY_CLASSES_ROOT,
                 workingRegistryPath,
                 nsIWindowsRegKey::ACCESS_QUERY_VALUE);
    if (NS_SUCCEEDED(rv)) {
      nsAutoString perceivedType;
      rv = regKey->ReadStringValue(NS_LITERAL_STRING("PerceivedType"),
                                   perceivedType);
      if (NS_SUCCEEDED(rv)) {
        nsAutoString openWithListPath(NS_LITERAL_STRING("SystemFileAssociations\\"));
        openWithListPath.Append(perceivedType); // no period
        openWithListPath.Append(NS_LITERAL_STRING("\\OpenWithList"));

        nsresult rv = appKey->Open(nsIWindowsRegKey::ROOT_KEY_CLASSES_ROOT,
                                   openWithListPath,
                                   nsIWindowsRegKey::ACCESS_QUERY_VALUE);
        if (NS_SUCCEEDED(rv)) {
          PRUint32 count = 0;
          if (NS_SUCCEEDED(regKey->GetValueCount(&count)) && count > 0) {
            for (PRUint32 index = 0; index < count; index++) {
              nsAutoString appName;
              if (NS_FAILED(regKey->GetValueName(index, appName)))
                continue;
              
              // HKEY_CLASSES_ROOT\Applications\firefox.exe = "path params"
              nsAutoString appFilesystemCommand;
              if (!GetAppsVerbCommandHandler(appName, appFilesystemCommand, 
                                             false) ||
                  IsPathInList(appFilesystemCommand, trackList))
                continue;
              ProcessPath(appList, trackList, appFilesystemCommand);
            }
          }
        }
      }
    }
  } // extKnown == false


  // 7) list global HKEY_CLASSES_ROOT\*\OpenWithList\
  // Listing general purpose handlers, not specific to a mime type or file extension

  workingRegistryPath = NS_LITERAL_STRING("*\\OpenWithList");

  rv = regKey->Open(nsIWindowsRegKey::ROOT_KEY_CLASSES_ROOT,
                    workingRegistryPath,
                    nsIWindowsRegKey::ACCESS_QUERY_VALUE);
  if (NS_SUCCEEDED(rv)) {
    PRUint32 count = 0;
    if (NS_SUCCEEDED(regKey->GetValueCount(&count)) && count > 0) {
      for (PRUint32 index = 0; index < count; index++) {
        nsAutoString appName;
        if (NS_FAILED(regKey->GetValueName(index, appName)))
          continue;

        // HKEY_CLASSES_ROOT\Applications\firefox.exe = "path params"
        nsAutoString appFilesystemCommand;
        if (!GetAppsVerbCommandHandler(appName, appFilesystemCommand,
                                       false) ||
            IsPathInList(appFilesystemCommand, trackList))
          continue;
        ProcessPath(appList, trackList, appFilesystemCommand);
      }
    }
    regKey->Close();
  }


  // 8) General application's list - not file extension specific on windows
  workingRegistryPath = NS_LITERAL_STRING("Applications");

  rv = regKey->Open(nsIWindowsRegKey::ROOT_KEY_CLASSES_ROOT,
                    workingRegistryPath,
                    nsIWindowsRegKey::ACCESS_ENUMERATE_SUB_KEYS|
                    nsIWindowsRegKey::ACCESS_QUERY_VALUE);
  if (NS_SUCCEEDED(rv)) {
    PRUint32 count = 0;
    if (NS_SUCCEEDED(regKey->GetChildCount(&count)) && count > 0) {
      for (PRUint32 index = 0; index < count; index++) {
        nsAutoString appName;
        if (NS_FAILED(regKey->GetChildName(index, appName)))
          continue;

        // HKEY_CLASSES_ROOT\Applications\firefox.exe = "path params"
        nsAutoString appFilesystemCommand;
        if (!GetAppsVerbCommandHandler(appName, appFilesystemCommand,
                                       false) ||
            IsPathInList(appFilesystemCommand, trackList))
          continue;
        ProcessPath(appList, trackList, appFilesystemCommand);
      }
    }
  }

  // Return to the caller
  *_retval = appList;
  NS_ADDREF(*_retval);

  return NS_OK;
}

/* -*- Mode: C++; tab-width: 3; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsArrayEnumerator.h"
#include "nsCOMArray.h"
#include "nsLocalFile.h"
#include "nsMIMEInfoWin.h"
#include "nsNetUtil.h"
#include <windows.h>
#include <shellapi.h>
#include "nsIMutableArray.h"
#include "nsTArray.h"
#include <shlobj.h>
#include "nsIWindowsRegKey.h"
#include "nsUnicharUtils.h"
#include "nsTextToSubURI.h"
#include "nsVariant.h"
#include "mozilla/CmdLineAndEnvUtils.h"
#include "mozilla/ShellHeaderOnlyUtils.h"
#include "mozilla/StaticPrefs_browser.h"
#include "mozilla/UrlmonHeaderOnlyUtils.h"
#include "mozilla/UniquePtrExtensions.h"

#define RUNDLL32_EXE L"\\rundll32.exe"

NS_IMPL_ISUPPORTS_INHERITED(nsMIMEInfoWin, nsMIMEInfoBase, nsIPropertyBag)

nsMIMEInfoWin::~nsMIMEInfoWin() {}

nsresult nsMIMEInfoWin::LaunchDefaultWithFile(nsIFile* aFile) {
  // Launch the file, unless it is an executable.
  bool executable = true;
  aFile->IsExecutable(&executable);
  if (executable) return NS_ERROR_FAILURE;

  return aFile->Launch();
}

nsresult nsMIMEInfoWin::ShellExecuteWithIFile(nsIFile* aExecutable, int aArgc,
                                              const wchar_t** aArgv) {
  nsresult rv;

  NS_ASSERTION(aArgc >= 1, "aArgc must be at least 1");

  nsAutoString execPath;
  rv = aExecutable->GetTarget(execPath);
  if (NS_FAILED(rv) || execPath.IsEmpty()) {
    rv = aExecutable->GetPath(execPath);
  }
  if (NS_FAILED(rv)) {
    return rv;
  }

  auto assembledArgs = mozilla::MakeCommandLine(aArgc, aArgv);
  if (!assembledArgs) {
    return NS_ERROR_FILE_EXECUTION_FAILED;
  }

  _bstr_t execPathBStr(execPath.get());
  // Pass VT_ERROR/DISP_E_PARAMNOTFOUND to omit an optional RPC parameter
  // to execute a file with the default verb.
  _variant_t verbDefault(DISP_E_PARAMNOTFOUND, VT_ERROR);
  _variant_t workingDir;
  _variant_t showCmd(SW_SHOWNORMAL);

  // Ask Explorer to ShellExecute on our behalf, as some applications such as
  // Skype for Business do not start correctly when inheriting our process's
  // migitation policies.
  // It does not work in a special environment such as Citrix.  In such a case
  // we fall back to launching an application as a child process.  We need to
  // find a way to handle the combination of these interop issues.
  mozilla::LauncherVoidResult shellExecuteOk = mozilla::ShellExecuteByExplorer(
      execPathBStr, assembledArgs.get(), verbDefault, workingDir, showCmd);
  if (shellExecuteOk.isErr()) {
    // No need to pass assembledArgs to LaunchWithIProcess.  aArgv will be
    // processed in nsProcess::RunProcess.
    return LaunchWithIProcess(aExecutable, aArgc,
                              reinterpret_cast<const char16_t**>(aArgv));
  }

  return NS_OK;
}

NS_IMETHODIMP
nsMIMEInfoWin::LaunchWithFile(nsIFile* aFile) {
  nsresult rv;

  // it doesn't make any sense to call this on protocol handlers
  NS_ASSERTION(mClass == eMIMEInfo,
               "nsMIMEInfoBase should have mClass == eMIMEInfo");

  if (AutomationOnlyCheckIfLaunchStubbed(aFile)) {
    return NS_OK;
  }

  if (mPreferredAction == useSystemDefault) {
    if (mDefaultApplication &&
        StaticPrefs::browser_pdf_launchDefaultEdgeAsApp()) {
      // Since Edgium is the default handler for PDF and other kinds of files,
      // if we're using the OS default and it's Edgium prefer its app mode so it
      // operates as a viewer (without browser toolbars). Bug 1632277.
      nsAutoCString defaultAppExecutable;
      rv = mDefaultApplication->GetNativeLeafName(defaultAppExecutable);
      if (NS_SUCCEEDED(rv) &&
          defaultAppExecutable.LowerCaseEqualsLiteral("msedge.exe")) {
        nsAutoString path;
        rv = aFile->GetPath(path);
        if (NS_SUCCEEDED(rv)) {
          // If the --app flag doesn't work we'll want to fallback to a
          // regular path. Send two args so we call `msedge.exe --app={path}
          // {path}`.
          nsAutoString appArg;
          appArg.AppendLiteral("--app=");
          appArg.Append(path);
          const wchar_t* argv[] = {appArg.get(), path.get()};

          return ShellExecuteWithIFile(mDefaultApplication,
                                       mozilla::ArrayLength(argv), argv);
        }
      }
    }
    return LaunchDefaultWithFile(aFile);
  }

  if (mPreferredAction == useHelperApp) {
    if (!mPreferredApplication) return NS_ERROR_FILE_NOT_FOUND;

    // at the moment, we only know how to hand files off to local handlers
    nsCOMPtr<nsILocalHandlerApp> localHandler =
        do_QueryInterface(mPreferredApplication, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIFile> executable;
    rv = localHandler->GetExecutable(getter_AddRefs(executable));
    NS_ENSURE_SUCCESS(rv, rv);

    // Deal with local dll based handlers
    nsCString filename;
    executable->GetNativeLeafName(filename);
    if (filename.Length() > 4) {
      nsCString extension(Substring(filename, filename.Length() - 4, 4));

      if (extension.LowerCaseEqualsLiteral(".dll")) {
        nsAutoString args;

        // executable is rundll32, everything else is a list of parameters,
        // including the dll handler.
        if (!GetDllLaunchInfo(executable, aFile, args, false))
          return NS_ERROR_INVALID_ARG;

        WCHAR rundll32Path[MAX_PATH + sizeof(RUNDLL32_EXE) / sizeof(WCHAR) +
                           1] = {L'\0'};
        if (!GetSystemDirectoryW(rundll32Path, MAX_PATH)) {
          return NS_ERROR_FILE_NOT_FOUND;
        }
        lstrcatW(rundll32Path, RUNDLL32_EXE);

        SHELLEXECUTEINFOW seinfo;
        memset(&seinfo, 0, sizeof(seinfo));
        seinfo.cbSize = sizeof(SHELLEXECUTEINFOW);
        seinfo.fMask = 0;
        seinfo.hwnd = nullptr;
        seinfo.lpVerb = nullptr;
        seinfo.lpFile = rundll32Path;
        seinfo.lpParameters = args.get();
        seinfo.lpDirectory = nullptr;
        seinfo.nShow = SW_SHOWNORMAL;
        if (ShellExecuteExW(&seinfo)) return NS_OK;

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
            switch (GetLastError()) {
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
    nsAutoString path;
    aFile->GetPath(path);
    const wchar_t* argv[] = {path.get()};
    return ShellExecuteWithIFile(executable, mozilla::ArrayLength(argv), argv);
  }

  return NS_ERROR_INVALID_ARG;
}

NS_IMETHODIMP
nsMIMEInfoWin::GetHasDefaultHandler(bool* _retval) {
  // We have a default application if we have a description
  // We can ShellExecute anything; however, callers are probably interested if
  // there is really an application associated with this type of file
  *_retval = !mDefaultAppDescription.IsEmpty();
  return NS_OK;
}

NS_IMETHODIMP
nsMIMEInfoWin::GetEnumerator(nsISimpleEnumerator** _retval) {
  nsCOMArray<nsIVariant> properties;

  nsCOMPtr<nsIVariant> variant;
  GetProperty(u"defaultApplicationIconURL"_ns, getter_AddRefs(variant));
  if (variant) properties.AppendObject(variant);

  GetProperty(u"customApplicationIconURL"_ns, getter_AddRefs(variant));
  if (variant) properties.AppendObject(variant);

  return NS_NewArrayEnumerator(_retval, properties, NS_GET_IID(nsIVariant));
}

static nsresult GetIconURLVariant(nsIFile* aApplication, nsIVariant** _retval) {
  nsAutoCString fileURLSpec;
  NS_GetURLSpecFromFile(aApplication, fileURLSpec);
  nsAutoCString iconURLSpec;
  iconURLSpec.AssignLiteral("moz-icon://");
  iconURLSpec += fileURLSpec;
  RefPtr<nsVariant> writable(new nsVariant());
  writable->SetAsAUTF8String(iconURLSpec);
  writable.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
nsMIMEInfoWin::GetProperty(const nsAString& aName, nsIVariant** _retval) {
  nsresult rv;
  if (mDefaultApplication &&
      aName.EqualsLiteral(PROPERTY_DEFAULT_APP_ICON_URL)) {
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
nsresult nsMIMEInfoWin::LoadUriInternal(nsIURI* aURL) {
  nsresult rv = NS_OK;

  // 1. Find the default app for this protocol
  // 2. Set up the command line
  // 3. Launch the app.

  // For now, we'll just cheat essentially, check for the command line
  // then just call ShellExecute()!

  if (aURL) {
    // extract the url spec from the url
    nsAutoCString urlSpec;
    aURL->GetAsciiSpec(urlSpec);

    // Unescape non-ASCII characters in the URL
    nsAutoString utf16Spec;
    if (NS_FAILED(nsTextToSubURI::UnEscapeNonAsciiURI("UTF-8"_ns, urlSpec,
                                                      utf16Spec))) {
      CopyASCIItoUTF16(urlSpec, utf16Spec);
    }

    // Ask the shell/urlmon to parse |utf16Spec| to avoid malformed URLs.
    // Failure is indicative of a potential security issue so we should
    // bail out if so.
    LauncherResult<_bstr_t> validatedUri = UrlmonValidateUri(utf16Spec.get());
    if (validatedUri.isErr()) {
      return NS_ERROR_FAILURE;
    }

    _variant_t args;
    _variant_t verb(L"open");
    _variant_t workingDir;
    _variant_t showCmd(SW_SHOWNORMAL);

    // To open a uri, we first try ShellExecuteByExplorer, which starts a new
    // process as a child process of explorer.exe, because applications may not
    // support the mitigation policies inherited from our process.  If it fails,
    // we fall back to ShellExecuteExW.
    //
    // For Thunderbird, however, there is a known issue that
    // ShellExecuteByExplorer succeeds but explorer.exe shows an error popup
    // if a uri to open includes credentials.  This does not happen in Firefox
    // because Firefox does not have to launch a process to open a uri.
    //
    // Since Thunderbird does not use mitigation policies which could cause
    // compatibility issues, we get no benefit from using
    // ShellExecuteByExplorer.  Thus we skip it and go straight to
    // ShellExecuteExW for Thunderbird.
#ifndef MOZ_THUNDERBIRD
    mozilla::LauncherVoidResult shellExecuteOk =
        mozilla::ShellExecuteByExplorer(validatedUri.inspect(), args, verb,
                                        workingDir, showCmd);
    if (shellExecuteOk.isOk()) {
      return NS_OK;
    }
#endif  // MOZ_THUNDERBIRD

    SHELLEXECUTEINFOW sinfo = {sizeof(sinfo)};
    sinfo.fMask = SEE_MASK_NOASYNC;
    sinfo.lpVerb = V_BSTR(&verb);
    sinfo.nShow = showCmd;
    sinfo.lpFile = validatedUri.inspect();

    BOOL result = ShellExecuteExW(&sinfo);
    if (!result || reinterpret_cast<LONG_PTR>(sinfo.hInstApp) < 32) {
      rv = NS_ERROR_FAILURE;
    }
  }

  return rv;
}

// Given a path to a local file, return its nsILocalHandlerApp instance.
bool nsMIMEInfoWin::GetLocalHandlerApp(const nsAString& aCommandHandler,
                                       nsCOMPtr<nsILocalHandlerApp>& aApp) {
  nsCOMPtr<nsIFile> locfile;
  nsresult rv = NS_NewLocalFile(aCommandHandler, true, getter_AddRefs(locfile));
  if (NS_FAILED(rv)) return false;

  aApp = do_CreateInstance("@mozilla.org/uriloader/local-handler-app;1");
  if (!aApp) return false;

  aApp->SetExecutable(locfile);
  return true;
}

// Return the cleaned up file path associated with a command verb
// located in root/Applications.
bool nsMIMEInfoWin::GetAppsVerbCommandHandler(const nsAString& appExeName,
                                              nsAString& applicationPath,
                                              bool edit) {
  nsCOMPtr<nsIWindowsRegKey> appKey =
      do_CreateInstance("@mozilla.org/windows-registry-key;1");
  if (!appKey) return false;

  // HKEY_CLASSES_ROOT\Applications\iexplore.exe
  nsAutoString applicationsPath;
  applicationsPath.AppendLiteral("Applications\\");
  applicationsPath.Append(appExeName);

  nsresult rv =
      appKey->Open(nsIWindowsRegKey::ROOT_KEY_CLASSES_ROOT, applicationsPath,
                   nsIWindowsRegKey::ACCESS_QUERY_VALUE);
  if (NS_FAILED(rv)) return false;

  // Check for the NoOpenWith flag, if it exists
  uint32_t value;
  if (NS_SUCCEEDED(appKey->ReadIntValue(u"NoOpenWith"_ns, &value)) &&
      value == 1)
    return false;

  nsAutoString dummy;
  if (NS_SUCCEEDED(appKey->ReadStringValue(u"NoOpenWith"_ns, dummy)))
    return false;

  appKey->Close();

  // HKEY_CLASSES_ROOT\Applications\iexplore.exe\shell\open\command
  applicationsPath.AssignLiteral("Applications\\");
  applicationsPath.Append(appExeName);
  if (!edit)
    applicationsPath.AppendLiteral("\\shell\\open\\command");
  else
    applicationsPath.AppendLiteral("\\shell\\edit\\command");

  rv = appKey->Open(nsIWindowsRegKey::ROOT_KEY_CLASSES_ROOT, applicationsPath,
                    nsIWindowsRegKey::ACCESS_QUERY_VALUE);
  if (NS_FAILED(rv)) return false;

  nsAutoString appFilesystemCommand;
  if (NS_SUCCEEDED(appKey->ReadStringValue(u""_ns, appFilesystemCommand))) {
    // Expand environment vars, clean up any misc.
    if (!nsLocalFile::CleanupCmdHandlerPath(appFilesystemCommand)) return false;

    applicationPath = appFilesystemCommand;
    return true;
  }
  return false;
}

// Return a fully populated command string based on
// passing information. Used in launchWithFile to trace
// back to the full handler path based on the dll.
// (dll, targetfile, return args, open/edit)
bool nsMIMEInfoWin::GetDllLaunchInfo(nsIFile* aDll, nsIFile* aFile,
                                     nsAString& args, bool edit) {
  if (!aDll || !aFile) return false;

  nsString appExeName;
  aDll->GetLeafName(appExeName);

  nsCOMPtr<nsIWindowsRegKey> appKey =
      do_CreateInstance("@mozilla.org/windows-registry-key;1");
  if (!appKey) return false;

  // HKEY_CLASSES_ROOT\Applications\iexplore.exe
  nsAutoString applicationsPath;
  applicationsPath.AppendLiteral("Applications\\");
  applicationsPath.Append(appExeName);

  nsresult rv =
      appKey->Open(nsIWindowsRegKey::ROOT_KEY_CLASSES_ROOT, applicationsPath,
                   nsIWindowsRegKey::ACCESS_QUERY_VALUE);
  if (NS_FAILED(rv)) return false;

  // Check for the NoOpenWith flag, if it exists
  uint32_t value;
  rv = appKey->ReadIntValue(u"NoOpenWith"_ns, &value);
  if (NS_SUCCEEDED(rv) && value == 1) return false;

  nsAutoString dummy;
  if (NS_SUCCEEDED(appKey->ReadStringValue(u"NoOpenWith"_ns, dummy)))
    return false;

  appKey->Close();

  // HKEY_CLASSES_ROOT\Applications\iexplore.exe\shell\open\command
  applicationsPath.AssignLiteral("Applications\\");
  applicationsPath.Append(appExeName);
  if (!edit)
    applicationsPath.AppendLiteral("\\shell\\open\\command");
  else
    applicationsPath.AppendLiteral("\\shell\\edit\\command");

  rv = appKey->Open(nsIWindowsRegKey::ROOT_KEY_CLASSES_ROOT, applicationsPath,
                    nsIWindowsRegKey::ACCESS_QUERY_VALUE);
  if (NS_FAILED(rv)) return false;

  nsAutoString appFilesystemCommand;
  if (NS_SUCCEEDED(appKey->ReadStringValue(u""_ns, appFilesystemCommand))) {
    // Replace embedded environment variables.
    uint32_t bufLength =
        ::ExpandEnvironmentStringsW(appFilesystemCommand.get(), nullptr, 0);
    if (bufLength == 0)  // Error
      return false;

    auto destination = mozilla::MakeUniqueFallible<wchar_t[]>(bufLength);
    if (!destination) return false;
    if (!::ExpandEnvironmentStringsW(appFilesystemCommand.get(),
                                     destination.get(), bufLength))
      return false;

    appFilesystemCommand.Assign(destination.get());

    // C:\Windows\System32\rundll32.exe "C:\Program Files\Windows
    // Photo Gallery\PhotoViewer.dll", ImageView_Fullscreen %1
    nsAutoString params;
    constexpr auto rundllSegment = u"rundll32.exe "_ns;
    int32_t index = appFilesystemCommand.Find(rundllSegment);
    if (index > kNotFound) {
      params.Append(
          Substring(appFilesystemCommand, index + rundllSegment.Length()));
    } else {
      params.Append(appFilesystemCommand);
    }

    // check to make sure we have a %1 and fill it
    constexpr auto percentOneParam = u"%1"_ns;
    index = params.Find(percentOneParam);
    if (index == kNotFound)  // no parameter
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
                                                bool edit) {
  nsCOMPtr<nsIWindowsRegKey> appKey =
      do_CreateInstance("@mozilla.org/windows-registry-key;1");
  if (!appKey) return false;

  nsAutoString appProgId(appProgIDName);

  // HKEY_CLASSES_ROOT\Windows.XPSReachViewer\shell\open\command
  if (!edit)
    appProgId.AppendLiteral("\\shell\\open\\command");
  else
    appProgId.AppendLiteral("\\shell\\edit\\command");

  nsresult rv = appKey->Open(nsIWindowsRegKey::ROOT_KEY_CLASSES_ROOT, appProgId,
                             nsIWindowsRegKey::ACCESS_QUERY_VALUE);
  if (NS_FAILED(rv)) return false;

  nsAutoString appFilesystemCommand;
  if (NS_SUCCEEDED(appKey->ReadStringValue(u""_ns, appFilesystemCommand))) {
    // Expand environment vars, clean up any misc.
    if (!nsLocalFile::CleanupCmdHandlerPath(appFilesystemCommand)) return false;

    applicationPath = appFilesystemCommand;
    return true;
  }
  return false;
}

// Helper routine used in tracking app lists. Converts path
// entries to lower case and stores them in the trackList array.
void nsMIMEInfoWin::ProcessPath(nsCOMPtr<nsIMutableArray>& appList,
                                nsTArray<nsString>& trackList,
                                const nsAString& appFilesystemCommand) {
  nsAutoString lower(appFilesystemCommand);
  ToLowerCase(lower);

  // Don't include firefox.exe in the list
  WCHAR exe[MAX_PATH + 1];
  uint32_t len = GetModuleFileNameW(nullptr, exe, MAX_PATH);
  if (len < MAX_PATH && len != 0) {
    int32_t index = lower.Find(exe);
    if (index != -1) return;
  }

  nsCOMPtr<nsILocalHandlerApp> aApp;
  if (!GetLocalHandlerApp(appFilesystemCommand, aApp)) return;

  // Save in our main tracking arrays
  appList->AppendElement(aApp);
  trackList.AppendElement(lower);
}

// Helper routine that handles a compare between a path
// and an array of paths.
static bool IsPathInList(nsAString& appPath, nsTArray<nsString>& trackList) {
  // trackList data is always lowercase, see ProcessPath
  // above.
  nsAutoString tmp(appPath);
  ToLowerCase(tmp);

  for (uint32_t i = 0; i < trackList.Length(); i++) {
    if (tmp.Equals(trackList[i])) return true;
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
nsMIMEInfoWin::GetPossibleLocalHandlers(nsIArray** _retval) {
  nsresult rv;

  *_retval = nullptr;

  nsCOMPtr<nsIMutableArray> appList = do_CreateInstance("@mozilla.org/array;1");

  if (!appList) return NS_ERROR_FAILURE;

  nsTArray<nsString> trackList;

  nsAutoCString fileExt;
  GetPrimaryExtension(fileExt);

  nsCOMPtr<nsIWindowsRegKey> regKey =
      do_CreateInstance("@mozilla.org/windows-registry-key;1");
  if (!regKey) return NS_ERROR_FAILURE;
  nsCOMPtr<nsIWindowsRegKey> appKey =
      do_CreateInstance("@mozilla.org/windows-registry-key;1");
  if (!appKey) return NS_ERROR_FAILURE;

  nsAutoString workingRegistryPath;

  bool extKnown = false;
  if (fileExt.IsEmpty()) {
    extKnown = true;
    // Mime type discovery is possible in some cases, through
    // HKEY_CLASSES_ROOT\MIME\Database\Content Type, however, a number
    // of file extensions related to mime type are simply not defined,
    // (application/rss+xml & application/atom+xml are good examples)
    // in which case we can only provide a generic list.
    nsAutoCString mimeType;
    GetMIMEType(mimeType);
    if (!mimeType.IsEmpty()) {
      workingRegistryPath.AppendLiteral("MIME\\Database\\Content Type\\");
      workingRegistryPath.Append(NS_ConvertASCIItoUTF16(mimeType));

      rv = regKey->Open(nsIWindowsRegKey::ROOT_KEY_CLASSES_ROOT,
                        workingRegistryPath,
                        nsIWindowsRegKey::ACCESS_QUERY_VALUE);
      if (NS_SUCCEEDED(rv)) {
        nsAutoString mimeFileExt;
        if (NS_SUCCEEDED(regKey->ReadStringValue(u""_ns, mimeFileExt))) {
          CopyUTF16toUTF8(mimeFileExt, fileExt);
          extKnown = false;
        }
      }
    }
  }

  nsAutoString fileExtToUse;
  if (!fileExt.IsEmpty() && fileExt.First() != '.') {
    fileExtToUse = char16_t('.');
  }
  fileExtToUse.Append(NS_ConvertUTF8toUTF16(fileExt));

  // Note, the order in which these occur has an effect on the
  // validity of the resulting display list.

  if (!extKnown) {
    // 1) Get the default handler if it exists
    workingRegistryPath = fileExtToUse;

    rv =
        regKey->Open(nsIWindowsRegKey::ROOT_KEY_CLASSES_ROOT,
                     workingRegistryPath, nsIWindowsRegKey::ACCESS_QUERY_VALUE);
    if (NS_SUCCEEDED(rv)) {
      nsAutoString appProgId;
      if (NS_SUCCEEDED(regKey->ReadStringValue(u""_ns, appProgId))) {
        // Bug 358297 - ignore the embedded internet explorer handler
        if (appProgId != u"XPSViewer.Document"_ns) {
          nsAutoString appFilesystemCommand;
          if (GetProgIDVerbCommandHandler(appProgId, appFilesystemCommand,
                                          false) &&
              !IsPathInList(appFilesystemCommand, trackList)) {
            ProcessPath(appList, trackList, appFilesystemCommand);
          }
        }
      }
      regKey->Close();
    }

    // 2) list HKEY_CLASSES_ROOT\.ext\OpenWithList

    workingRegistryPath = fileExtToUse;
    workingRegistryPath.AppendLiteral("\\OpenWithList");

    rv =
        regKey->Open(nsIWindowsRegKey::ROOT_KEY_CLASSES_ROOT,
                     workingRegistryPath, nsIWindowsRegKey::ACCESS_QUERY_VALUE);
    if (NS_SUCCEEDED(rv)) {
      uint32_t count = 0;
      if (NS_SUCCEEDED(regKey->GetValueCount(&count)) && count > 0) {
        for (uint32_t index = 0; index < count; index++) {
          nsAutoString appName;
          if (NS_FAILED(regKey->GetValueName(index, appName))) continue;

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

    // 3) List HKEY_CLASSES_ROOT\.ext\OpenWithProgids, with the
    // different step of resolving the progids for the command handler.

    workingRegistryPath = fileExtToUse;
    workingRegistryPath.AppendLiteral("\\OpenWithProgids");

    rv =
        regKey->Open(nsIWindowsRegKey::ROOT_KEY_CLASSES_ROOT,
                     workingRegistryPath, nsIWindowsRegKey::ACCESS_QUERY_VALUE);
    if (NS_SUCCEEDED(rv)) {
      uint32_t count = 0;
      if (NS_SUCCEEDED(regKey->GetValueCount(&count)) && count > 0) {
        for (uint32_t index = 0; index < count; index++) {
          // HKEY_CLASSES_ROOT\.ext\OpenWithProgids\Windows.XPSReachViewer
          nsAutoString appProgId;
          if (NS_FAILED(regKey->GetValueName(index, appProgId))) continue;

          nsAutoString appFilesystemCommand;
          if (!GetProgIDVerbCommandHandler(appProgId, appFilesystemCommand,
                                           false) ||
              IsPathInList(appFilesystemCommand, trackList))
            continue;
          ProcessPath(appList, trackList, appFilesystemCommand);
        }
      }
      regKey->Close();
    }

    // 4) Add any non configured applications located in the MRU list

    // HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion
    // \Explorer\FileExts\.ext\OpenWithList
    workingRegistryPath = nsLiteralString(
        u"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts\\");
    workingRegistryPath += fileExtToUse;
    workingRegistryPath.AppendLiteral("\\OpenWithList");

    rv =
        regKey->Open(nsIWindowsRegKey::ROOT_KEY_CURRENT_USER,
                     workingRegistryPath, nsIWindowsRegKey::ACCESS_QUERY_VALUE);
    if (NS_SUCCEEDED(rv)) {
      uint32_t count = 0;
      if (NS_SUCCEEDED(regKey->GetValueCount(&count)) && count > 0) {
        for (uint32_t index = 0; index < count; index++) {
          nsAutoString appName, appValue;
          if (NS_FAILED(regKey->GetValueName(index, appName))) continue;
          if (appName.EqualsLiteral("MRUList")) continue;
          if (NS_FAILED(regKey->ReadStringValue(appName, appValue))) continue;

          // HKEY_CLASSES_ROOT\Applications\firefox.exe = "path params"
          nsAutoString appFilesystemCommand;
          if (!GetAppsVerbCommandHandler(appValue, appFilesystemCommand,
                                         false) ||
              IsPathInList(appFilesystemCommand, trackList))
            continue;
          ProcessPath(appList, trackList, appFilesystemCommand);
        }
      }
    }

    // 5) Add any non configured progids in the MRU list, with the
    // different step of resolving the progids for the command handler.

    // HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion
    // \Explorer\FileExts\.ext\OpenWithProgids
    workingRegistryPath = nsLiteralString(
        u"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts\\");
    workingRegistryPath += fileExtToUse;
    workingRegistryPath.AppendLiteral("\\OpenWithProgids");

    regKey->Open(nsIWindowsRegKey::ROOT_KEY_CURRENT_USER, workingRegistryPath,
                 nsIWindowsRegKey::ACCESS_QUERY_VALUE);
    if (NS_SUCCEEDED(rv)) {
      uint32_t count = 0;
      if (NS_SUCCEEDED(regKey->GetValueCount(&count)) && count > 0) {
        for (uint32_t index = 0; index < count; index++) {
          nsAutoString appIndex, appProgId;
          if (NS_FAILED(regKey->GetValueName(index, appProgId))) continue;

          nsAutoString appFilesystemCommand;
          if (!GetProgIDVerbCommandHandler(appProgId, appFilesystemCommand,
                                           false) ||
              IsPathInList(appFilesystemCommand, trackList))
            continue;
          ProcessPath(appList, trackList, appFilesystemCommand);
        }
      }
      regKey->Close();
    }

    // 6) Check the perceived type value, and use this to lookup the
    // perceivedtype open with list.
    // http://msdn2.microsoft.com/en-us/library/aa969373.aspx

    workingRegistryPath = fileExtToUse;

    regKey->Open(nsIWindowsRegKey::ROOT_KEY_CLASSES_ROOT, workingRegistryPath,
                 nsIWindowsRegKey::ACCESS_QUERY_VALUE);
    if (NS_SUCCEEDED(rv)) {
      nsAutoString perceivedType;
      rv = regKey->ReadStringValue(u"PerceivedType"_ns, perceivedType);
      if (NS_SUCCEEDED(rv)) {
        nsAutoString openWithListPath(u"SystemFileAssociations\\"_ns);
        openWithListPath.Append(perceivedType);  // no period
        openWithListPath.AppendLiteral("\\OpenWithList");

        nsresult rv = appKey->Open(nsIWindowsRegKey::ROOT_KEY_CLASSES_ROOT,
                                   openWithListPath,
                                   nsIWindowsRegKey::ACCESS_QUERY_VALUE);
        if (NS_SUCCEEDED(rv)) {
          uint32_t count = 0;
          if (NS_SUCCEEDED(regKey->GetValueCount(&count)) && count > 0) {
            for (uint32_t index = 0; index < count; index++) {
              nsAutoString appName;
              if (NS_FAILED(regKey->GetValueName(index, appName))) continue;

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
  }  // extKnown == false

  // 7) list global HKEY_CLASSES_ROOT\*\OpenWithList
  // Listing general purpose handlers, not specific to a mime type or file
  // extension

  workingRegistryPath = u"*\\OpenWithList"_ns;

  rv = regKey->Open(nsIWindowsRegKey::ROOT_KEY_CLASSES_ROOT,
                    workingRegistryPath, nsIWindowsRegKey::ACCESS_QUERY_VALUE);
  if (NS_SUCCEEDED(rv)) {
    uint32_t count = 0;
    if (NS_SUCCEEDED(regKey->GetValueCount(&count)) && count > 0) {
      for (uint32_t index = 0; index < count; index++) {
        nsAutoString appName;
        if (NS_FAILED(regKey->GetValueName(index, appName))) continue;

        // HKEY_CLASSES_ROOT\Applications\firefox.exe = "path params"
        nsAutoString appFilesystemCommand;
        if (!GetAppsVerbCommandHandler(appName, appFilesystemCommand, false) ||
            IsPathInList(appFilesystemCommand, trackList))
          continue;
        ProcessPath(appList, trackList, appFilesystemCommand);
      }
    }
    regKey->Close();
  }

  // 8) General application's list - not file extension specific on windows
  workingRegistryPath = u"Applications"_ns;

  rv =
      regKey->Open(nsIWindowsRegKey::ROOT_KEY_CLASSES_ROOT, workingRegistryPath,
                   nsIWindowsRegKey::ACCESS_ENUMERATE_SUB_KEYS |
                       nsIWindowsRegKey::ACCESS_QUERY_VALUE);
  if (NS_SUCCEEDED(rv)) {
    uint32_t count = 0;
    if (NS_SUCCEEDED(regKey->GetChildCount(&count)) && count > 0) {
      for (uint32_t index = 0; index < count; index++) {
        nsAutoString appName;
        if (NS_FAILED(regKey->GetChildName(index, appName))) continue;

        // HKEY_CLASSES_ROOT\Applications\firefox.exe = "path params"
        nsAutoString appFilesystemCommand;
        if (!GetAppsVerbCommandHandler(appName, appFilesystemCommand, false) ||
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

NS_IMETHODIMP
nsMIMEInfoWin::IsCurrentAppOSDefault(bool* _retval) {
  *_retval = false;
  if (mDefaultApplication) {
    // Determine if the default executable is our executable.
    nsCOMPtr<nsIFile> ourBinary;
    XRE_GetBinaryPath(getter_AddRefs(ourBinary));
    bool isSame = false;
    nsresult rv = mDefaultApplication->Equals(ourBinary, &isSame);
    if (NS_FAILED(rv)) {
      return rv;
    }
    *_retval = isSame;
  }
  return NS_OK;
}

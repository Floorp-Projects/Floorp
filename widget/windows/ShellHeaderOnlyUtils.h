/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ShellHeaderOnlyUtils_h
#define mozilla_ShellHeaderOnlyUtils_h

#include "mozilla/LauncherResult.h"
#include "mozilla/WinHeaderOnlyUtils.h"

#include <objbase.h>

#include <exdisp.h>
#include <shldisp.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <shobjidl.h>
#include <shtypes.h>
// NB: include this after shldisp.h so its macros do not conflict with COM
// interfaces defined by shldisp.h
#include <shellapi.h>

#include <comdef.h>
#include <comutil.h>

#include "mozilla/RefPtr.h"
#include "mozilla/TypeTraits.h"
#include "mozilla/UniquePtr.h"

namespace mozilla {

/**
 * Ask the current user's Desktop to ShellExecute on our behalf, thus causing
 * the resulting launched process to inherit its security priviliges from
 * Explorer instead of our process.
 *
 * This is useful in two scenarios, in particular:
 *   * We are running as an elevated user and we want to start something as the
 *     "normal" user;
 *   * We are starting a process that is incompatible with our process's
 *     process mitigation policies. By delegating to Explorer, the child process
 *     will not be affected by our process mitigations.
 *
 * Since this communication happens over DCOM, Explorer's COM DACL governs
 * whether or not we can execute against it, thus avoiding privilege escalation.
 */
inline LauncherVoidResult ShellExecuteByExplorer(const _bstr_t& aPath,
                                                 const _variant_t& aArgs,
                                                 const _variant_t& aVerb,
                                                 const _variant_t& aWorkingDir,
                                                 const _variant_t& aShowCmd) {
  // NB: Explorer may be a local server, not an inproc server
  RefPtr<IShellWindows> shellWindows;
  HRESULT hr = ::CoCreateInstance(
      CLSID_ShellWindows, nullptr, CLSCTX_INPROC_SERVER | CLSCTX_LOCAL_SERVER,
      IID_IShellWindows, getter_AddRefs(shellWindows));
  if (FAILED(hr)) {
    return LAUNCHER_ERROR_FROM_HRESULT(hr);
  }

  // 1. Find the shell view for the desktop.
  _variant_t loc(int(CSIDL_DESKTOP));
  _variant_t empty;
  long hwnd;
  RefPtr<IDispatch> dispDesktop;
  hr = shellWindows->FindWindowSW(&loc, &empty, SWC_DESKTOP, &hwnd,
                                  SWFO_NEEDDISPATCH,
                                  getter_AddRefs(dispDesktop));
  if (FAILED(hr)) {
    return LAUNCHER_ERROR_FROM_HRESULT(hr);
  }

  if (hr == S_FALSE) {
    // The call succeeded but the window was not found.
    return LAUNCHER_ERROR_FROM_WIN32(ERROR_NOT_FOUND);
  }

  RefPtr<IServiceProvider> servProv;
  hr = dispDesktop->QueryInterface(IID_IServiceProvider,
                                   getter_AddRefs(servProv));
  if (FAILED(hr)) {
    return LAUNCHER_ERROR_FROM_HRESULT(hr);
  }

  RefPtr<IShellBrowser> browser;
  hr = servProv->QueryService(SID_STopLevelBrowser, IID_IShellBrowser,
                              getter_AddRefs(browser));
  if (FAILED(hr)) {
    return LAUNCHER_ERROR_FROM_HRESULT(hr);
  }

  RefPtr<IShellView> activeShellView;
  hr = browser->QueryActiveShellView(getter_AddRefs(activeShellView));
  if (FAILED(hr)) {
    return LAUNCHER_ERROR_FROM_HRESULT(hr);
  }

  // 2. Get the automation object for the desktop.
  RefPtr<IDispatch> dispView;
  hr = activeShellView->GetItemObject(SVGIO_BACKGROUND, IID_IDispatch,
                                      getter_AddRefs(dispView));
  if (FAILED(hr)) {
    return LAUNCHER_ERROR_FROM_HRESULT(hr);
  }

  RefPtr<IShellFolderViewDual> folderView;
  hr = dispView->QueryInterface(IID_IShellFolderViewDual,
                                getter_AddRefs(folderView));
  if (FAILED(hr)) {
    return LAUNCHER_ERROR_FROM_HRESULT(hr);
  }

  // 3. Get the interface to IShellDispatch2
  RefPtr<IDispatch> dispShell;
  hr = folderView->get_Application(getter_AddRefs(dispShell));
  if (FAILED(hr)) {
    return LAUNCHER_ERROR_FROM_HRESULT(hr);
  }

  RefPtr<IShellDispatch2> shellDisp;
  hr =
      dispShell->QueryInterface(IID_IShellDispatch2, getter_AddRefs(shellDisp));
  if (FAILED(hr)) {
    return LAUNCHER_ERROR_FROM_HRESULT(hr);
  }

  // shellapi.h macros interfere with the correct naming of the method being
  // called on IShellDispatch2. Temporarily remove that definition.
#if defined(ShellExecute)
#  define MOZ_REDEFINE_SHELLEXECUTE
#  undef ShellExecute
#endif  // defined(ShellExecute)

  // 4. Now call IShellDispatch2::ShellExecute to ask Explorer to execute.
  hr = shellDisp->ShellExecute(aPath, aArgs, aVerb, aWorkingDir, aShowCmd);
  if (FAILED(hr)) {
    return LAUNCHER_ERROR_FROM_HRESULT(hr);
  }

  // Restore the macro that was removed prior to IShellDispatch2::ShellExecute
#if defined(MOZ_REDEFINE_SHELLEXECUTE)
#  if defined(UNICODE)
#    define ShellExecute ShellExecuteW
#  else
#    define ShellExecute ShellExecuteA
#  endif
#  undef MOZ_REDEFINE_SHELLEXECUTE
#endif  // defined(MOZ_REDEFINE_SHELLEXECUTE)

  return Ok();
}

using UniqueAbsolutePidl =
    UniquePtr<RemovePointer<PIDLIST_ABSOLUTE>::Type, CoTaskMemFreeDeleter>;

inline LauncherResult<UniqueAbsolutePidl> ShellParseDisplayName(
    const wchar_t* aPath) {
  PIDLIST_ABSOLUTE rawAbsPidl = nullptr;
  SFGAOF sfgao;
  HRESULT hr = ::SHParseDisplayName(aPath, nullptr, &rawAbsPidl, 0, &sfgao);
  if (FAILED(hr)) {
    return LAUNCHER_ERROR_FROM_HRESULT(hr);
  }

  return UniqueAbsolutePidl(rawAbsPidl);
}

}  // namespace mozilla

#endif  // mozilla_ShellHeaderOnlyUtils_h

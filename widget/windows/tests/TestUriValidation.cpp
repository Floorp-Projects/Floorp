/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "mozilla/UrlmonHeaderOnlyUtils.h"
#include "TestUrisToValidate.h"

#include <urlmon.h>

using namespace mozilla;

static LauncherResult<_bstr_t> ShellValidateUri(const wchar_t* aUri) {
  LauncherResult<UniqueAbsolutePidl> pidlResult = ShellParseDisplayName(aUri);
  if (pidlResult.isErr()) {
    return LAUNCHER_ERROR_FROM_RESULT(pidlResult);
  }
  UniqueAbsolutePidl pidl = pidlResult.unwrap();

  // |pidl| is an absolute path. IShellFolder::GetDisplayNameOf requires a
  // valid child ID, so the first thing we need to resolve is the IShellFolder
  // for |pidl|'s parent, as well as the childId that represents |pidl|.
  // Fortunately SHBindToParent does exactly that!
  PCUITEMID_CHILD childId = nullptr;
  RefPtr<IShellFolder> parentFolder;
  HRESULT hr = SHBindToParent(pidl.get(), IID_IShellFolder,
                              getter_AddRefs(parentFolder), &childId);
  if (FAILED(hr)) {
    return LAUNCHER_ERROR_FROM_HRESULT(hr);
  }

  // Now we retrieve the display name of |childId|, telling the shell that we
  // plan to have the string parsed.
  STRRET strret;
  hr = parentFolder->GetDisplayNameOf(childId, SHGDN_FORPARSING, &strret);
  if (FAILED(hr)) {
    return LAUNCHER_ERROR_FROM_HRESULT(hr);
  }

  // StrRetToBSTR automatically takes care of freeing any dynamically
  // allocated memory in |strret|.
  _bstr_t bstrUri;
  hr = StrRetToBSTR(&strret, nullptr, bstrUri.GetAddress());
  if (FAILED(hr)) {
    return LAUNCHER_ERROR_FROM_HRESULT(hr);
  }

  return bstrUri;
}

static LauncherResult<_bstr_t> GetFragment(const wchar_t* aUri) {
  constexpr DWORD flags =
      Uri_CREATE_NO_DECODE_EXTRA_INFO | Uri_CREATE_CANONICALIZE |
      Uri_CREATE_CRACK_UNKNOWN_SCHEMES | Uri_CREATE_PRE_PROCESS_HTML_URI |
      Uri_CREATE_IE_SETTINGS;
  RefPtr<IUri> uri;
  HRESULT hr = CreateUri(aUri, flags, 0, getter_AddRefs(uri));
  if (FAILED(hr)) {
    return LAUNCHER_ERROR_FROM_HRESULT(hr);
  }

  _bstr_t bstrFragment;
  hr = uri->GetFragment(bstrFragment.GetAddress());
  if (FAILED(hr)) {
    return LAUNCHER_ERROR_FROM_HRESULT(hr);
  }
  return bstrFragment;
}

static bool RunSingleTest(const wchar_t* aUri) {
  LauncherResult<_bstr_t> uriOld = ShellValidateUri(aUri),
                          uriNew = UrlmonValidateUri(aUri);
  if (uriOld.isErr() != uriNew.isErr()) {
    printf("TEST-FAILED | UriValidation | Validation result mismatch on %S\n",
           aUri);
    return false;
  }

  if (uriOld.isErr()) {
    if (uriOld.unwrapErr().mError != uriNew.unwrapErr().mError) {
      printf("TEST-FAILED | UriValidation | Error code mismatch on %S\n", aUri);
      return false;
    }
    return true;
  }

  LauncherResult<_bstr_t> bstrFragment = GetFragment(aUri);
  if (bstrFragment.isErr()) {
    printf("TEST-FAILED | UriValidation | Failed to get a fragment from %S\n",
           aUri);
    return false;
  }

  // We validate a uri with two logics: the current one UrlmonValidateUri and
  // the older one ShellValidateUri, to make sure the same validation result.
  // We introduced UrlmonValidateUri because ShellValidateUri drops a fragment
  // in a uri due to the design of Windows.  To bypass the fragment issue, we
  // extract a fragment and appends it into the validated string, and compare.
  _bstr_t bstrUriOldCorrected = uriOld.unwrap() + bstrFragment.unwrap();
  const _bstr_t& bstrUriNew = uriNew.unwrap();
  if (bstrUriOldCorrected != bstrUriNew) {
    printf("TEST-FAILED | UriValidation | %S %S %S\n", aUri,
           static_cast<const wchar_t*>(bstrUriOldCorrected),
           static_cast<const wchar_t*>(bstrUriNew));
    return false;
  }

  return true;
}

int wmain(int argc, wchar_t* argv[]) {
  HRESULT hr = CoInitialize(nullptr);
  if (FAILED(hr)) {
    return 1;
  }

  bool isOk = true;

  if (argc == 2) {
    isOk = RunSingleTest(argv[1]);
  } else {
    for (const wchar_t*& testUri : kTestUris) {
      if (!RunSingleTest(testUri)) {
        isOk = false;
      }
    }
  }

  CoUninitialize();
  return isOk ? 0 : 1;
}

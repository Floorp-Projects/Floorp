/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef mozilla_UrlmonHeaderOnlyUtils_h
#define mozilla_UrlmonHeaderOnlyUtils_h

#include "mozilla/ShellHeaderOnlyUtils.h"

namespace mozilla {

/**
 * We used to validate a uri with SHParseDisplayName to mitigate the Windows
 * bug (Bug 394974).  However, Bug 1573051 revealed an issue that a fragment,
 * a string following a hash mark (#), is dropped when we extract a string
 * from PIDL.  This is the intended behavior of Windows.
 *
 * To deal with the fragment issue as well as keeping our mitigation, we
 * decided to use CreateUri to validate a uri string, but we also keep using
 * SHParseDisplayName as a pre-check.  This is because there are several
 * cases where CreateUri succeeds while SHParseDisplayName fails such as
 * a non-existent file: uri.
 *
 * To minimize the impact of introducing CreateUri into the validation logic,
 * we try to mimic the logic of windows_storage!IUriToPidl (ieframe!IUriToPidl
 * in Win7) which is executed behind SHParseDisplayName.
 * What IUriToPidl does is:
 *   1) If a given uri has a fragment, removes a fragment.
 *   2) Takes an absolute uri if it's available in the given uri, otherwise
 *      takes a raw uri.
 *
 * As we need to get a full uri including a fragment, this function does 2).
 */
inline LauncherResult<_bstr_t> UrlmonValidateUri(const wchar_t* aUri) {
  LauncherResult<UniqueAbsolutePidl> pidlResult = ShellParseDisplayName(aUri);
  if (pidlResult.isErr()) {
    return LAUNCHER_ERROR_FROM_RESULT(pidlResult);
  }

  // The value of |flags| is the same value as used in ieframe!_EnsureIUri in
  // Win7, which is called behind SHParseDisplayName.  In Win10, on the other
  // hand, an flag 0x03000000 is also passed to CreateUri, but we don't
  // specify it because it's undocumented and unknown.
  constexpr DWORD flags =
      Uri_CREATE_NO_DECODE_EXTRA_INFO | Uri_CREATE_CANONICALIZE |
      Uri_CREATE_CRACK_UNKNOWN_SCHEMES | Uri_CREATE_PRE_PROCESS_HTML_URI |
      Uri_CREATE_IE_SETTINGS;
  RefPtr<IUri> uri;
  HRESULT hr = CreateUri(aUri, flags, 0, getter_AddRefs(uri));
  if (FAILED(hr)) {
    return LAUNCHER_ERROR_FROM_HRESULT(hr);
  }

  _bstr_t bstrUri;

  hr = uri->GetAbsoluteUri(bstrUri.GetAddress());
  if (FAILED(hr)) {
    return LAUNCHER_ERROR_FROM_HRESULT(hr);
  }

  if (hr == S_FALSE) {
    hr = uri->GetRawUri(bstrUri.GetAddress());
    if (FAILED(hr)) {
      return LAUNCHER_ERROR_FROM_HRESULT(hr);
    }
  }

  return bstrUri;
}

}  // namespace mozilla

#endif  // mozilla_UrlmonHeaderOnlyUtils_h

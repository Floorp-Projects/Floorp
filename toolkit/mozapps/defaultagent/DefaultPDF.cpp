/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DefaultPDF.h"

#include <string>

#include <shlobj.h>
#include <winerror.h>

#include "EventLog.h"
#include "UtfConvert.h"

#include "mozilla/Buffer.h"
#include "mozilla/RefPtr.h"
#include "mozilla/WinHeaderOnlyUtils.h"

using PdfResult = mozilla::WindowsErrorResult<std::string>;

static PdfResult GetDefaultPdf() {
  RefPtr<IApplicationAssociationRegistration> pAAR;
  HRESULT hr = CoCreateInstance(
      CLSID_ApplicationAssociationRegistration, nullptr, CLSCTX_INPROC,
      IID_IApplicationAssociationRegistration, getter_AddRefs(pAAR));
  if (FAILED(hr)) {
    LOG_ERROR(hr);
    return PdfResult(mozilla::WindowsError::FromHResult(hr));
  }

  mozilla::UniquePtr<wchar_t, mozilla::CoTaskMemFreeDeleter> registeredApp;
  {
    wchar_t* rawRegisteredApp;
    hr = pAAR->QueryCurrentDefault(L".pdf", AT_FILEEXTENSION, AL_EFFECTIVE,
                                   &rawRegisteredApp);
    if (FAILED(hr)) {
      LOG_ERROR(hr);
      return PdfResult(mozilla::WindowsError::FromHResult(hr));
    }
    registeredApp = mozilla::UniquePtr<wchar_t, mozilla::CoTaskMemFreeDeleter>{
        rawRegisteredApp};
  }

  // Get the application Friendly Name associated to the found ProgID. This is
  // sized to be larger than any observed or expected friendly names. Long
  // friendly names tend to be in the form `[Company] [Viewer] [Variant]`
  DWORD friendlyNameLen = 0;
  hr = AssocQueryStringW(ASSOCF_NONE, ASSOCSTR_FRIENDLYAPPNAME,
                         registeredApp.get(), nullptr, nullptr,
                         &friendlyNameLen);
  if (FAILED(hr)) {
    LOG_ERROR(hr);
    return PdfResult(mozilla::WindowsError::FromHResult(hr));
  }

  mozilla::Buffer<wchar_t> friendlyNameBuffer(friendlyNameLen);
  hr = AssocQueryStringW(ASSOCF_NONE, ASSOCSTR_FRIENDLYAPPNAME,
                         registeredApp.get(), nullptr,
                         friendlyNameBuffer.Elements(), &friendlyNameLen);
  if (FAILED(hr)) {
    LOG_ERROR(hr);
    return PdfResult(mozilla::WindowsError::FromHResult(hr));
  }

  return Utf16ToUtf8(friendlyNameBuffer.Elements());
}

DefaultPdfResult GetDefaultPdfInfo() {
  DefaultPdfInfo pdfInfo;
  MOZ_TRY_VAR(pdfInfo.currentDefaultPdf, GetDefaultPdf());

  return pdfInfo;
}

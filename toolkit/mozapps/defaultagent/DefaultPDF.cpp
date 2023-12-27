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

#include "mozilla/Buffer.h"
#include "mozilla/RefPtr.h"
#include "mozilla/WinHeaderOnlyUtils.h"
#include "mozilla/Try.h"

namespace mozilla::default_agent {

constexpr std::string_view kUnknownPdfString = "";

constexpr std::pair<std::string_view, PDFHandler> kStringPdfHandlerMap[]{
    {"error", PDFHandler::Error},
    {kUnknownPdfString, PDFHandler::Unknown},
    {"Firefox", PDFHandler::Firefox},
    {"Microsoft Edge", PDFHandler::MicrosoftEdge},
    {"Google Chrome", PDFHandler::GoogleChrome},
    {"Adobe Acrobat", PDFHandler::AdobeAcrobat},
    {"WPS", PDFHandler::WPS},
    {"Nitro", PDFHandler::Nitro},
    {"Foxit", PDFHandler::Foxit},
    {"PDF-XChange", PDFHandler::PDFXChange},
    {"Avast", PDFHandler::AvastSecureBrowser},
    {"Sumatra", PDFHandler::SumatraPDF},
};

static_assert(mozilla::ArrayLength(kStringPdfHandlerMap) == kPDFHandlerCount);

std::string GetStringForPDFHandler(PDFHandler handler) {
  for (const auto& [mapString, mapPdf] : kStringPdfHandlerMap) {
    if (handler == mapPdf) {
      return std::string{mapString};
    }
  }

  return std::string(kUnknownPdfString);
}

PDFHandler GetPDFHandlerFromString(const std::string& pdfHandlerString) {
  for (const auto& [mapString, mapPdfHandler] : kStringPdfHandlerMap) {
    if (pdfHandlerString == mapString) {
      return mapPdfHandler;
    }
  }

  return PDFHandler::Unknown;
}

using PdfResult = mozilla::WindowsErrorResult<PDFHandler>;

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

  constexpr std::pair<std::wstring_view, PDFHandler> kFriendlyNamePrefixes[] = {
      {L"Firefox", PDFHandler::Firefox},
      {L"Microsoft Edge", PDFHandler::MicrosoftEdge},
      {L"Google Chrome", PDFHandler::GoogleChrome},
      {L"Adobe", PDFHandler::AdobeAcrobat},
      {L"Acrobat", PDFHandler::AdobeAcrobat},
      {L"WPS", PDFHandler::WPS},
      {L"Nitro", PDFHandler::Nitro},
      {L"Foxit", PDFHandler::Foxit},
      {L"PDF-XChange", PDFHandler::PDFXChange},
      {L"Avast", PDFHandler::AvastSecureBrowser},
      {L"Sumatra", PDFHandler::SumatraPDF},
  };

  // We should have one prefix for every PDF handler we track, with exceptions
  // listed below.
  // Error - removed; not a real pdf handler.
  // Unknown - removed; not a real pdf handler.
  // AdobeAcrobat - duplicate; `Adobe` and `Acrobat` prefixes are both seen in
  // telemetry.
  static_assert(mozilla::ArrayLength(kFriendlyNamePrefixes) ==
                kPDFHandlerCount - 2 + 1);

  PDFHandler resolvedHandler = PDFHandler::Unknown;
  for (const auto& [knownHandlerSubstring, handlerEnum] :
       kFriendlyNamePrefixes) {
    if (!wcsnicmp(friendlyNameBuffer.Elements(), knownHandlerSubstring.data(),
                  knownHandlerSubstring.length())) {
      resolvedHandler = handlerEnum;
      break;
    }
  }

  return resolvedHandler;
}

DefaultPdfResult GetDefaultPdfInfo() {
  DefaultPdfInfo pdfInfo;
  MOZ_TRY_VAR(pdfInfo.currentDefaultPdf, GetDefaultPdf());

  return pdfInfo;
}

}  // namespace mozilla::default_agent

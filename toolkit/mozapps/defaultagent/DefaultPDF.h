/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DEFAULT_BROWSER_DEFAULT_PDF_H__
#define DEFAULT_BROWSER_DEFAULT_PDF_H__

#include <string>

#include "mozilla/DefineEnum.h"
#include "mozilla/WinHeaderOnlyUtils.h"

namespace mozilla::default_agent {

MOZ_DEFINE_ENUM_CLASS(PDFHandler,
                      (Error, Unknown, Firefox, MicrosoftEdge, GoogleChrome,
                       AdobeAcrobat, WPS, Nitro, Foxit, PDFXChange,
                       AvastSecureBrowser, SumatraPDF));

struct DefaultPdfInfo {
  PDFHandler currentDefaultPdf;
};

using DefaultPdfResult = mozilla::WindowsErrorResult<DefaultPdfInfo>;

DefaultPdfResult GetDefaultPdfInfo();
std::string GetStringForPDFHandler(PDFHandler handler);

}  // namespace mozilla::default_agent

#endif  // DEFAULT_BROWSER_DEFAULT_PDF_H__

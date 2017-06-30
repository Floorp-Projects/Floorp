// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef PUBLIC_FPDF_PPO_H_
#define PUBLIC_FPDF_PPO_H_

// NOLINTNEXTLINE(build/include)
#include "fpdfview.h"

#ifdef __cplusplus
extern "C" {
#endif

// Import pages to a FPDF_DOCUMENT.
//
//   dest_doc  - The destination document for the pages.
//   src_doc   - The document to be imported.
//   pagerange - A page range string, Such as "1,3,5-7". If |pagerange| is NULL,
//               all pages from |src_doc| are imported.
//   index     - The page index to insert at.
//
// Returns TRUE on success.
DLLEXPORT FPDF_BOOL STDCALL FPDF_ImportPages(FPDF_DOCUMENT dest_doc,
                                             FPDF_DOCUMENT src_doc,
                                             FPDF_BYTESTRING pagerange,
                                             int index);

// Copy the viewer preferences from |src_doc| into |dest_doc|.
//
//   dest_doc - Document to write the viewer preferences into.
//   src_doc  - Document to read the viewer preferences from.
//
// Returns TRUE on success.
DLLEXPORT FPDF_BOOL STDCALL FPDF_CopyViewerPreferences(FPDF_DOCUMENT dest_doc,
                                                       FPDF_DOCUMENT src_doc);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif  // PUBLIC_FPDF_PPO_H_

// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef PUBLIC_FPDF_SAVE_H_
#define PUBLIC_FPDF_SAVE_H_

// NOLINTNEXTLINE(build/include)
#include "fpdfview.h"

#ifdef __cplusplus
extern "C" {
#endif

// Structure for custom file write
typedef struct FPDF_FILEWRITE_ {
  //
  // Version number of the interface. Currently must be 1.
  //
  int version;

  //
  // Method: WriteBlock
  //          Output a block of data in your custom way.
  // Interface Version:
  //          1
  // Implementation Required:
  //          Yes
  // Comments:
  //          Called by function FPDF_SaveDocument
  // Parameters:
  //          pThis       -   Pointer to the structure itself
  //          pData       -   Pointer to a buffer to output
  //          size        -   The size of the buffer.
  // Return value:
  //          Should be non-zero if successful, zero for error.
  //
  int (*WriteBlock)(struct FPDF_FILEWRITE_* pThis,
                    const void* pData,
                    unsigned long size);
} FPDF_FILEWRITE;

/** @brief Incremental. */
#define FPDF_INCREMENTAL 1
/** @brief No Incremental. */
#define FPDF_NO_INCREMENTAL 2
/** @brief Remove security. */
#define FPDF_REMOVE_SECURITY 3

// Function: FPDF_SaveAsCopy
//          Saves the copy of specified document in custom way.
// Parameters:
//          document        -   Handle to document. Returned by
//          FPDF_LoadDocument and FPDF_CreateNewDocument.
//          pFileWrite      -   A pointer to a custom file write structure.
//          flags           -   The creating flags.
// Return value:
//          TRUE for succeed, FALSE for failed.
//
DLLEXPORT FPDF_BOOL STDCALL FPDF_SaveAsCopy(FPDF_DOCUMENT document,
                                            FPDF_FILEWRITE* pFileWrite,
                                            FPDF_DWORD flags);

// Function: FPDF_SaveWithVersion
//          Same as function ::FPDF_SaveAsCopy, except the file version of the
//          saved document could be specified by user.
// Parameters:
//          document        -   Handle to document.
//          pFileWrite      -   A pointer to a custom file write structure.
//          flags           -   The creating flags.
//          fileVersion     -   The PDF file version. File version: 14 for 1.4,
//          15 for 1.5, ...
// Return value:
//          TRUE if succeed, FALSE if failed.
//
DLLEXPORT FPDF_BOOL STDCALL FPDF_SaveWithVersion(FPDF_DOCUMENT document,
                                                 FPDF_FILEWRITE* pFileWrite,
                                                 FPDF_DWORD flags,
                                                 int fileVersion);

#ifdef __cplusplus
}
#endif

#endif  // PUBLIC_FPDF_SAVE_H_

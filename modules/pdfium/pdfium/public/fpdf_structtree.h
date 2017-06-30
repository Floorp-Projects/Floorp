// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef PUBLIC_FPDF_STRUCTTREE_H_
#define PUBLIC_FPDF_STRUCTTREE_H_

// NOLINTNEXTLINE(build/include)
#include "fpdfview.h"

#ifdef __cplusplus
extern "C" {
#endif

// Function: FPDF_StructTree_GetForPage
//          Get the structure tree for a page.
// Parameters:
//          page        -   Handle to the page. Returned by FPDF_LoadPage
//          function.
// Return value:
//          A handle to the structure tree or NULL on error.
DLLEXPORT FPDF_STRUCTTREE STDCALL FPDF_StructTree_GetForPage(FPDF_PAGE page);

// Function: FPDF_StructTree_Close
//          Release the resource allocate by FPDF_StructTree_GetForPage.
// Parameters:
//          struct_tree -   Handle to the struct tree. Returned by
//          FPDF_StructTree_LoadPage function.
// Return value:
//          NULL
DLLEXPORT void STDCALL FPDF_StructTree_Close(FPDF_STRUCTTREE struct_tree);

// Function: FPDF_StructTree_CountChildren
//          Count the number of children for the structure tree.
// Parameters:
//          struct_tree -   Handle to the struct tree. Returned by
//          FPDF_StructTree_LoadPage function.
// Return value:
//          The number of children, or -1 on error.
DLLEXPORT int STDCALL
FPDF_StructTree_CountChildren(FPDF_STRUCTTREE struct_tree);

// Function: FPDF_StructTree_GetChildAtIndex
//          Get a child in the structure tree.
// Parameters:
//          struct_tree -   Handle to the struct tree. Returned by
//          FPDF_StructTree_LoadPage function.
//          index       -   The index for the child, 0-based.
// Return value:
//          The child at the n-th index or NULL on error.
DLLEXPORT FPDF_STRUCTELEMENT STDCALL
FPDF_StructTree_GetChildAtIndex(FPDF_STRUCTTREE struct_tree, int index);

// Function: FPDF_StructElement_GetAltText
//          Get the alt text for a given element.
// Parameters:
//          struct_element -   Handle to the struct element.
//          buffer         -   A buffer for output the alt text. May be NULL.
//          buflen         -   The length of the buffer, in bytes. May be 0.
// Return value:
//          The number of bytes in the title, including the terminating NUL
//          character. The number of bytes is returned regardless of the
//          |buffer| and |buflen| parameters.
// Comments:
//          Regardless of the platform, the |buffer| is always in UTF-16LE
//          encoding. The string is terminated by a UTF16 NUL character. If
//          |buflen| is less than the required length, or |buffer| is NULL,
//          |buffer| will not be modified.
DLLEXPORT unsigned long STDCALL
FPDF_StructElement_GetAltText(FPDF_STRUCTELEMENT struct_element,
                              void* buffer,
                              unsigned long buflen);

// Function: FPDF_StructElement_CountChildren
//          Count the number of children for the structure element.
// Parameters:
//          struct_element -   Handle to the struct element.
// Return value:
//          The number of children, or -1 on error.
DLLEXPORT int STDCALL
FPDF_StructElement_CountChildren(FPDF_STRUCTELEMENT struct_element);

// Function: FPDF_StructElement_GetChildAtIndex
//          Get a child in the structure element.
// Parameters:
//          struct_tree -   Handle to the struct element.
//          index       -   The index for the child, 0-based.
// Return value:
//          The child at the n-th index or NULL on error.
// Comments:
//          If the child exists but is not an element, then this function will
//          return NULL. This will also return NULL for out of bounds indices.
DLLEXPORT FPDF_STRUCTELEMENT STDCALL
FPDF_StructElement_GetChildAtIndex(FPDF_STRUCTELEMENT struct_element,
                                   int index);

#ifdef __cplusplus
}
#endif

#endif  // PUBLIC_FPDF_STRUCTTREE_H_

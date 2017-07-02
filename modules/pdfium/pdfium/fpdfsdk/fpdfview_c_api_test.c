// Copyright 2015 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This "C" (not "C++") file ensures that the public headers compile
// and link for "C" (and not just "C++").

#include <stdio.h>

#include "fpdfsdk/fpdfview_c_api_test.h"

#include "public/fpdf_dataavail.h"
#include "public/fpdf_doc.h"
#include "public/fpdf_edit.h"
#include "public/fpdf_ext.h"
#include "public/fpdf_flatten.h"
#include "public/fpdf_formfill.h"
#include "public/fpdf_fwlevent.h"
#include "public/fpdf_ppo.h"
#include "public/fpdf_progressive.h"
#include "public/fpdf_save.h"
#include "public/fpdf_searchex.h"
#include "public/fpdf_structtree.h"
#include "public/fpdf_sysfontinfo.h"
#include "public/fpdf_text.h"
#include "public/fpdf_transformpage.h"
#include "public/fpdfview.h"

// Scheme for avoiding LTO out of existence, warnings, etc.
typedef void (*fnptr)(void);  // Legal generic function type for casts.
fnptr g_c_api_test_fnptr = NULL;  // Extern, so can't know it doesn't change.
#define CHK(x) if ((fnptr)(x) == g_c_api_test_fnptr) return 0

// Function to call from gtest harness to ensure linker resolution.
int CheckPDFiumCApi() {
    // fpdf_dataavail.h
    CHK(FPDFAvail_Create);
    CHK(FPDFAvail_Destroy);
    CHK(FPDFAvail_IsDocAvail);
    CHK(FPDFAvail_GetDocument);
    CHK(FPDFAvail_GetFirstPageNum);
    CHK(FPDFAvail_IsPageAvail);
    CHK(FPDFAvail_IsFormAvail);
    CHK(FPDFAvail_IsLinearized);

    // fpdf_doc.h
    CHK(FPDFBookmark_GetFirstChild);
    CHK(FPDFBookmark_GetNextSibling);
    CHK(FPDFBookmark_GetTitle);
    CHK(FPDFBookmark_Find);
    CHK(FPDFBookmark_GetDest);
    CHK(FPDFBookmark_GetAction);
    CHK(FPDFAction_GetType);
    CHK(FPDFAction_GetDest);
    CHK(FPDFAction_GetFilePath);
    CHK(FPDFAction_GetURIPath);
    CHK(FPDFDest_GetPageIndex);
    CHK(FPDFDest_GetLocationInPage);
    CHK(FPDFLink_GetLinkAtPoint);
    CHK(FPDFLink_GetLinkZOrderAtPoint);
    CHK(FPDFLink_GetDest);
    CHK(FPDFLink_GetAction);
    CHK(FPDFLink_Enumerate);
    CHK(FPDFLink_GetAnnotRect);
    CHK(FPDFLink_CountQuadPoints);
    CHK(FPDFLink_GetQuadPoints);
    CHK(FPDF_GetMetaText);
    CHK(FPDF_GetPageLabel);

    // fpdf_edit.h
    CHK(FPDF_CreateNewDocument);
    CHK(FPDFPage_New);
    CHK(FPDFPage_Delete);
    CHK(FPDFPage_GetRotation);
    CHK(FPDFPage_SetRotation);
    CHK(FPDFPage_InsertObject);
    CHK(FPDFPage_CountObject);
    CHK(FPDFPage_GetObject);
    CHK(FPDFPage_HasTransparency);
    CHK(FPDFPage_GenerateContent);
    CHK(FPDFPageObj_HasTransparency);
    CHK(FPDFPageObj_Transform);
    CHK(FPDFPage_TransformAnnots);
    CHK(FPDFPageObj_NewImgeObj);
    CHK(FPDFImageObj_LoadJpegFile);
    CHK(FPDFImageObj_LoadJpegFileInline);
    CHK(FPDFImageObj_SetMatrix);
    CHK(FPDFImageObj_SetBitmap);
    CHK(FPDFPageObj_CreateNewPath);
    CHK(FPDFPageObj_CreateNewRect);
    CHK(FPDFPath_SetStrokeColor);
    CHK(FPDFPath_SetStrokeWidth);
    CHK(FPDFPath_SetFillColor);
    CHK(FPDFPath_MoveTo);
    CHK(FPDFPath_LineTo);
    CHK(FPDFPath_BezierTo);
    CHK(FPDFPath_Close);
    CHK(FPDFPath_SetDrawMode);
    CHK(FPDFPageObj_NewTextObj);
    CHK(FPDFText_SetText);
    CHK(FPDFText_LoadType1Font);

    // fpdf_ext.h
    CHK(FSDK_SetUnSpObjProcessHandler);
    CHK(FPDFDoc_GetPageMode);

    // fpdf_flatten.h
    CHK(FPDFPage_Flatten);

    // fpdf_fwlevent.h - no exports.

    // fpdf_formfill.h
    CHK(FPDFDOC_InitFormFillEnvironment);
    CHK(FPDFDOC_ExitFormFillEnvironment);
    CHK(FORM_OnAfterLoadPage);
    CHK(FORM_OnBeforeClosePage);
    CHK(FORM_DoDocumentJSAction);
    CHK(FORM_DoDocumentOpenAction);
    CHK(FORM_DoDocumentAAction);
    CHK(FORM_DoPageAAction);
    CHK(FORM_OnMouseMove);
    CHK(FORM_OnLButtonDown);
    CHK(FORM_OnLButtonUp);
#ifdef PDF_ENABLE_XFA
    CHK(FORM_OnRButtonDown);
    CHK(FORM_OnRButtonUp);
#endif
    CHK(FORM_OnKeyDown);
    CHK(FORM_OnKeyUp);
    CHK(FORM_OnChar);
    CHK(FORM_ForceToKillFocus);
    CHK(FPDFPage_HasFormFieldAtPoint);
    CHK(FPDPage_HasFormFieldAtPoint);  // DEPRECATED. Remove in the future.
    CHK(FPDFPage_FormFieldZOrderAtPoint);
    CHK(FPDF_SetFormFieldHighlightColor);
    CHK(FPDF_SetFormFieldHighlightAlpha);
    CHK(FPDF_RemoveFormFieldHighlight);
    CHK(FPDF_FFLDraw);
#ifdef PDF_ENABLE_XFA
    CHK(FPDF_HasXFAField);
    CHK(FPDF_LoadXFA);
    CHK(FPDF_Widget_Undo);
    CHK(FPDF_Widget_Redo);
    CHK(FPDF_Widget_SelectAll);
    CHK(FPDF_Widget_Copy);
    CHK(FPDF_Widget_Cut);
    CHK(FPDF_Widget_Paste);
    CHK(FPDF_Widget_ReplaceSpellCheckWord);
    CHK(FPDF_Widget_GetSpellCheckWords);
    CHK(FPDF_StringHandleCounts);
    CHK(FPDF_StringHandleGetStringByIndex);
    CHK(FPDF_StringHandleRelease);
    CHK(FPDF_StringHandleAddString);
#endif

    // fpdf_ppo.h
    CHK(FPDF_ImportPages);
    CHK(FPDF_CopyViewerPreferences);

    // fpdf_progressive.h
    CHK(FPDF_RenderPageBitmap_Start);
    CHK(FPDF_RenderPage_Continue);
    CHK(FPDF_RenderPage_Close);

    // fpdf_save.h
    CHK(FPDF_SaveAsCopy);
    CHK(FPDF_SaveWithVersion);

    // fpdf_searchex.h
    CHK(FPDFText_GetCharIndexFromTextIndex);

    // fpdf_structtree.h
    CHK(FPDF_StructTree_GetForPage);
    CHK(FPDF_StructTree_Close);
    CHK(FPDF_StructTree_CountChildren);
    CHK(FPDF_StructTree_GetChildAtIndex);
    CHK(FPDF_StructElement_GetAltText);
    CHK(FPDF_StructElement_CountChildren);
    CHK(FPDF_StructElement_GetChildAtIndex);

    // fpdf_sysfontinfo.h
    CHK(FPDF_GetDefaultTTFMap);
    CHK(FPDF_AddInstalledFont);
    CHK(FPDF_SetSystemFontInfo);
    CHK(FPDF_GetDefaultSystemFontInfo);
    CHK(FPDF_FreeDefaultSystemFontInfo);

    // fpdf_text.h
    CHK(FPDFText_LoadPage);
    CHK(FPDFText_ClosePage);
    CHK(FPDFText_CountChars);
    CHK(FPDFText_GetUnicode);
    CHK(FPDFText_GetFontSize);
    CHK(FPDFText_GetCharBox);
    CHK(FPDFText_GetCharIndexAtPos);
    CHK(FPDFText_GetText);
    CHK(FPDFText_CountRects);
    CHK(FPDFText_GetRect);
    CHK(FPDFText_GetBoundedText);
    CHK(FPDFText_FindStart);
    CHK(FPDFText_FindNext);
    CHK(FPDFText_FindPrev);
    CHK(FPDFText_GetSchResultIndex);
    CHK(FPDFText_GetSchCount);
    CHK(FPDFText_FindClose);
    CHK(FPDFLink_LoadWebLinks);
    CHK(FPDFLink_CountWebLinks);
    CHK(FPDFLink_GetURL);
    CHK(FPDFLink_CountRects);
    CHK(FPDFLink_GetRect);
    CHK(FPDFLink_CloseWebLinks);

    // fpdf_transformpage.h
    CHK(FPDFPage_SetMediaBox);
    CHK(FPDFPage_SetCropBox);
    CHK(FPDFPage_GetMediaBox);
    CHK(FPDFPage_GetCropBox);
    CHK(FPDFPage_TransFormWithClip);
    CHK(FPDFPageObj_TransformClipPath);
    CHK(FPDF_CreateClipPath);
    CHK(FPDF_DestroyClipPath);
    CHK(FPDFPage_InsertClipPath);

    // fpdfview.h
    CHK(FPDF_InitLibrary);
    CHK(FPDF_InitLibraryWithConfig);
    CHK(FPDF_DestroyLibrary);
    CHK(FPDF_SetSandBoxPolicy);
    CHK(FPDF_LoadDocument);
    CHK(FPDF_LoadMemDocument);
    CHK(FPDF_LoadCustomDocument);
    CHK(FPDF_GetFileVersion);
    CHK(FPDF_GetLastError);
    CHK(FPDF_GetDocPermissions);
    CHK(FPDF_GetSecurityHandlerRevision);
    CHK(FPDF_GetPageCount);
    CHK(FPDF_LoadPage);
    CHK(FPDF_GetPageWidth);
    CHK(FPDF_GetPageHeight);
    CHK(FPDF_GetPageSizeByIndex);
    CHK(FPDF_RenderPageBitmap);
    CHK(FPDF_RenderPageBitmapWithMatrix);
    CHK(FPDF_ClosePage);
    CHK(FPDF_CloseDocument);
    CHK(FPDF_DeviceToPage);
    CHK(FPDF_PageToDevice);
    CHK(FPDFBitmap_Create);
    CHK(FPDFBitmap_CreateEx);
    CHK(FPDFBitmap_FillRect);
    CHK(FPDFBitmap_GetBuffer);
    CHK(FPDFBitmap_GetWidth);
    CHK(FPDFBitmap_GetHeight);
    CHK(FPDFBitmap_GetStride);
    CHK(FPDFBitmap_Destroy);
    CHK(FPDF_VIEWERREF_GetPrintScaling);
    CHK(FPDF_VIEWERREF_GetNumCopies);
    CHK(FPDF_VIEWERREF_GetPrintPageRange);
    CHK(FPDF_VIEWERREF_GetDuplex);
    CHK(FPDF_VIEWERREF_GetName);
    CHK(FPDF_CountNamedDests);
    CHK(FPDF_GetNamedDestByName);
    CHK(FPDF_GetNamedDest);
#ifdef PDF_ENABLE_XFA
    CHK(FPDF_BStr_Init);
    CHK(FPDF_BStr_Set);
    CHK(FPDF_BStr_Clear);
#endif

    return 1;
}

#undef CHK

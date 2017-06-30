// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "public/fpdf_text.h"

#include <algorithm>
#include <vector>

#include "core/fpdfapi/page/cpdf_page.h"
#include "core/fpdfdoc/cpdf_viewerpreferences.h"
#include "core/fpdftext/cpdf_linkextract.h"
#include "core/fpdftext/cpdf_textpage.h"
#include "core/fpdftext/cpdf_textpagefind.h"
#include "fpdfsdk/fsdk_define.h"
#include "third_party/base/numerics/safe_conversions.h"
#include "third_party/base/stl_util.h"

#ifdef PDF_ENABLE_XFA
#include "fpdfsdk/fpdfxfa/cpdfxfa_context.h"
#include "fpdfsdk/fpdfxfa/cpdfxfa_page.h"
#endif  // PDF_ENABLE_XFA

#ifdef _WIN32
#include <tchar.h>
#endif

namespace {

CPDF_TextPage* CPDFTextPageFromFPDFTextPage(FPDF_TEXTPAGE text_page) {
  return static_cast<CPDF_TextPage*>(text_page);
}

CPDF_TextPageFind* CPDFTextPageFindFromFPDFSchHandle(FPDF_SCHHANDLE handle) {
  return static_cast<CPDF_TextPageFind*>(handle);
}

CPDF_LinkExtract* CPDFLinkExtractFromFPDFPageLink(FPDF_PAGELINK link) {
  return static_cast<CPDF_LinkExtract*>(link);
}

}  // namespace

DLLEXPORT FPDF_TEXTPAGE STDCALL FPDFText_LoadPage(FPDF_PAGE page) {
  CPDF_Page* pPDFPage = CPDFPageFromFPDFPage(page);
  if (!pPDFPage)
    return nullptr;

#ifdef PDF_ENABLE_XFA
  CPDFXFA_Page* pPage = (CPDFXFA_Page*)page;
  CPDFXFA_Context* pContext = pPage->GetContext();
  CPDF_ViewerPreferences viewRef(pContext->GetPDFDoc());
#else  // PDF_ENABLE_XFA
  CPDF_ViewerPreferences viewRef(pPDFPage->m_pDocument);
#endif  // PDF_ENABLE_XFA

  CPDF_TextPage* textpage = new CPDF_TextPage(
      pPDFPage, viewRef.IsDirectionR2L() ? FPDFText_Direction::Right
                                         : FPDFText_Direction::Left);
  textpage->ParseTextPage();
  return textpage;
}

DLLEXPORT void STDCALL FPDFText_ClosePage(FPDF_TEXTPAGE text_page) {
  delete CPDFTextPageFromFPDFTextPage(text_page);
}

DLLEXPORT int STDCALL FPDFText_CountChars(FPDF_TEXTPAGE text_page) {
  if (!text_page)
    return -1;

  CPDF_TextPage* textpage = CPDFTextPageFromFPDFTextPage(text_page);
  return textpage->CountChars();
}

DLLEXPORT unsigned int STDCALL FPDFText_GetUnicode(FPDF_TEXTPAGE text_page,
                                                   int index) {
  if (!text_page)
    return 0;

  CPDF_TextPage* textpage = CPDFTextPageFromFPDFTextPage(text_page);
  if (index < 0 || index >= textpage->CountChars())
    return 0;

  FPDF_CHAR_INFO charinfo;
  textpage->GetCharInfo(index, &charinfo);
  return charinfo.m_Unicode;
}

DLLEXPORT double STDCALL FPDFText_GetFontSize(FPDF_TEXTPAGE text_page,
                                              int index) {
  if (!text_page)
    return 0;
  CPDF_TextPage* textpage = CPDFTextPageFromFPDFTextPage(text_page);

  if (index < 0 || index >= textpage->CountChars())
    return 0;

  FPDF_CHAR_INFO charinfo;
  textpage->GetCharInfo(index, &charinfo);
  return charinfo.m_FontSize;
}

DLLEXPORT void STDCALL FPDFText_GetCharBox(FPDF_TEXTPAGE text_page,
                                           int index,
                                           double* left,
                                           double* right,
                                           double* bottom,
                                           double* top) {
  if (!text_page)
    return;
  CPDF_TextPage* textpage = CPDFTextPageFromFPDFTextPage(text_page);

  if (index < 0 || index >= textpage->CountChars())
    return;
  FPDF_CHAR_INFO charinfo;
  textpage->GetCharInfo(index, &charinfo);
  *left = charinfo.m_CharBox.left;
  *right = charinfo.m_CharBox.right;
  *bottom = charinfo.m_CharBox.bottom;
  *top = charinfo.m_CharBox.top;
}

// select
DLLEXPORT int STDCALL FPDFText_GetCharIndexAtPos(FPDF_TEXTPAGE text_page,
                                                 double x,
                                                 double y,
                                                 double xTolerance,
                                                 double yTolerance) {
  if (!text_page)
    return -3;

  CPDF_TextPage* textpage = CPDFTextPageFromFPDFTextPage(text_page);
  return textpage->GetIndexAtPos(
      CFX_PointF(static_cast<FX_FLOAT>(x), static_cast<FX_FLOAT>(y)),
      CFX_SizeF(static_cast<FX_FLOAT>(xTolerance),
                static_cast<FX_FLOAT>(yTolerance)));
}

DLLEXPORT int STDCALL FPDFText_GetText(FPDF_TEXTPAGE text_page,
                                       int start,
                                       int count,
                                       unsigned short* result) {
  if (!text_page)
    return 0;

  CPDF_TextPage* textpage = CPDFTextPageFromFPDFTextPage(text_page);
  if (start >= textpage->CountChars())
    return 0;

  CFX_WideString str = textpage->GetPageText(start, count);
  if (str.GetLength() > count)
    str = str.Left(count);

  CFX_ByteString cbUTF16str = str.UTF16LE_Encode();
  FXSYS_memcpy(result, cbUTF16str.GetBuffer(cbUTF16str.GetLength()),
               cbUTF16str.GetLength());
  cbUTF16str.ReleaseBuffer(cbUTF16str.GetLength());

  return cbUTF16str.GetLength() / sizeof(unsigned short);
}

DLLEXPORT int STDCALL FPDFText_CountRects(FPDF_TEXTPAGE text_page,
                                          int start,
                                          int count) {
  if (!text_page)
    return 0;

  CPDF_TextPage* textpage = CPDFTextPageFromFPDFTextPage(text_page);
  return textpage->CountRects(start, count);
}

DLLEXPORT void STDCALL FPDFText_GetRect(FPDF_TEXTPAGE text_page,
                                        int rect_index,
                                        double* left,
                                        double* top,
                                        double* right,
                                        double* bottom) {
  if (!text_page)
    return;

  CPDF_TextPage* textpage = CPDFTextPageFromFPDFTextPage(text_page);
  CFX_FloatRect rect;
  textpage->GetRect(rect_index, rect.left, rect.top, rect.right, rect.bottom);
  *left = rect.left;
  *top = rect.top;
  *right = rect.right;
  *bottom = rect.bottom;
}

DLLEXPORT int STDCALL FPDFText_GetBoundedText(FPDF_TEXTPAGE text_page,
                                              double left,
                                              double top,
                                              double right,
                                              double bottom,
                                              unsigned short* buffer,
                                              int buflen) {
  if (!text_page)
    return 0;

  CPDF_TextPage* textpage = CPDFTextPageFromFPDFTextPage(text_page);
  CFX_FloatRect rect((FX_FLOAT)left, (FX_FLOAT)bottom, (FX_FLOAT)right,
                     (FX_FLOAT)top);
  CFX_WideString str = textpage->GetTextByRect(rect);

  if (buflen <= 0 || !buffer)
    return str.GetLength();

  CFX_ByteString cbUTF16Str = str.UTF16LE_Encode();
  int len = cbUTF16Str.GetLength() / sizeof(unsigned short);
  int size = buflen > len ? len : buflen;
  FXSYS_memcpy(buffer, cbUTF16Str.GetBuffer(size * sizeof(unsigned short)),
               size * sizeof(unsigned short));
  cbUTF16Str.ReleaseBuffer(size * sizeof(unsigned short));

  return size;
}

// Search
// -1 for end
DLLEXPORT FPDF_SCHHANDLE STDCALL FPDFText_FindStart(FPDF_TEXTPAGE text_page,
                                                    FPDF_WIDESTRING findwhat,
                                                    unsigned long flags,
                                                    int start_index) {
  if (!text_page)
    return nullptr;

  CPDF_TextPageFind* textpageFind =
      new CPDF_TextPageFind(CPDFTextPageFromFPDFTextPage(text_page));
  FX_STRSIZE len = CFX_WideString::WStringLength(findwhat);
  textpageFind->FindFirst(CFX_WideString::FromUTF16LE(findwhat, len), flags,
                          start_index);
  return textpageFind;
}

DLLEXPORT FPDF_BOOL STDCALL FPDFText_FindNext(FPDF_SCHHANDLE handle) {
  if (!handle)
    return false;

  CPDF_TextPageFind* textpageFind = CPDFTextPageFindFromFPDFSchHandle(handle);
  return textpageFind->FindNext();
}

DLLEXPORT FPDF_BOOL STDCALL FPDFText_FindPrev(FPDF_SCHHANDLE handle) {
  if (!handle)
    return false;

  CPDF_TextPageFind* textpageFind = CPDFTextPageFindFromFPDFSchHandle(handle);
  return textpageFind->FindPrev();
}

DLLEXPORT int STDCALL FPDFText_GetSchResultIndex(FPDF_SCHHANDLE handle) {
  if (!handle)
    return 0;

  CPDF_TextPageFind* textpageFind = CPDFTextPageFindFromFPDFSchHandle(handle);
  return textpageFind->GetCurOrder();
}

DLLEXPORT int STDCALL FPDFText_GetSchCount(FPDF_SCHHANDLE handle) {
  if (!handle)
    return 0;

  CPDF_TextPageFind* textpageFind = CPDFTextPageFindFromFPDFSchHandle(handle);
  return textpageFind->GetMatchedCount();
}

DLLEXPORT void STDCALL FPDFText_FindClose(FPDF_SCHHANDLE handle) {
  if (!handle)
    return;

  CPDF_TextPageFind* textpageFind = CPDFTextPageFindFromFPDFSchHandle(handle);
  delete textpageFind;
  handle = nullptr;
}

// web link
DLLEXPORT FPDF_PAGELINK STDCALL FPDFLink_LoadWebLinks(FPDF_TEXTPAGE text_page) {
  if (!text_page)
    return nullptr;

  CPDF_LinkExtract* pageLink =
      new CPDF_LinkExtract(CPDFTextPageFromFPDFTextPage(text_page));
  pageLink->ExtractLinks();
  return pageLink;
}

DLLEXPORT int STDCALL FPDFLink_CountWebLinks(FPDF_PAGELINK link_page) {
  if (!link_page)
    return 0;

  CPDF_LinkExtract* pageLink = CPDFLinkExtractFromFPDFPageLink(link_page);
  return pdfium::base::checked_cast<int>(pageLink->CountLinks());
}

DLLEXPORT int STDCALL FPDFLink_GetURL(FPDF_PAGELINK link_page,
                                      int link_index,
                                      unsigned short* buffer,
                                      int buflen) {
  CFX_WideString wsUrl(L"");
  if (link_page && link_index >= 0) {
    CPDF_LinkExtract* pageLink = CPDFLinkExtractFromFPDFPageLink(link_page);
    wsUrl = pageLink->GetURL(link_index);
  }
  CFX_ByteString cbUTF16URL = wsUrl.UTF16LE_Encode();
  int required = cbUTF16URL.GetLength() / sizeof(unsigned short);
  if (!buffer || buflen <= 0)
    return required;

  int size = std::min(required, buflen);
  if (size > 0) {
    int buf_size = size * sizeof(unsigned short);
    FXSYS_memcpy(buffer, cbUTF16URL.GetBuffer(buf_size), buf_size);
  }
  return size;
}

DLLEXPORT int STDCALL FPDFLink_CountRects(FPDF_PAGELINK link_page,
                                          int link_index) {
  if (!link_page || link_index < 0)
    return 0;

  CPDF_LinkExtract* pageLink = CPDFLinkExtractFromFPDFPageLink(link_page);
  return pdfium::CollectionSize<int>(pageLink->GetRects(link_index));
}

DLLEXPORT void STDCALL FPDFLink_GetRect(FPDF_PAGELINK link_page,
                                        int link_index,
                                        int rect_index,
                                        double* left,
                                        double* top,
                                        double* right,
                                        double* bottom) {
  if (!link_page || link_index < 0 || rect_index < 0)
    return;

  CPDF_LinkExtract* pageLink = CPDFLinkExtractFromFPDFPageLink(link_page);
  std::vector<CFX_FloatRect> rectArray = pageLink->GetRects(link_index);
  if (rect_index >= pdfium::CollectionSize<int>(rectArray))
    return;

  *left = rectArray[rect_index].left;
  *right = rectArray[rect_index].right;
  *top = rectArray[rect_index].top;
  *bottom = rectArray[rect_index].bottom;
}

DLLEXPORT void STDCALL FPDFLink_CloseWebLinks(FPDF_PAGELINK link_page) {
  delete CPDFLinkExtractFromFPDFPageLink(link_page);
}

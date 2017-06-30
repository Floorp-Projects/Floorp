// Copyright 2017 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "core/fpdfapi/cpdf_modulemgr.h"
#include "core/fpdfapi/font/cpdf_font.h"
#include "core/fpdfapi/font/cpdf_type1font.h"
#include "core/fpdfapi/page/cpdf_textobject.h"
#include "core/fpdfapi/parser/cpdf_array.h"
#include "core/fpdfapi/parser/cpdf_dictionary.h"
#include "core/fpdfapi/parser/cpdf_document.h"
#include "core/fpdfapi/parser/cpdf_name.h"
#include "core/fpdfapi/parser/cpdf_number.h"
#include "core/fpdfapi/parser/cpdf_reference.h"
#include "core/fpdfapi/parser/cpdf_stream.h"
#include "core/fxge/cfx_fontmgr.h"
#include "core/fxge/fx_font.h"
#include "fpdfsdk/fsdk_define.h"
#include "public/fpdf_edit.h"

DLLEXPORT FPDF_PAGEOBJECT STDCALL FPDFPageObj_NewTextObj(FPDF_DOCUMENT document,
                                                         FPDF_BYTESTRING font,
                                                         float font_size) {
  CPDF_Document* pDoc = CPDFDocumentFromFPDFDocument(document);
  if (!pDoc)
    return nullptr;

  CPDF_Font* pFont = CPDF_Font::GetStockFont(pDoc, CFX_ByteStringC(font));
  if (!pFont)
    return nullptr;

  CPDF_TextObject* pTextObj = new CPDF_TextObject;
  pTextObj->m_TextState.SetFont(pFont);
  pTextObj->m_TextState.SetFontSize(font_size);
  pTextObj->DefaultStates();
  return pTextObj;
}

DLLEXPORT FPDF_BOOL STDCALL FPDFText_SetText(FPDF_PAGEOBJECT text_object,
                                             FPDF_BYTESTRING text) {
  if (!text_object)
    return false;

  auto pTextObj = reinterpret_cast<CPDF_TextObject*>(text_object);
  pTextObj->SetText(CFX_ByteString(text));
  return true;
}

DLLEXPORT FPDF_FONT STDCALL FPDFText_LoadType1Font(FPDF_DOCUMENT document,
                                                   const uint8_t* data,
                                                   uint32_t size) {
  CPDF_Document* pDoc = CPDFDocumentFromFPDFDocument(document);
  if (!pDoc || !data || size == 0)
    return nullptr;

  auto pFont = pdfium::MakeUnique<CFX_Font>();

  // TODO(npm): Maybe use FT_Get_X11_Font_Format to check format?
  if (!pFont->LoadEmbedded(data, size))
    return nullptr;

  CPDF_Dictionary* fontDict = pDoc->NewIndirect<CPDF_Dictionary>();
  fontDict->SetNewFor<CPDF_Name>("Type", "Font");
  fontDict->SetNewFor<CPDF_Name>("Subtype", "Type1");
  CFX_ByteString name = pFont->GetFaceName();
  if (name.IsEmpty())
    name = "Unnamed";
  fontDict->SetNewFor<CPDF_Name>("BaseFont", name);

  uint32_t glyphIndex;
  int currentChar = FXFT_Get_First_Char(pFont->GetFace(), &glyphIndex);
  fontDict->SetNewFor<CPDF_Number>("FirstChar", currentChar);
  int nextChar;
  CPDF_Array* widthsArray = pDoc->NewIndirect<CPDF_Array>();
  while (true) {
    int width = pFont->GetGlyphWidth(glyphIndex);
    widthsArray->AddNew<CPDF_Number>(width);
    nextChar = FXFT_Get_Next_Char(pFont->GetFace(), currentChar, &glyphIndex);
    if (glyphIndex == 0)
      break;
    for (int i = currentChar + 1; i < nextChar; i++)
      widthsArray->AddNew<CPDF_Number>(0);
    currentChar = nextChar;
  }
  fontDict->SetNewFor<CPDF_Number>("LastChar", currentChar);
  fontDict->SetNewFor<CPDF_Reference>("Widths", pDoc, widthsArray->GetObjNum());
  CPDF_Dictionary* fontDesc = pDoc->NewIndirect<CPDF_Dictionary>();
  fontDesc->SetNewFor<CPDF_Name>("Type", "FontDescriptor");
  fontDesc->SetNewFor<CPDF_Name>("FontName", name);
  int flags = 0;
  if (FXFT_Is_Face_fixedwidth(pFont->GetFace()))
    flags |= FXFONT_FIXED_PITCH;
  if (name.Find("Serif") > -1)
    flags |= FXFONT_SERIF;
  if (FXFT_Is_Face_Italic(pFont->GetFace()))
    flags |= FXFONT_ITALIC;
  if (FXFT_Is_Face_Bold(pFont->GetFace()))
    flags |= FXFONT_BOLD;

  // TODO(npm): How do I know if a Type1 font is symbolic, script, allcap,
  // smallcap
  flags |= FXFONT_NONSYMBOLIC;

  fontDesc->SetNewFor<CPDF_Number>("Flags", flags);
  FX_RECT bbox;
  pFont->GetBBox(bbox);
  auto pBBox = pdfium::MakeUnique<CPDF_Array>();
  pBBox->AddNew<CPDF_Number>(bbox.left);
  pBBox->AddNew<CPDF_Number>(bbox.bottom);
  pBBox->AddNew<CPDF_Number>(bbox.right);
  pBBox->AddNew<CPDF_Number>(bbox.top);
  fontDesc->SetFor("FontBBox", std::move(pBBox));

  // TODO(npm): calculate italic angle correctly
  fontDesc->SetNewFor<CPDF_Number>("ItalicAngle", pFont->IsItalic() ? -12 : 0);

  fontDesc->SetNewFor<CPDF_Number>("Ascent", pFont->GetAscent());
  fontDesc->SetNewFor<CPDF_Number>("Descent", pFont->GetDescent());

  // TODO(npm): calculate the capheight, stemV correctly
  fontDesc->SetNewFor<CPDF_Number>("CapHeight", pFont->GetAscent());
  fontDesc->SetNewFor<CPDF_Number>("StemV", pFont->IsBold() ? 120 : 70);

  CPDF_Stream* pStream = pDoc->NewIndirect<CPDF_Stream>();
  pStream->SetData(data, size);
  fontDesc->SetNewFor<CPDF_Reference>("FontFile", pDoc, pStream->GetObjNum());
  fontDict->SetNewFor<CPDF_Reference>("FontDescriptor", pDoc,
                                      fontDesc->GetObjNum());
  return pDoc->LoadFont(fontDict);
}

// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fpdfapi/font/cpdf_font.h"
#include "core/fpdfapi/parser/fpdf_parser_decode.h"
#include "core/fpdfdoc/cpvt_word.h"
#include "core/fpdfdoc/ipvt_fontmap.h"
#include "fpdfsdk/fxedit/fx_edit.h"
#include "fpdfsdk/fxedit/fxet_edit.h"

CFX_ByteString GetPDFWordString(IPVT_FontMap* pFontMap,
                                int32_t nFontIndex,
                                uint16_t Word,
                                uint16_t SubWord) {
  CPDF_Font* pPDFFont = pFontMap->GetPDFFont(nFontIndex);
  if (!pPDFFont)
    return CFX_ByteString();

  CFX_ByteString sWord;
  if (SubWord > 0) {
    Word = SubWord;
  } else {
    uint32_t dwCharCode = pPDFFont->IsUnicodeCompatible()
                              ? pPDFFont->CharCodeFromUnicode(Word)
                              : pFontMap->CharCodeFromUnicode(nFontIndex, Word);

    if (dwCharCode > 0) {
      pPDFFont->AppendChar(sWord, dwCharCode);
      return sWord;
    }
  }

  pPDFFont->AppendChar(sWord, Word);
  return sWord;
}

// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fpdfdoc/ctypeset.h"

#include <algorithm>

#include "core/fpdfdoc/cline.h"
#include "core/fpdfdoc/cpvt_wordinfo.h"
#include "core/fpdfdoc/csection.h"

namespace {

const uint8_t special_chars[128] = {
    0x00, 0x0C, 0x08, 0x0C, 0x08, 0x00, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x08, 0x00,
    0x10, 0x00, 0x00, 0x28, 0x0C, 0x08, 0x00, 0x00, 0x28, 0x28, 0x28, 0x28,
    0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x08, 0x08,
    0x00, 0x00, 0x00, 0x08, 0x00, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x0C, 0x00, 0x08, 0x00, 0x00,
    0x00, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
    0x01, 0x01, 0x01, 0x0C, 0x00, 0x08, 0x00, 0x00,
};

bool IsLatin(uint16_t word) {
  if (word <= 0x007F)
    return !!(special_chars[word] & 0x01);

  return ((word >= 0x00C0 && word <= 0x00FF) ||
          (word >= 0x0100 && word <= 0x024F) ||
          (word >= 0x1E00 && word <= 0x1EFF) ||
          (word >= 0x2C60 && word <= 0x2C7F) ||
          (word >= 0xA720 && word <= 0xA7FF) ||
          (word >= 0xFF21 && word <= 0xFF3A) ||
          (word >= 0xFF41 && word <= 0xFF5A));
}

bool IsDigit(uint32_t word) {
  return word >= 0x0030 && word <= 0x0039;
}

bool IsCJK(uint32_t word) {
  if ((word >= 0x1100 && word <= 0x11FF) ||
      (word >= 0x2E80 && word <= 0x2FFF) ||
      (word >= 0x3040 && word <= 0x9FBF) ||
      (word >= 0xAC00 && word <= 0xD7AF) ||
      (word >= 0xF900 && word <= 0xFAFF) ||
      (word >= 0xFE30 && word <= 0xFE4F) ||
      (word >= 0x20000 && word <= 0x2A6DF) ||
      (word >= 0x2F800 && word <= 0x2FA1F)) {
    return true;
  }
  if (word >= 0x3000 && word <= 0x303F) {
    return (
        word == 0x3005 || word == 0x3006 || word == 0x3021 || word == 0x3022 ||
        word == 0x3023 || word == 0x3024 || word == 0x3025 || word == 0x3026 ||
        word == 0x3027 || word == 0x3028 || word == 0x3029 || word == 0x3031 ||
        word == 0x3032 || word == 0x3033 || word == 0x3034 || word == 0x3035);
  }
  return word >= 0xFF66 && word <= 0xFF9D;
}

bool IsPunctuation(uint32_t word) {
  if (word <= 0x007F)
    return !!(special_chars[word] & 0x08);

  if (word >= 0x0080 && word <= 0x00FF) {
    return (word == 0x0082 || word == 0x0084 || word == 0x0085 ||
            word == 0x0091 || word == 0x0092 || word == 0x0093 ||
            word <= 0x0094 || word == 0x0096 || word == 0x00B4 ||
            word == 0x00B8);
  }

  if (word >= 0x2000 && word <= 0x206F) {
    return (
        word == 0x2010 || word == 0x2011 || word == 0x2012 || word == 0x2013 ||
        word == 0x2018 || word == 0x2019 || word == 0x201A || word == 0x201B ||
        word == 0x201C || word == 0x201D || word == 0x201E || word == 0x201F ||
        word == 0x2032 || word == 0x2033 || word == 0x2034 || word == 0x2035 ||
        word == 0x2036 || word == 0x2037 || word == 0x203C || word == 0x203D ||
        word == 0x203E || word == 0x2044);
  }

  if (word >= 0x3000 && word <= 0x303F) {
    return (
        word == 0x3001 || word == 0x3002 || word == 0x3003 || word == 0x3005 ||
        word == 0x3009 || word == 0x300A || word == 0x300B || word == 0x300C ||
        word == 0x300D || word == 0x300F || word == 0x300E || word == 0x3010 ||
        word == 0x3011 || word == 0x3014 || word == 0x3015 || word == 0x3016 ||
        word == 0x3017 || word == 0x3018 || word == 0x3019 || word == 0x301A ||
        word == 0x301B || word == 0x301D || word == 0x301E || word == 0x301F);
  }

  if (word >= 0xFE50 && word <= 0xFE6F)
    return (word >= 0xFE50 && word <= 0xFE5E) || word == 0xFE63;

  if (word >= 0xFF00 && word <= 0xFFEF) {
    return (
        word == 0xFF01 || word == 0xFF02 || word == 0xFF07 || word == 0xFF08 ||
        word == 0xFF09 || word == 0xFF0C || word == 0xFF0E || word == 0xFF0F ||
        word == 0xFF1A || word == 0xFF1B || word == 0xFF1F || word == 0xFF3B ||
        word == 0xFF3D || word == 0xFF40 || word == 0xFF5B || word == 0xFF5C ||
        word == 0xFF5D || word == 0xFF61 || word == 0xFF62 || word == 0xFF63 ||
        word == 0xFF64 || word == 0xFF65 || word == 0xFF9E || word == 0xFF9F);
  }

  return false;
}

bool IsConnectiveSymbol(uint32_t word) {
  return word <= 0x007F && (special_chars[word] & 0x20);
}

bool IsOpenStylePunctuation(uint32_t word) {
  if (word <= 0x007F)
    return !!(special_chars[word] & 0x04);

  return (word == 0x300A || word == 0x300C || word == 0x300E ||
          word == 0x3010 || word == 0x3014 || word == 0x3016 ||
          word == 0x3018 || word == 0x301A || word == 0xFF08 ||
          word == 0xFF3B || word == 0xFF5B || word == 0xFF62);
}

bool IsCurrencySymbol(uint16_t word) {
  return (word == 0x0024 || word == 0x0080 || word == 0x00A2 ||
          word == 0x00A3 || word == 0x00A4 || word == 0x00A5 ||
          (word >= 0x20A0 && word <= 0x20CF) || word == 0xFE69 ||
          word == 0xFF04 || word == 0xFFE0 || word == 0xFFE1 ||
          word == 0xFFE5 || word == 0xFFE6);
}

bool IsPrefixSymbol(uint16_t word) {
  return IsCurrencySymbol(word) || word == 0x2116;
}

bool IsSpace(uint16_t word) {
  return word == 0x0020 || word == 0x3000;
}

bool NeedDivision(uint16_t prevWord, uint16_t curWord) {
  if ((IsLatin(prevWord) || IsDigit(prevWord)) &&
      (IsLatin(curWord) || IsDigit(curWord))) {
    return false;
  }
  if (IsSpace(curWord) || IsPunctuation(curWord)) {
    return false;
  }
  if (IsConnectiveSymbol(prevWord) || IsConnectiveSymbol(curWord)) {
    return false;
  }
  if (IsSpace(prevWord) || IsPunctuation(prevWord)) {
    return true;
  }
  if (IsPrefixSymbol(prevWord)) {
    return false;
  }
  if (IsPrefixSymbol(curWord) || IsCJK(curWord)) {
    return true;
  }
  if (IsCJK(prevWord)) {
    return true;
  }
  return false;
}

}  // namespace

CTypeset::CTypeset(CSection* pSection)
    : m_rcRet(0.0f, 0.0f, 0.0f, 0.0f),
      m_pVT(pSection->m_pVT),
      m_pSection(pSection) {}

CTypeset::~CTypeset() {}

CPVT_FloatRect CTypeset::CharArray() {
  ASSERT(m_pSection);
  FX_FLOAT fLineAscent =
      m_pVT->GetFontAscent(m_pVT->GetDefaultFontIndex(), m_pVT->GetFontSize());
  FX_FLOAT fLineDescent =
      m_pVT->GetFontDescent(m_pVT->GetDefaultFontIndex(), m_pVT->GetFontSize());
  m_rcRet.Default();
  FX_FLOAT x = 0.0f, y = 0.0f;
  FX_FLOAT fNextWidth;
  int32_t nStart = 0;
  FX_FLOAT fNodeWidth = m_pVT->GetPlateWidth() /
                        (m_pVT->m_nCharArray <= 0 ? 1 : m_pVT->m_nCharArray);
  if (CLine* pLine = m_pSection->m_LineArray.GetAt(0)) {
    x = 0.0f;
    y += m_pVT->GetLineLeading(m_pSection->m_SecInfo);
    y += fLineAscent;
    nStart = 0;
    switch (m_pVT->GetAlignment(m_pSection->m_SecInfo)) {
      case 0:
        pLine->m_LineInfo.fLineX = fNodeWidth * VARIABLETEXT_HALF;
        break;
      case 1:
        nStart = (m_pVT->m_nCharArray - m_pSection->m_WordArray.GetSize()) / 2;
        pLine->m_LineInfo.fLineX =
            fNodeWidth * nStart - fNodeWidth * VARIABLETEXT_HALF;
        break;
      case 2:
        nStart = m_pVT->m_nCharArray - m_pSection->m_WordArray.GetSize();
        pLine->m_LineInfo.fLineX =
            fNodeWidth * nStart - fNodeWidth * VARIABLETEXT_HALF;
        break;
    }
    for (int32_t w = 0, sz = m_pSection->m_WordArray.GetSize(); w < sz; w++) {
      if (w >= m_pVT->m_nCharArray) {
        break;
      }
      fNextWidth = 0;
      if (CPVT_WordInfo* pNextWord = m_pSection->m_WordArray.GetAt(w + 1)) {
        pNextWord->fWordTail = 0;
        fNextWidth = m_pVT->GetWordWidth(*pNextWord);
      }
      if (CPVT_WordInfo* pWord = m_pSection->m_WordArray.GetAt(w)) {
        pWord->fWordTail = 0;
        FX_FLOAT fWordWidth = m_pVT->GetWordWidth(*pWord);
        FX_FLOAT fWordAscent = m_pVT->GetWordAscent(*pWord);
        FX_FLOAT fWordDescent = m_pVT->GetWordDescent(*pWord);
        x = (FX_FLOAT)(fNodeWidth * (w + nStart + 0.5) -
                       fWordWidth * VARIABLETEXT_HALF);
        pWord->fWordX = x;
        pWord->fWordY = y;
        if (w == 0) {
          pLine->m_LineInfo.fLineX = x;
        }
        if (w != m_pSection->m_WordArray.GetSize() - 1) {
          pWord->fWordTail =
              (fNodeWidth - (fWordWidth + fNextWidth) * VARIABLETEXT_HALF > 0
                   ? fNodeWidth - (fWordWidth + fNextWidth) * VARIABLETEXT_HALF
                   : 0);
        } else {
          pWord->fWordTail = 0;
        }
        x += fWordWidth;
        fLineAscent = std::max(fLineAscent, fWordAscent);
        fLineDescent = std::min(fLineDescent, fWordDescent);
      }
    }
    pLine->m_LineInfo.nBeginWordIndex = 0;
    pLine->m_LineInfo.nEndWordIndex = m_pSection->m_WordArray.GetSize() - 1;
    pLine->m_LineInfo.fLineY = y;
    pLine->m_LineInfo.fLineWidth = x - pLine->m_LineInfo.fLineX;
    pLine->m_LineInfo.fLineAscent = fLineAscent;
    pLine->m_LineInfo.fLineDescent = fLineDescent;
    y -= fLineDescent;
  }
  return m_rcRet = CPVT_FloatRect(0, 0, x, y);
}

CFX_SizeF CTypeset::GetEditSize(FX_FLOAT fFontSize) {
  ASSERT(m_pSection);
  ASSERT(m_pVT);
  SplitLines(false, fFontSize);
  return CFX_SizeF(m_rcRet.Width(), m_rcRet.Height());
}

CPVT_FloatRect CTypeset::Typeset() {
  ASSERT(m_pVT);
  m_pSection->m_LineArray.Empty();
  SplitLines(true, 0.0f);
  m_pSection->m_LineArray.Clear();
  OutputLines();
  return m_rcRet;
}

void CTypeset::SplitLines(bool bTypeset, FX_FLOAT fFontSize) {
  ASSERT(m_pVT);
  ASSERT(m_pSection);
  int32_t nLineHead = 0;
  int32_t nLineTail = 0;
  FX_FLOAT fMaxX = 0.0f, fMaxY = 0.0f;
  FX_FLOAT fLineWidth = 0.0f, fBackupLineWidth = 0.0f;
  FX_FLOAT fLineAscent = 0.0f, fBackupLineAscent = 0.0f;
  FX_FLOAT fLineDescent = 0.0f, fBackupLineDescent = 0.0f;
  int32_t nWordStartPos = 0;
  bool bFullWord = false;
  int32_t nLineFullWordIndex = 0;
  int32_t nCharIndex = 0;
  CPVT_LineInfo line;
  FX_FLOAT fWordWidth = 0;
  FX_FLOAT fTypesetWidth = std::max(
      m_pVT->GetPlateWidth() - m_pVT->GetLineIndent(m_pSection->m_SecInfo),
      0.0f);
  int32_t nTotalWords = m_pSection->m_WordArray.GetSize();
  bool bOpened = false;
  if (nTotalWords > 0) {
    int32_t i = 0;
    while (i < nTotalWords) {
      CPVT_WordInfo* pWord = m_pSection->m_WordArray.GetAt(i);
      CPVT_WordInfo* pOldWord = pWord;
      if (i > 0) {
        pOldWord = m_pSection->m_WordArray.GetAt(i - 1);
      }
      if (pWord) {
        if (bTypeset) {
          fLineAscent = std::max(fLineAscent, m_pVT->GetWordAscent(*pWord));
          fLineDescent = std::min(fLineDescent, m_pVT->GetWordDescent(*pWord));
          fWordWidth = m_pVT->GetWordWidth(*pWord);
        } else {
          fLineAscent =
              std::max(fLineAscent, m_pVT->GetWordAscent(*pWord, fFontSize));
          fLineDescent =
              std::min(fLineDescent, m_pVT->GetWordDescent(*pWord, fFontSize));
          fWordWidth = m_pVT->GetWordWidth(
              pWord->nFontIndex, pWord->Word, m_pVT->m_wSubWord,
              m_pVT->m_fCharSpace, m_pVT->m_nHorzScale, fFontSize,
              pWord->fWordTail);
        }
        if (!bOpened) {
          if (IsOpenStylePunctuation(pWord->Word)) {
            bOpened = true;
            bFullWord = true;
          } else if (pOldWord) {
            if (NeedDivision(pOldWord->Word, pWord->Word)) {
              bFullWord = true;
            }
          }
        } else {
          if (!IsSpace(pWord->Word) && !IsOpenStylePunctuation(pWord->Word)) {
            bOpened = false;
          }
        }
        if (bFullWord) {
          bFullWord = false;
          if (nCharIndex > 0) {
            nLineFullWordIndex++;
          }
          nWordStartPos = i;
          fBackupLineWidth = fLineWidth;
          fBackupLineAscent = fLineAscent;
          fBackupLineDescent = fLineDescent;
        }
        nCharIndex++;
      }
      if (m_pVT->m_bLimitWidth && fTypesetWidth > 0 &&
          fLineWidth + fWordWidth > fTypesetWidth) {
        if (nLineFullWordIndex > 0) {
          i = nWordStartPos;
          fLineWidth = fBackupLineWidth;
          fLineAscent = fBackupLineAscent;
          fLineDescent = fBackupLineDescent;
        }
        if (nCharIndex == 1) {
          fLineWidth = fWordWidth;
          i++;
        }
        nLineTail = i - 1;
        if (bTypeset) {
          line.nBeginWordIndex = nLineHead;
          line.nEndWordIndex = nLineTail;
          line.nTotalWord = nLineTail - nLineHead + 1;
          line.fLineWidth = fLineWidth;
          line.fLineAscent = fLineAscent;
          line.fLineDescent = fLineDescent;
          m_pSection->AddLine(line);
        }
        fMaxY += (fLineAscent + m_pVT->GetLineLeading(m_pSection->m_SecInfo));
        fMaxY -= fLineDescent;
        fMaxX = std::max(fLineWidth, fMaxX);
        nLineHead = i;
        fLineWidth = 0.0f;
        fLineAscent = 0.0f;
        fLineDescent = 0.0f;
        nCharIndex = 0;
        nLineFullWordIndex = 0;
        bFullWord = false;
      } else {
        fLineWidth += fWordWidth;
        i++;
      }
    }
    if (nLineHead <= nTotalWords - 1) {
      nLineTail = nTotalWords - 1;
      if (bTypeset) {
        line.nBeginWordIndex = nLineHead;
        line.nEndWordIndex = nLineTail;
        line.nTotalWord = nLineTail - nLineHead + 1;
        line.fLineWidth = fLineWidth;
        line.fLineAscent = fLineAscent;
        line.fLineDescent = fLineDescent;
        m_pSection->AddLine(line);
      }
      fMaxY += (fLineAscent + m_pVT->GetLineLeading(m_pSection->m_SecInfo));
      fMaxY -= fLineDescent;
      fMaxX = std::max(fLineWidth, fMaxX);
    }
  } else {
    if (bTypeset) {
      fLineAscent = m_pVT->GetLineAscent(m_pSection->m_SecInfo);
      fLineDescent = m_pVT->GetLineDescent(m_pSection->m_SecInfo);
    } else {
      fLineAscent =
          m_pVT->GetFontAscent(m_pVT->GetDefaultFontIndex(), fFontSize);
      fLineDescent =
          m_pVT->GetFontDescent(m_pVT->GetDefaultFontIndex(), fFontSize);
    }
    if (bTypeset) {
      line.nBeginWordIndex = -1;
      line.nEndWordIndex = -1;
      line.nTotalWord = 0;
      line.fLineWidth = 0;
      line.fLineAscent = fLineAscent;
      line.fLineDescent = fLineDescent;
      m_pSection->AddLine(line);
    }
    fMaxY += m_pVT->GetLineLeading(m_pSection->m_SecInfo) + fLineAscent -
             fLineDescent;
  }
  m_rcRet = CPVT_FloatRect(0, 0, fMaxX, fMaxY);
}

void CTypeset::OutputLines() {
  ASSERT(m_pVT);
  ASSERT(m_pSection);
  FX_FLOAT fMinX = 0.0f, fMinY = 0.0f, fMaxX = 0.0f, fMaxY = 0.0f;
  FX_FLOAT fPosX = 0.0f, fPosY = 0.0f;
  FX_FLOAT fLineIndent = m_pVT->GetLineIndent(m_pSection->m_SecInfo);
  FX_FLOAT fTypesetWidth = std::max(m_pVT->GetPlateWidth() - fLineIndent, 0.0f);
  switch (m_pVT->GetAlignment(m_pSection->m_SecInfo)) {
    default:
    case 0:
      fMinX = 0.0f;
      break;
    case 1:
      fMinX = (fTypesetWidth - m_rcRet.Width()) * VARIABLETEXT_HALF;
      break;
    case 2:
      fMinX = fTypesetWidth - m_rcRet.Width();
      break;
  }
  fMaxX = fMinX + m_rcRet.Width();
  fMinY = 0.0f;
  fMaxY = m_rcRet.Height();
  int32_t nTotalLines = m_pSection->m_LineArray.GetSize();
  if (nTotalLines > 0) {
    m_pSection->m_SecInfo.nTotalLine = nTotalLines;
    for (int32_t l = 0; l < nTotalLines; l++) {
      if (CLine* pLine = m_pSection->m_LineArray.GetAt(l)) {
        switch (m_pVT->GetAlignment(m_pSection->m_SecInfo)) {
          default:
          case 0:
            fPosX = 0;
            break;
          case 1:
            fPosX = (fTypesetWidth - pLine->m_LineInfo.fLineWidth) *
                    VARIABLETEXT_HALF;
            break;
          case 2:
            fPosX = fTypesetWidth - pLine->m_LineInfo.fLineWidth;
            break;
        }
        fPosX += fLineIndent;
        fPosY += m_pVT->GetLineLeading(m_pSection->m_SecInfo);
        fPosY += pLine->m_LineInfo.fLineAscent;
        pLine->m_LineInfo.fLineX = fPosX - fMinX;
        pLine->m_LineInfo.fLineY = fPosY - fMinY;
        for (int32_t w = pLine->m_LineInfo.nBeginWordIndex;
             w <= pLine->m_LineInfo.nEndWordIndex; w++) {
          if (CPVT_WordInfo* pWord = m_pSection->m_WordArray.GetAt(w)) {
            pWord->fWordX = fPosX - fMinX;
            if (pWord->pWordProps) {
              switch (pWord->pWordProps->nScriptType) {
                default:
                case CPDF_VariableText::ScriptType::Normal:
                  pWord->fWordY = fPosY - fMinY;
                  break;
                case CPDF_VariableText::ScriptType::Super:
                  pWord->fWordY = fPosY - m_pVT->GetWordAscent(*pWord) - fMinY;
                  break;
                case CPDF_VariableText::ScriptType::Sub:
                  pWord->fWordY = fPosY - m_pVT->GetWordDescent(*pWord) - fMinY;
                  break;
              }
            } else {
              pWord->fWordY = fPosY - fMinY;
            }
            fPosX += m_pVT->GetWordWidth(*pWord);
          }
        }
        fPosY -= pLine->m_LineInfo.fLineDescent;
      }
    }
  }
  m_rcRet = CPVT_FloatRect(fMinX, fMinY, fMaxX, fMaxY);
}

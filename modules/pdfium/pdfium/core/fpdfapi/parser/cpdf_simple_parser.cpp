// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fpdfapi/parser/cpdf_simple_parser.h"

#include "core/fpdfapi/parser/fpdf_parser_utility.h"

CPDF_SimpleParser::CPDF_SimpleParser(const uint8_t* pData, uint32_t dwSize)
    : m_pData(pData), m_dwSize(dwSize), m_dwCurPos(0) {}

CPDF_SimpleParser::CPDF_SimpleParser(const CFX_ByteStringC& str)
    : m_pData(str.raw_str()), m_dwSize(str.GetLength()), m_dwCurPos(0) {}

void CPDF_SimpleParser::ParseWord(const uint8_t*& pStart, uint32_t& dwSize) {
  pStart = nullptr;
  dwSize = 0;
  uint8_t ch;
  while (1) {
    if (m_dwSize <= m_dwCurPos)
      return;
    ch = m_pData[m_dwCurPos++];
    while (PDFCharIsWhitespace(ch)) {
      if (m_dwSize <= m_dwCurPos)
        return;
      ch = m_pData[m_dwCurPos++];
    }

    if (ch != '%')
      break;

    while (1) {
      if (m_dwSize <= m_dwCurPos)
        return;
      ch = m_pData[m_dwCurPos++];
      if (PDFCharIsLineEnding(ch))
        break;
    }
  }

  uint32_t start_pos = m_dwCurPos - 1;
  pStart = m_pData + start_pos;
  if (PDFCharIsDelimiter(ch)) {
    if (ch == '/') {
      while (1) {
        if (m_dwSize <= m_dwCurPos)
          return;
        ch = m_pData[m_dwCurPos++];
        if (!PDFCharIsOther(ch) && !PDFCharIsNumeric(ch)) {
          m_dwCurPos--;
          dwSize = m_dwCurPos - start_pos;
          return;
        }
      }
    } else {
      dwSize = 1;
      if (ch == '<') {
        if (m_dwSize <= m_dwCurPos)
          return;
        ch = m_pData[m_dwCurPos++];
        if (ch == '<')
          dwSize = 2;
        else
          m_dwCurPos--;
      } else if (ch == '>') {
        if (m_dwSize <= m_dwCurPos)
          return;
        ch = m_pData[m_dwCurPos++];
        if (ch == '>')
          dwSize = 2;
        else
          m_dwCurPos--;
      }
    }
    return;
  }

  dwSize = 1;
  while (1) {
    if (m_dwSize <= m_dwCurPos)
      return;
    ch = m_pData[m_dwCurPos++];

    if (PDFCharIsDelimiter(ch) || PDFCharIsWhitespace(ch)) {
      m_dwCurPos--;
      break;
    }
    dwSize++;
  }
}

CFX_ByteStringC CPDF_SimpleParser::GetWord() {
  const uint8_t* pStart;
  uint32_t dwSize;
  ParseWord(pStart, dwSize);
  if (dwSize == 1 && pStart[0] == '<') {
    while (m_dwCurPos < m_dwSize && m_pData[m_dwCurPos] != '>') {
      m_dwCurPos++;
    }
    if (m_dwCurPos < m_dwSize) {
      m_dwCurPos++;
    }
    return CFX_ByteStringC(pStart,
                           (FX_STRSIZE)(m_dwCurPos - (pStart - m_pData)));
  }
  if (dwSize == 1 && pStart[0] == '(') {
    int level = 1;
    while (m_dwCurPos < m_dwSize) {
      if (m_pData[m_dwCurPos] == ')') {
        level--;
        if (level == 0) {
          break;
        }
      }
      if (m_pData[m_dwCurPos] == '\\') {
        if (m_dwSize <= m_dwCurPos) {
          break;
        }
        m_dwCurPos++;
      } else if (m_pData[m_dwCurPos] == '(') {
        level++;
      }
      if (m_dwSize <= m_dwCurPos) {
        break;
      }
      m_dwCurPos++;
    }
    if (m_dwCurPos < m_dwSize) {
      m_dwCurPos++;
    }
    return CFX_ByteStringC(pStart,
                           (FX_STRSIZE)(m_dwCurPos - (pStart - m_pData)));
  }
  return CFX_ByteStringC(pStart, dwSize);
}

bool CPDF_SimpleParser::FindTagParamFromStart(const CFX_ByteStringC& token,
                                              int nParams) {
  nParams++;
  uint32_t* pBuf = FX_Alloc(uint32_t, nParams);
  int buf_index = 0;
  int buf_count = 0;
  m_dwCurPos = 0;
  while (1) {
    pBuf[buf_index++] = m_dwCurPos;
    if (buf_index == nParams) {
      buf_index = 0;
    }
    buf_count++;
    if (buf_count > nParams) {
      buf_count = nParams;
    }
    CFX_ByteStringC word = GetWord();
    if (word.IsEmpty()) {
      FX_Free(pBuf);
      return false;
    }
    if (word == token) {
      if (buf_count < nParams) {
        continue;
      }
      m_dwCurPos = pBuf[buf_index];
      FX_Free(pBuf);
      return true;
    }
  }
  return false;
}

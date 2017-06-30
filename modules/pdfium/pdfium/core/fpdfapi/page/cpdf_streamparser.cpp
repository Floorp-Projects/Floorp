// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fpdfapi/page/cpdf_streamparser.h"

#include <limits.h>

#include <memory>
#include <utility>

#include "core/fpdfapi/cpdf_modulemgr.h"
#include "core/fpdfapi/page/cpdf_docpagedata.h"
#include "core/fpdfapi/parser/cpdf_array.h"
#include "core/fpdfapi/parser/cpdf_boolean.h"
#include "core/fpdfapi/parser/cpdf_dictionary.h"
#include "core/fpdfapi/parser/cpdf_document.h"
#include "core/fpdfapi/parser/cpdf_name.h"
#include "core/fpdfapi/parser/cpdf_null.h"
#include "core/fpdfapi/parser/cpdf_number.h"
#include "core/fpdfapi/parser/cpdf_stream.h"
#include "core/fpdfapi/parser/cpdf_string.h"
#include "core/fpdfapi/parser/fpdf_parser_decode.h"
#include "core/fpdfapi/parser/fpdf_parser_utility.h"
#include "core/fxcodec/fx_codec.h"
#include "core/fxcrt/fx_ext.h"

namespace {

const uint32_t kMaxNestedParsingLevel = 512;
const uint32_t kMaxWordBuffer = 256;
const FX_STRSIZE kMaxStringLength = 32767;

uint32_t DecodeAllScanlines(std::unique_ptr<CCodec_ScanlineDecoder> pDecoder,
                            uint8_t*& dest_buf,
                            uint32_t& dest_size) {
  if (!pDecoder)
    return FX_INVALID_OFFSET;
  int ncomps = pDecoder->CountComps();
  int bpc = pDecoder->GetBPC();
  int width = pDecoder->GetWidth();
  int height = pDecoder->GetHeight();
  int pitch = (width * ncomps * bpc + 7) / 8;
  if (height == 0 || pitch > (1 << 30) / height)
    return FX_INVALID_OFFSET;

  dest_buf = FX_Alloc2D(uint8_t, pitch, height);
  dest_size = pitch * height;  // Safe since checked alloc returned.
  for (int row = 0; row < height; row++) {
    const uint8_t* pLine = pDecoder->GetScanline(row);
    if (!pLine)
      break;

    FXSYS_memcpy(dest_buf + row * pitch, pLine, pitch);
  }
  return pDecoder->GetSrcOffset();
}

uint32_t PDF_DecodeInlineStream(const uint8_t* src_buf,
                                uint32_t limit,
                                int width,
                                int height,
                                CFX_ByteString& decoder,
                                CPDF_Dictionary* pParam,
                                uint8_t*& dest_buf,
                                uint32_t& dest_size) {
  if (decoder == "CCITTFaxDecode" || decoder == "CCF") {
    std::unique_ptr<CCodec_ScanlineDecoder> pDecoder =
        FPDFAPI_CreateFaxDecoder(src_buf, limit, width, height, pParam);
    return DecodeAllScanlines(std::move(pDecoder), dest_buf, dest_size);
  }
  if (decoder == "ASCII85Decode" || decoder == "A85")
    return A85Decode(src_buf, limit, dest_buf, dest_size);
  if (decoder == "ASCIIHexDecode" || decoder == "AHx")
    return HexDecode(src_buf, limit, dest_buf, dest_size);
  if (decoder == "FlateDecode" || decoder == "Fl") {
    return FPDFAPI_FlateOrLZWDecode(false, src_buf, limit, pParam, dest_size,
                                    dest_buf, dest_size);
  }
  if (decoder == "LZWDecode" || decoder == "LZW") {
    return FPDFAPI_FlateOrLZWDecode(true, src_buf, limit, pParam, 0, dest_buf,
                                    dest_size);
  }
  if (decoder == "DCTDecode" || decoder == "DCT") {
    std::unique_ptr<CCodec_ScanlineDecoder> pDecoder =
        CPDF_ModuleMgr::Get()->GetJpegModule()->CreateDecoder(
            src_buf, limit, width, height, 0,
            !pParam || pParam->GetIntegerFor("ColorTransform", 1));
    return DecodeAllScanlines(std::move(pDecoder), dest_buf, dest_size);
  }
  if (decoder == "RunLengthDecode" || decoder == "RL")
    return RunLengthDecode(src_buf, limit, dest_buf, dest_size);
  dest_size = 0;
  dest_buf = 0;
  return (uint32_t)-1;
}

}  // namespace

CPDF_StreamParser::CPDF_StreamParser(const uint8_t* pData, uint32_t dwSize)
    : m_pBuf(pData),
      m_Size(dwSize),
      m_Pos(0),
      m_pPool(nullptr) {}

CPDF_StreamParser::CPDF_StreamParser(
    const uint8_t* pData,
    uint32_t dwSize,
    const CFX_WeakPtr<CFX_ByteStringPool>& pPool)
    : m_pBuf(pData),
      m_Size(dwSize),
      m_Pos(0),
      m_pPool(pPool) {}

CPDF_StreamParser::~CPDF_StreamParser() {}

std::unique_ptr<CPDF_Stream> CPDF_StreamParser::ReadInlineStream(
    CPDF_Document* pDoc,
    std::unique_ptr<CPDF_Dictionary> pDict,
    CPDF_Object* pCSObj) {
  if (m_Pos == m_Size)
    return nullptr;

  if (PDFCharIsWhitespace(m_pBuf[m_Pos]))
    m_Pos++;

  CFX_ByteString Decoder;
  CPDF_Dictionary* pParam = nullptr;
  CPDF_Object* pFilter = pDict->GetDirectObjectFor("Filter");
  if (pFilter) {
    if (CPDF_Array* pArray = pFilter->AsArray()) {
      Decoder = pArray->GetStringAt(0);
      CPDF_Array* pParams = pDict->GetArrayFor("DecodeParms");
      if (pParams)
        pParam = pParams->GetDictAt(0);
    } else {
      Decoder = pFilter->GetString();
      pParam = pDict->GetDictFor("DecodeParms");
    }
  }
  uint32_t width = pDict->GetIntegerFor("Width");
  uint32_t height = pDict->GetIntegerFor("Height");
  uint32_t OrigSize = 0;
  if (pCSObj) {
    uint32_t bpc = pDict->GetIntegerFor("BitsPerComponent");
    uint32_t nComponents = 1;
    CPDF_ColorSpace* pCS = pDoc->LoadColorSpace(pCSObj);
    if (pCS) {
      nComponents = pCS->CountComponents();
      pDoc->GetPageData()->ReleaseColorSpace(pCSObj);
    } else {
      nComponents = 3;
    }
    uint32_t pitch = width;
    if (bpc && pitch > INT_MAX / bpc)
      return nullptr;

    pitch *= bpc;
    if (nComponents && pitch > INT_MAX / nComponents)
      return nullptr;

    pitch *= nComponents;
    if (pitch > INT_MAX - 7)
      return nullptr;

    pitch += 7;
    pitch /= 8;
    OrigSize = pitch;
  } else {
    if (width > INT_MAX - 7)
      return nullptr;

    OrigSize = ((width + 7) / 8);
  }
  if (height && OrigSize > INT_MAX / height)
    return nullptr;

  OrigSize *= height;
  std::unique_ptr<uint8_t, FxFreeDeleter> pData;
  uint32_t dwStreamSize;
  if (Decoder.IsEmpty()) {
    if (OrigSize > m_Size - m_Pos)
      OrigSize = m_Size - m_Pos;
    pData.reset(FX_Alloc(uint8_t, OrigSize));
    FXSYS_memcpy(pData.get(), m_pBuf + m_Pos, OrigSize);
    dwStreamSize = OrigSize;
    m_Pos += OrigSize;
  } else {
    uint8_t* pIgnore = nullptr;
    uint32_t dwDestSize = OrigSize;
    dwStreamSize =
        PDF_DecodeInlineStream(m_pBuf + m_Pos, m_Size - m_Pos, width, height,
                               Decoder, pParam, pIgnore, dwDestSize);
    FX_Free(pIgnore);
    if (static_cast<int>(dwStreamSize) < 0)
      return nullptr;

    uint32_t dwSavePos = m_Pos;
    m_Pos += dwStreamSize;
    while (1) {
      uint32_t dwPrevPos = m_Pos;
      CPDF_StreamParser::SyntaxType type = ParseNextElement();
      if (type == CPDF_StreamParser::EndOfData)
        break;

      if (type != CPDF_StreamParser::Keyword) {
        dwStreamSize += m_Pos - dwPrevPos;
        continue;
      }
      if (GetWord() == "EI") {
        m_Pos = dwPrevPos;
        break;
      }
      dwStreamSize += m_Pos - dwPrevPos;
    }
    m_Pos = dwSavePos;
    pData.reset(FX_Alloc(uint8_t, dwStreamSize));
    FXSYS_memcpy(pData.get(), m_pBuf + m_Pos, dwStreamSize);
    m_Pos += dwStreamSize;
  }
  pDict->SetNewFor<CPDF_Number>("Length", (int)dwStreamSize);
  return pdfium::MakeUnique<CPDF_Stream>(std::move(pData), dwStreamSize,
                                         std::move(pDict));
}

CPDF_StreamParser::SyntaxType CPDF_StreamParser::ParseNextElement() {
  m_pLastObj.reset();
  m_WordSize = 0;
  if (!PositionIsInBounds())
    return EndOfData;

  int ch = m_pBuf[m_Pos++];
  while (1) {
    while (PDFCharIsWhitespace(ch)) {
      if (!PositionIsInBounds())
        return EndOfData;

      ch = m_pBuf[m_Pos++];
    }

    if (ch != '%')
      break;

    while (1) {
      if (!PositionIsInBounds())
        return EndOfData;

      ch = m_pBuf[m_Pos++];
      if (PDFCharIsLineEnding(ch))
        break;
    }
  }

  if (PDFCharIsDelimiter(ch) && ch != '/') {
    m_Pos--;
    m_pLastObj = ReadNextObject(false, false, 0);
    return Others;
  }

  bool bIsNumber = true;
  while (1) {
    if (m_WordSize < kMaxWordBuffer)
      m_WordBuffer[m_WordSize++] = ch;

    if (!PDFCharIsNumeric(ch))
      bIsNumber = false;

    if (!PositionIsInBounds())
      break;

    ch = m_pBuf[m_Pos++];

    if (PDFCharIsDelimiter(ch) || PDFCharIsWhitespace(ch)) {
      m_Pos--;
      break;
    }
  }

  m_WordBuffer[m_WordSize] = 0;
  if (bIsNumber)
    return Number;

  if (m_WordBuffer[0] == '/')
    return Name;

  if (m_WordSize == 4) {
    if (memcmp(m_WordBuffer, "true", 4) == 0) {
      m_pLastObj = pdfium::MakeUnique<CPDF_Boolean>(true);
      return Others;
    }
    if (memcmp(m_WordBuffer, "null", 4) == 0) {
      m_pLastObj = pdfium::MakeUnique<CPDF_Null>();
      return Others;
    }
  } else if (m_WordSize == 5) {
    if (memcmp(m_WordBuffer, "false", 5) == 0) {
      m_pLastObj = pdfium::MakeUnique<CPDF_Boolean>(false);
      return Others;
    }
  }
  return Keyword;
}

std::unique_ptr<CPDF_Object> CPDF_StreamParser::ReadNextObject(
    bool bAllowNestedArray,
    bool bInArray,
    uint32_t dwRecursionLevel) {
  bool bIsNumber;
  // Must get the next word before returning to avoid infinite loops.
  GetNextWord(bIsNumber);
  if (!m_WordSize || dwRecursionLevel > kMaxNestedParsingLevel)
    return nullptr;

  if (bIsNumber) {
    m_WordBuffer[m_WordSize] = 0;
    return pdfium::MakeUnique<CPDF_Number>(
        CFX_ByteStringC(m_WordBuffer, m_WordSize));
  }

  int first_char = m_WordBuffer[0];
  if (first_char == '/') {
    CFX_ByteString name =
        PDF_NameDecode(CFX_ByteStringC(m_WordBuffer + 1, m_WordSize - 1));
    return pdfium::MakeUnique<CPDF_Name>(m_pPool, name);
  }

  if (first_char == '(') {
    CFX_ByteString str = ReadString();
    return pdfium::MakeUnique<CPDF_String>(m_pPool, str, false);
  }

  if (first_char == '<') {
    if (m_WordSize == 1)
      return pdfium::MakeUnique<CPDF_String>(m_pPool, ReadHexString(), true);

    auto pDict = pdfium::MakeUnique<CPDF_Dictionary>(m_pPool);
    while (1) {
      GetNextWord(bIsNumber);
      if (m_WordSize == 2 && m_WordBuffer[0] == '>')
        break;

      if (!m_WordSize || m_WordBuffer[0] != '/')
        return nullptr;

      CFX_ByteString key =
          PDF_NameDecode(CFX_ByteStringC(m_WordBuffer + 1, m_WordSize - 1));
      std::unique_ptr<CPDF_Object> pObj =
          ReadNextObject(true, bInArray, dwRecursionLevel + 1);
      if (!pObj)
        return nullptr;

      if (!key.IsEmpty())
        pDict->SetFor(key, std::move(pObj));
    }
    return std::move(pDict);
  }

  if (first_char == '[') {
    if ((!bAllowNestedArray && bInArray))
      return nullptr;

    auto pArray = pdfium::MakeUnique<CPDF_Array>();
    while (1) {
      std::unique_ptr<CPDF_Object> pObj =
          ReadNextObject(bAllowNestedArray, true, dwRecursionLevel + 1);
      if (pObj) {
        pArray->Add(std::move(pObj));
        continue;
      }
      if (!m_WordSize || m_WordBuffer[0] == ']')
        break;
    }
    return std::move(pArray);
  }

  if (m_WordSize == 5 && !memcmp(m_WordBuffer, "false", 5))
    return pdfium::MakeUnique<CPDF_Boolean>(false);

  if (m_WordSize == 4) {
    if (memcmp(m_WordBuffer, "true", 4) == 0)
      return pdfium::MakeUnique<CPDF_Boolean>(true);
    if (memcmp(m_WordBuffer, "null", 4) == 0)
      return pdfium::MakeUnique<CPDF_Null>();
  }

  return nullptr;
}

// TODO(npm): the following methods are almost identical in cpdf_syntaxparser
void CPDF_StreamParser::GetNextWord(bool& bIsNumber) {
  m_WordSize = 0;
  bIsNumber = true;
  if (!PositionIsInBounds())
    return;

  int ch = m_pBuf[m_Pos++];
  while (1) {
    while (PDFCharIsWhitespace(ch)) {
      if (!PositionIsInBounds()) {
        return;
      }
      ch = m_pBuf[m_Pos++];
    }

    if (ch != '%')
      break;

    while (1) {
      if (!PositionIsInBounds())
        return;
      ch = m_pBuf[m_Pos++];
      if (PDFCharIsLineEnding(ch))
        break;
    }
  }

  if (PDFCharIsDelimiter(ch)) {
    bIsNumber = false;
    m_WordBuffer[m_WordSize++] = ch;
    if (ch == '/') {
      while (1) {
        if (!PositionIsInBounds())
          return;
        ch = m_pBuf[m_Pos++];
        if (!PDFCharIsOther(ch) && !PDFCharIsNumeric(ch)) {
          m_Pos--;
          return;
        }

        if (m_WordSize < kMaxWordBuffer)
          m_WordBuffer[m_WordSize++] = ch;
      }
    } else if (ch == '<') {
      if (!PositionIsInBounds())
        return;
      ch = m_pBuf[m_Pos++];
      if (ch == '<')
        m_WordBuffer[m_WordSize++] = ch;
      else
        m_Pos--;
    } else if (ch == '>') {
      if (!PositionIsInBounds())
        return;
      ch = m_pBuf[m_Pos++];
      if (ch == '>')
        m_WordBuffer[m_WordSize++] = ch;
      else
        m_Pos--;
    }
    return;
  }

  while (1) {
    if (m_WordSize < kMaxWordBuffer)
      m_WordBuffer[m_WordSize++] = ch;
    if (!PDFCharIsNumeric(ch))
      bIsNumber = false;

    if (!PositionIsInBounds())
      return;
    ch = m_pBuf[m_Pos++];
    if (PDFCharIsDelimiter(ch) || PDFCharIsWhitespace(ch)) {
      m_Pos--;
      break;
    }
  }
}

CFX_ByteString CPDF_StreamParser::ReadString() {
  if (!PositionIsInBounds())
    return CFX_ByteString();

  uint8_t ch = m_pBuf[m_Pos++];
  CFX_ByteTextBuf buf;
  int parlevel = 0;
  int status = 0;
  int iEscCode = 0;
  while (1) {
    switch (status) {
      case 0:
        if (ch == ')') {
          if (parlevel == 0) {
            if (buf.GetLength() > kMaxStringLength) {
              return CFX_ByteString(buf.GetBuffer(), kMaxStringLength);
            }
            return buf.MakeString();
          }
          parlevel--;
          buf.AppendChar(')');
        } else if (ch == '(') {
          parlevel++;
          buf.AppendChar('(');
        } else if (ch == '\\') {
          status = 1;
        } else {
          buf.AppendChar((char)ch);
        }
        break;
      case 1:
        if (ch >= '0' && ch <= '7') {
          iEscCode = FXSYS_toDecimalDigit(static_cast<FX_WCHAR>(ch));
          status = 2;
          break;
        }
        if (ch == 'n') {
          buf.AppendChar('\n');
        } else if (ch == 'r') {
          buf.AppendChar('\r');
        } else if (ch == 't') {
          buf.AppendChar('\t');
        } else if (ch == 'b') {
          buf.AppendChar('\b');
        } else if (ch == 'f') {
          buf.AppendChar('\f');
        } else if (ch == '\r') {
          status = 4;
          break;
        } else if (ch == '\n') {
        } else {
          buf.AppendChar(ch);
        }
        status = 0;
        break;
      case 2:
        if (ch >= '0' && ch <= '7') {
          iEscCode =
              iEscCode * 8 + FXSYS_toDecimalDigit(static_cast<FX_WCHAR>(ch));
          status = 3;
        } else {
          buf.AppendChar(iEscCode);
          status = 0;
          continue;
        }
        break;
      case 3:
        if (ch >= '0' && ch <= '7') {
          iEscCode =
              iEscCode * 8 + FXSYS_toDecimalDigit(static_cast<FX_WCHAR>(ch));
          buf.AppendChar(iEscCode);
          status = 0;
        } else {
          buf.AppendChar(iEscCode);
          status = 0;
          continue;
        }
        break;
      case 4:
        status = 0;
        if (ch != '\n') {
          continue;
        }
        break;
    }
    if (!PositionIsInBounds())
      break;

    ch = m_pBuf[m_Pos++];
  }
  if (PositionIsInBounds())
    ++m_Pos;

  if (buf.GetLength() > kMaxStringLength) {
    return CFX_ByteString(buf.GetBuffer(), kMaxStringLength);
  }
  return buf.MakeString();
}

CFX_ByteString CPDF_StreamParser::ReadHexString() {
  if (!PositionIsInBounds())
    return CFX_ByteString();

  CFX_ByteTextBuf buf;
  bool bFirst = true;
  int code = 0;
  while (PositionIsInBounds()) {
    int ch = m_pBuf[m_Pos++];

    if (ch == '>')
      break;

    if (!std::isxdigit(ch))
      continue;

    int val = FXSYS_toHexDigit(ch);
    if (bFirst) {
      code = val * 16;
    } else {
      code += val;
      buf.AppendByte((uint8_t)code);
    }
    bFirst = !bFirst;
  }
  if (!bFirst)
    buf.AppendChar((char)code);

  if (buf.GetLength() > kMaxStringLength)
    return CFX_ByteString(buf.GetBuffer(), kMaxStringLength);

  return buf.MakeString();
}

bool CPDF_StreamParser::PositionIsInBounds() const {
  return m_Pos < m_Size;
}

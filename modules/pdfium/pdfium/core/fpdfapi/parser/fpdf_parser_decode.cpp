// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fpdfapi/parser/fpdf_parser_decode.h"

#include <limits.h>

#include <algorithm>
#include <utility>
#include <vector>

#include "core/fpdfapi/cpdf_modulemgr.h"
#include "core/fpdfapi/parser/cpdf_array.h"
#include "core/fpdfapi/parser/cpdf_dictionary.h"
#include "core/fpdfapi/parser/fpdf_parser_utility.h"
#include "core/fxcodec/fx_codec.h"
#include "core/fxcrt/fx_ext.h"
#include "third_party/base/stl_util.h"

namespace {

const uint32_t kMaxStreamSize = 20 * 1024 * 1024;

bool CheckFlateDecodeParams(int Colors, int BitsPerComponent, int Columns) {
  if (Colors < 0 || BitsPerComponent < 0 || Columns < 0)
    return false;

  int check = Columns;
  if (check > 0 && Colors > INT_MAX / check)
    return false;

  check *= Colors;
  if (check > 0 && BitsPerComponent > INT_MAX / check)
    return false;

  return check * BitsPerComponent <= INT_MAX - 7;
}

}  // namespace

const uint16_t PDFDocEncoding[256] = {
    0x0000, 0x0001, 0x0002, 0x0003, 0x0004, 0x0005, 0x0006, 0x0007, 0x0008,
    0x0009, 0x000a, 0x000b, 0x000c, 0x000d, 0x000e, 0x000f, 0x0010, 0x0011,
    0x0012, 0x0013, 0x0014, 0x0015, 0x0016, 0x0017, 0x02d8, 0x02c7, 0x02c6,
    0x02d9, 0x02dd, 0x02db, 0x02da, 0x02dc, 0x0020, 0x0021, 0x0022, 0x0023,
    0x0024, 0x0025, 0x0026, 0x0027, 0x0028, 0x0029, 0x002a, 0x002b, 0x002c,
    0x002d, 0x002e, 0x002f, 0x0030, 0x0031, 0x0032, 0x0033, 0x0034, 0x0035,
    0x0036, 0x0037, 0x0038, 0x0039, 0x003a, 0x003b, 0x003c, 0x003d, 0x003e,
    0x003f, 0x0040, 0x0041, 0x0042, 0x0043, 0x0044, 0x0045, 0x0046, 0x0047,
    0x0048, 0x0049, 0x004a, 0x004b, 0x004c, 0x004d, 0x004e, 0x004f, 0x0050,
    0x0051, 0x0052, 0x0053, 0x0054, 0x0055, 0x0056, 0x0057, 0x0058, 0x0059,
    0x005a, 0x005b, 0x005c, 0x005d, 0x005e, 0x005f, 0x0060, 0x0061, 0x0062,
    0x0063, 0x0064, 0x0065, 0x0066, 0x0067, 0x0068, 0x0069, 0x006a, 0x006b,
    0x006c, 0x006d, 0x006e, 0x006f, 0x0070, 0x0071, 0x0072, 0x0073, 0x0074,
    0x0075, 0x0076, 0x0077, 0x0078, 0x0079, 0x007a, 0x007b, 0x007c, 0x007d,
    0x007e, 0x0000, 0x2022, 0x2020, 0x2021, 0x2026, 0x2014, 0x2013, 0x0192,
    0x2044, 0x2039, 0x203a, 0x2212, 0x2030, 0x201e, 0x201c, 0x201d, 0x2018,
    0x2019, 0x201a, 0x2122, 0xfb01, 0xfb02, 0x0141, 0x0152, 0x0160, 0x0178,
    0x017d, 0x0131, 0x0142, 0x0153, 0x0161, 0x017e, 0x0000, 0x20ac, 0x00a1,
    0x00a2, 0x00a3, 0x00a4, 0x00a5, 0x00a6, 0x00a7, 0x00a8, 0x00a9, 0x00aa,
    0x00ab, 0x00ac, 0x0000, 0x00ae, 0x00af, 0x00b0, 0x00b1, 0x00b2, 0x00b3,
    0x00b4, 0x00b5, 0x00b6, 0x00b7, 0x00b8, 0x00b9, 0x00ba, 0x00bb, 0x00bc,
    0x00bd, 0x00be, 0x00bf, 0x00c0, 0x00c1, 0x00c2, 0x00c3, 0x00c4, 0x00c5,
    0x00c6, 0x00c7, 0x00c8, 0x00c9, 0x00ca, 0x00cb, 0x00cc, 0x00cd, 0x00ce,
    0x00cf, 0x00d0, 0x00d1, 0x00d2, 0x00d3, 0x00d4, 0x00d5, 0x00d6, 0x00d7,
    0x00d8, 0x00d9, 0x00da, 0x00db, 0x00dc, 0x00dd, 0x00de, 0x00df, 0x00e0,
    0x00e1, 0x00e2, 0x00e3, 0x00e4, 0x00e5, 0x00e6, 0x00e7, 0x00e8, 0x00e9,
    0x00ea, 0x00eb, 0x00ec, 0x00ed, 0x00ee, 0x00ef, 0x00f0, 0x00f1, 0x00f2,
    0x00f3, 0x00f4, 0x00f5, 0x00f6, 0x00f7, 0x00f8, 0x00f9, 0x00fa, 0x00fb,
    0x00fc, 0x00fd, 0x00fe, 0x00ff};

uint32_t A85Decode(const uint8_t* src_buf,
                   uint32_t src_size,
                   uint8_t*& dest_buf,
                   uint32_t& dest_size) {
  dest_size = 0;
  dest_buf = nullptr;
  if (src_size == 0)
    return 0;

  // Count legal characters and zeros.
  uint32_t zcount = 0;
  uint32_t pos = 0;
  while (pos < src_size) {
    uint8_t ch = src_buf[pos];
    if (ch == 'z') {
      zcount++;
    } else if ((ch < '!' || ch > 'u') && !PDFCharIsLineEnding(ch) &&
               ch != ' ' && ch != '\t') {
      break;
    }
    pos++;
  }
  // No content to decode.
  if (pos == 0)
    return 0;

  // Count the space needed to contain non-zero characters. The encoding ratio
  // of Ascii85 is 4:5.
  uint32_t space_for_non_zeroes = (pos - zcount) / 5 * 4 + 4;
  if (zcount > (UINT_MAX - space_for_non_zeroes) / 4) {
    return (uint32_t)-1;
  }
  dest_buf = FX_Alloc(uint8_t, zcount * 4 + space_for_non_zeroes);
  size_t state = 0;
  uint32_t res = 0;
  pos = dest_size = 0;
  while (pos < src_size) {
    uint8_t ch = src_buf[pos++];
    if (PDFCharIsLineEnding(ch) || ch == ' ' || ch == '\t')
      continue;

    if (ch == 'z') {
      FXSYS_memset(dest_buf + dest_size, 0, 4);
      state = 0;
      res = 0;
      dest_size += 4;
    } else if (ch >= '!' && ch <= 'u') {
      res = res * 85 + ch - 33;
      state++;
      if (state == 5) {
        for (size_t i = 0; i < 4; i++) {
          dest_buf[dest_size++] = (uint8_t)(res >> (3 - i) * 8);
        }
        state = 0;
        res = 0;
      }
    } else {
      // The end or illegal character.
      break;
    }
  }
  // Handle partial group.
  if (state) {
    for (size_t i = state; i < 5; i++)
      res = res * 85 + 84;
    for (size_t i = 0; i < state - 1; i++)
      dest_buf[dest_size++] = (uint8_t)(res >> (3 - i) * 8);
  }
  if (pos < src_size && src_buf[pos] == '>')
    pos++;
  return pos;
}

uint32_t HexDecode(const uint8_t* src_buf,
                   uint32_t src_size,
                   uint8_t*& dest_buf,
                   uint32_t& dest_size) {
  dest_size = 0;
  if (src_size == 0) {
    dest_buf = nullptr;
    return 0;
  }

  uint32_t i = 0;
  // Find the end of data.
  while (i < src_size && src_buf[i] != '>')
    i++;

  dest_buf = FX_Alloc(uint8_t, i / 2 + 1);
  bool bFirst = true;
  for (i = 0; i < src_size; i++) {
    uint8_t ch = src_buf[i];
    if (PDFCharIsLineEnding(ch) || ch == ' ' || ch == '\t')
      continue;

    if (ch == '>') {
      ++i;
      break;
    }
    if (!std::isxdigit(ch))
      continue;

    int digit = FXSYS_toHexDigit(ch);
    if (bFirst)
      dest_buf[dest_size] = digit * 16;
    else
      dest_buf[dest_size++] += digit;

    bFirst = !bFirst;
  }
  if (!bFirst)
    dest_size++;
  return i;
}

uint32_t RunLengthDecode(const uint8_t* src_buf,
                         uint32_t src_size,
                         uint8_t*& dest_buf,
                         uint32_t& dest_size) {
  uint32_t i = 0;
  dest_size = 0;
  while (i < src_size) {
    if (src_buf[i] == 128)
      break;

    uint32_t old = dest_size;
    if (src_buf[i] < 128) {
      dest_size += src_buf[i] + 1;
      if (dest_size < old)
        return FX_INVALID_OFFSET;
      i += src_buf[i] + 2;
    } else {
      dest_size += 257 - src_buf[i];
      if (dest_size < old)
        return FX_INVALID_OFFSET;
      i += 2;
    }
  }
  if (dest_size >= kMaxStreamSize)
    return FX_INVALID_OFFSET;

  dest_buf = FX_Alloc(uint8_t, dest_size);
  i = 0;
  int dest_count = 0;
  while (i < src_size) {
    if (src_buf[i] == 128)
      break;

    if (src_buf[i] < 128) {
      uint32_t copy_len = src_buf[i] + 1;
      uint32_t buf_left = src_size - i - 1;
      if (buf_left < copy_len) {
        uint32_t delta = copy_len - buf_left;
        copy_len = buf_left;
        FXSYS_memset(dest_buf + dest_count + copy_len, '\0', delta);
      }
      FXSYS_memcpy(dest_buf + dest_count, src_buf + i + 1, copy_len);
      dest_count += src_buf[i] + 1;
      i += src_buf[i] + 2;
    } else {
      int fill = 0;
      if (i < src_size - 1) {
        fill = src_buf[i + 1];
      }
      FXSYS_memset(dest_buf + dest_count, fill, 257 - src_buf[i]);
      dest_count += 257 - src_buf[i];
      i += 2;
    }
  }

  return std::min(i + 1, src_size);
}

std::unique_ptr<CCodec_ScanlineDecoder> FPDFAPI_CreateFaxDecoder(
    const uint8_t* src_buf,
    uint32_t src_size,
    int width,
    int height,
    const CPDF_Dictionary* pParams) {
  int K = 0;
  bool EndOfLine = false;
  bool ByteAlign = false;
  bool BlackIs1 = false;
  int Columns = 1728;
  int Rows = 0;
  if (pParams) {
    K = pParams->GetIntegerFor("K");
    EndOfLine = !!pParams->GetIntegerFor("EndOfLine");
    ByteAlign = !!pParams->GetIntegerFor("EncodedByteAlign");
    BlackIs1 = !!pParams->GetIntegerFor("BlackIs1");
    Columns = pParams->GetIntegerFor("Columns", 1728);
    Rows = pParams->GetIntegerFor("Rows");
    if (Rows > USHRT_MAX) {
      Rows = 0;
    }
  }
  return CPDF_ModuleMgr::Get()->GetFaxModule()->CreateDecoder(
      src_buf, src_size, width, height, K, EndOfLine, ByteAlign, BlackIs1,
      Columns, Rows);
}

std::unique_ptr<CCodec_ScanlineDecoder> FPDFAPI_CreateFlateDecoder(
    const uint8_t* src_buf,
    uint32_t src_size,
    int width,
    int height,
    int nComps,
    int bpc,
    const CPDF_Dictionary* pParams) {
  int predictor = 0;
  int Colors = 0, BitsPerComponent = 0, Columns = 0;
  if (pParams) {
    predictor = pParams->GetIntegerFor("Predictor");
    Colors = pParams->GetIntegerFor("Colors", 1);
    BitsPerComponent = pParams->GetIntegerFor("BitsPerComponent", 8);
    Columns = pParams->GetIntegerFor("Columns", 1);
    if (!CheckFlateDecodeParams(Colors, BitsPerComponent, Columns))
      return nullptr;
  }
  return CPDF_ModuleMgr::Get()->GetFlateModule()->CreateDecoder(
      src_buf, src_size, width, height, nComps, bpc, predictor, Colors,
      BitsPerComponent, Columns);
}

uint32_t FPDFAPI_FlateOrLZWDecode(bool bLZW,
                                  const uint8_t* src_buf,
                                  uint32_t src_size,
                                  CPDF_Dictionary* pParams,
                                  uint32_t estimated_size,
                                  uint8_t*& dest_buf,
                                  uint32_t& dest_size) {
  int predictor = 0;
  int Colors = 0;
  int BitsPerComponent = 0;
  int Columns = 0;
  bool bEarlyChange = true;
  if (pParams) {
    predictor = pParams->GetIntegerFor("Predictor");
    bEarlyChange = !!pParams->GetIntegerFor("EarlyChange", 1);
    Colors = pParams->GetIntegerFor("Colors", 1);
    BitsPerComponent = pParams->GetIntegerFor("BitsPerComponent", 8);
    Columns = pParams->GetIntegerFor("Columns", 1);
    if (!CheckFlateDecodeParams(Colors, BitsPerComponent, Columns))
      return (uint32_t)-1;
  }
  return CPDF_ModuleMgr::Get()->GetFlateModule()->FlateOrLZWDecode(
      bLZW, src_buf, src_size, bEarlyChange, predictor, Colors,
      BitsPerComponent, Columns, estimated_size, dest_buf, dest_size);
}

bool PDF_DataDecode(const uint8_t* src_buf,
                    uint32_t src_size,
                    const CPDF_Dictionary* pDict,
                    uint8_t*& dest_buf,
                    uint32_t& dest_size,
                    CFX_ByteString& ImageEncoding,
                    CPDF_Dictionary*& pImageParms,
                    uint32_t last_estimated_size,
                    bool bImageAcc) {
  CPDF_Object* pDecoder = pDict ? pDict->GetDirectObjectFor("Filter") : nullptr;
  if (!pDecoder || (!pDecoder->IsArray() && !pDecoder->IsName()))
    return false;

  CPDF_Object* pParams =
      pDict ? pDict->GetDirectObjectFor("DecodeParms") : nullptr;

  std::vector<std::pair<CFX_ByteString, CPDF_Object*>> DecoderArray;
  if (CPDF_Array* pDecoders = pDecoder->AsArray()) {
    CPDF_Array* pParamsArray = ToArray(pParams);
    for (size_t i = 0; i < pDecoders->GetCount(); i++) {
      DecoderArray.push_back(
          {pDecoders->GetStringAt(i),
           pParamsArray ? pParamsArray->GetDictAt(i) : nullptr});
    }
  } else {
    DecoderArray.push_back(
        {pDecoder->GetString(), pParams ? pParams->GetDict() : nullptr});
  }
  uint8_t* last_buf = const_cast<uint8_t*>(src_buf);
  uint32_t last_size = src_size;
  int nSize = pdfium::CollectionSize<int>(DecoderArray);
  for (int i = 0; i < nSize; i++) {
    int estimated_size = i == nSize - 1 ? last_estimated_size : 0;
    CFX_ByteString decoder = DecoderArray[i].first;
    CPDF_Dictionary* pParam = ToDictionary(DecoderArray[i].second);
    uint8_t* new_buf = nullptr;
    uint32_t new_size = (uint32_t)-1;
    int offset = -1;
    if (decoder == "FlateDecode" || decoder == "Fl") {
      if (bImageAcc && i == nSize - 1) {
        ImageEncoding = "FlateDecode";
        dest_buf = (uint8_t*)last_buf;
        dest_size = last_size;
        pImageParms = pParam;
        return true;
      }
      offset = FPDFAPI_FlateOrLZWDecode(false, last_buf, last_size, pParam,
                                        estimated_size, new_buf, new_size);
    } else if (decoder == "LZWDecode" || decoder == "LZW") {
      offset = FPDFAPI_FlateOrLZWDecode(true, last_buf, last_size, pParam,
                                        estimated_size, new_buf, new_size);
    } else if (decoder == "ASCII85Decode" || decoder == "A85") {
      offset = A85Decode(last_buf, last_size, new_buf, new_size);
    } else if (decoder == "ASCIIHexDecode" || decoder == "AHx") {
      offset = HexDecode(last_buf, last_size, new_buf, new_size);
    } else if (decoder == "RunLengthDecode" || decoder == "RL") {
      if (bImageAcc && i == nSize - 1) {
        ImageEncoding = "RunLengthDecode";
        dest_buf = (uint8_t*)last_buf;
        dest_size = last_size;
        pImageParms = pParam;
        return true;
      }
      offset = RunLengthDecode(last_buf, last_size, new_buf, new_size);
    } else if (decoder == "Crypt") {
      continue;
    } else {
      // If we get here, assume it's an image decoder.
      if (decoder == "DCT") {
        decoder = "DCTDecode";
      } else if (decoder == "CCF") {
        decoder = "CCITTFaxDecode";
      }
      ImageEncoding = decoder;
      pImageParms = pParam;
      dest_buf = (uint8_t*)last_buf;
      dest_size = last_size;
      if (CPDF_Array* pDecoders = pDecoder->AsArray())
        pDecoders->RemoveAt(i + 1, pDecoders->GetCount() - i - 1);
      return true;
    }
    if (last_buf != src_buf) {
      FX_Free(last_buf);
    }
    if (offset == -1) {
      FX_Free(new_buf);
      return false;
    }
    last_buf = new_buf;
    last_size = new_size;
  }
  ImageEncoding = "";
  pImageParms = nullptr;
  dest_buf = last_buf;
  dest_size = last_size;
  return true;
}

CFX_WideString PDF_DecodeText(const uint8_t* src_data, uint32_t src_len) {
  CFX_WideString result;
  if (src_len >= 2 && ((src_data[0] == 0xfe && src_data[1] == 0xff) ||
                       (src_data[0] == 0xff && src_data[1] == 0xfe))) {
    bool bBE = src_data[0] == 0xfe;
    uint32_t max_chars = (src_len - 2) / 2;
    if (!max_chars) {
      return result;
    }
    if (src_data[0] == 0xff) {
      bBE = !src_data[2];
    }
    FX_WCHAR* dest_buf = result.GetBuffer(max_chars);
    const uint8_t* uni_str = src_data + 2;
    int dest_pos = 0;
    for (uint32_t i = 0; i < max_chars * 2; i += 2) {
      uint16_t unicode = bBE ? (uni_str[i] << 8 | uni_str[i + 1])
                             : (uni_str[i + 1] << 8 | uni_str[i]);
      if (unicode == 0x1b) {
        i += 2;
        while (i < max_chars * 2) {
          uint16_t unicode2 = bBE ? (uni_str[i] << 8 | uni_str[i + 1])
                                  : (uni_str[i + 1] << 8 | uni_str[i]);
          i += 2;
          if (unicode2 == 0x1b)
            break;
        }
      } else {
        dest_buf[dest_pos++] = unicode;
      }
    }
    result.ReleaseBuffer(dest_pos);
  } else {
    FX_WCHAR* dest_buf = result.GetBuffer(src_len);
    for (uint32_t i = 0; i < src_len; i++)
      dest_buf[i] = PDFDocEncoding[src_data[i]];
    result.ReleaseBuffer(src_len);
  }
  return result;
}

CFX_WideString PDF_DecodeText(const CFX_ByteString& bstr) {
  return PDF_DecodeText((const uint8_t*)bstr.c_str(), bstr.GetLength());
}

CFX_ByteString PDF_EncodeText(const FX_WCHAR* pString, int len) {
  if (len == -1) {
    len = FXSYS_wcslen(pString);
  }
  CFX_ByteString result;
  FX_CHAR* dest_buf1 = result.GetBuffer(len);
  int i;
  for (i = 0; i < len; i++) {
    int code;
    for (code = 0; code < 256; code++)
      if (PDFDocEncoding[code] == pString[i]) {
        break;
      }
    if (code == 256) {
      break;
    }
    dest_buf1[i] = code;
  }
  result.ReleaseBuffer(i);
  if (i == len) {
    return result;
  }

  if (len > INT_MAX / 2 - 1) {
    result.ReleaseBuffer(0);
    return result;
  }

  int encLen = len * 2 + 2;

  uint8_t* dest_buf2 = (uint8_t*)result.GetBuffer(encLen);
  dest_buf2[0] = 0xfe;
  dest_buf2[1] = 0xff;
  dest_buf2 += 2;
  for (int j = 0; j < len; j++) {
    *dest_buf2++ = pString[j] >> 8;
    *dest_buf2++ = (uint8_t)pString[j];
  }
  result.ReleaseBuffer(encLen);
  return result;
}

CFX_ByteString PDF_EncodeText(const CFX_WideString& str) {
  return PDF_EncodeText(str.c_str(), str.GetLength());
}

CFX_ByteString PDF_EncodeString(const CFX_ByteString& src, bool bHex) {
  CFX_ByteTextBuf result;
  int srclen = src.GetLength();
  if (bHex) {
    result.AppendChar('<');
    for (int i = 0; i < srclen; i++) {
      result.AppendChar("0123456789ABCDEF"[src[i] / 16]);
      result.AppendChar("0123456789ABCDEF"[src[i] % 16]);
    }
    result.AppendChar('>');
    return result.MakeString();
  }
  result.AppendChar('(');
  for (int i = 0; i < srclen; i++) {
    uint8_t ch = src[i];
    if (ch == 0x0a) {
      result << "\\n";
      continue;
    }
    if (ch == 0x0d) {
      result << "\\r";
      continue;
    }
    if (ch == ')' || ch == '\\' || ch == '(')
      result.AppendChar('\\');
    result.AppendChar(ch);
  }
  result.AppendChar(')');
  return result.MakeString();
}

bool FlateEncode(const uint8_t* src_buf,
                 uint32_t src_size,
                 uint8_t** dest_buf,
                 uint32_t* dest_size) {
  CCodec_ModuleMgr* pEncoders = CPDF_ModuleMgr::Get()->GetCodecModule();
  return pEncoders &&
         pEncoders->GetFlateModule()->Encode(src_buf, src_size, dest_buf,
                                             dest_size);
}

bool PngEncode(const uint8_t* src_buf,
               uint32_t src_size,
               uint8_t** dest_buf,
               uint32_t* dest_size) {
  CCodec_ModuleMgr* pEncoders = CPDF_ModuleMgr::Get()->GetCodecModule();
  return pEncoders &&
         pEncoders->GetFlateModule()->PngEncode(src_buf, src_size, dest_buf,
                                                dest_size);
}

uint32_t FlateDecode(const uint8_t* src_buf,
                     uint32_t src_size,
                     uint8_t*& dest_buf,
                     uint32_t& dest_size) {
  CCodec_ModuleMgr* pEncoders = CPDF_ModuleMgr::Get()->GetCodecModule();
  if (pEncoders) {
    return pEncoders->GetFlateModule()->FlateOrLZWDecode(
        false, src_buf, src_size, false, 0, 0, 0, 0, 0, dest_buf, dest_size);
  }
  return 0;
}

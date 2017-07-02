// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fpdfapi/parser/fpdf_parser_utility.h"

#include "core/fpdfapi/parser/cpdf_array.h"
#include "core/fpdfapi/parser/cpdf_boolean.h"
#include "core/fpdfapi/parser/cpdf_dictionary.h"
#include "core/fpdfapi/parser/cpdf_number.h"
#include "core/fpdfapi/parser/cpdf_reference.h"
#include "core/fpdfapi/parser/cpdf_stream.h"
#include "core/fpdfapi/parser/cpdf_stream_acc.h"
#include "core/fpdfapi/parser/cpdf_string.h"
#include "core/fpdfapi/parser/fpdf_parser_decode.h"
#include "core/fxcrt/fx_ext.h"

// Indexed by 8-bit character code, contains either:
//   'W' - for whitespace: NUL, TAB, CR, LF, FF, SPACE, 0x80, 0xff
//   'N' - for numeric: 0123456789+-.
//   'D' - for delimiter: %()/<>[]{}
//   'R' - otherwise.
const char PDF_CharType[256] = {
    // NUL  SOH  STX  ETX  EOT  ENQ  ACK  BEL  BS   HT   LF   VT   FF   CR   SO
    // SI
    'W', 'R', 'R', 'R', 'R', 'R', 'R', 'R', 'R', 'W', 'W', 'R', 'W', 'W', 'R',
    'R',

    // DLE  DC1  DC2  DC3  DC4  NAK  SYN  ETB  CAN  EM   SUB  ESC  FS   GS   RS
    // US
    'R', 'R', 'R', 'R', 'R', 'R', 'R', 'R', 'R', 'R', 'R', 'R', 'R', 'R', 'R',
    'R',

    // SP    !    "    #    $    %    &    ´    (    )    *    +    ,    -    .
    // /
    'W', 'R', 'R', 'R', 'R', 'D', 'R', 'R', 'D', 'D', 'R', 'N', 'R', 'N', 'N',
    'D',

    // 0    1    2    3    4    5    6    7    8    9    :    ;    <    =    > ?
    'N', 'N', 'N', 'N', 'N', 'N', 'N', 'N', 'N', 'N', 'R', 'R', 'D', 'R', 'D',
    'R',

    // @    A    B    C    D    E    F    G    H    I    J    K    L    M    N O
    'R', 'R', 'R', 'R', 'R', 'R', 'R', 'R', 'R', 'R', 'R', 'R', 'R', 'R', 'R',
    'R',

    // P    Q    R    S    T    U    V    W    X    Y    Z    [    \    ]    ^ _
    'R', 'R', 'R', 'R', 'R', 'R', 'R', 'R', 'R', 'R', 'R', 'D', 'R', 'D', 'R',
    'R',

    // `    a    b    c    d    e    f    g    h    i    j    k    l    m    n o
    'R', 'R', 'R', 'R', 'R', 'R', 'R', 'R', 'R', 'R', 'R', 'R', 'R', 'R', 'R',
    'R',

    // p    q    r    s    t    u    v    w    x    y    z    {    |    }    ~
    // DEL
    'R', 'R', 'R', 'R', 'R', 'R', 'R', 'R', 'R', 'R', 'R', 'D', 'R', 'D', 'R',
    'R',

    'W', 'R', 'R', 'R', 'R', 'R', 'R', 'R', 'R', 'R', 'R', 'R', 'R', 'R', 'R',
    'R', 'R', 'R', 'R', 'R', 'R', 'R', 'R', 'R', 'R', 'R', 'R', 'R', 'R', 'R',
    'R', 'R', 'R', 'R', 'R', 'R', 'R', 'R', 'R', 'R', 'R', 'R', 'R', 'R', 'R',
    'R', 'R', 'R', 'R', 'R', 'R', 'R', 'R', 'R', 'R', 'R', 'R', 'R', 'R', 'R',
    'R', 'R', 'R', 'R', 'R', 'R', 'R', 'R', 'R', 'R', 'R', 'R', 'R', 'R', 'R',
    'R', 'R', 'R', 'R', 'R', 'R', 'R', 'R', 'R', 'R', 'R', 'R', 'R', 'R', 'R',
    'R', 'R', 'R', 'R', 'R', 'R', 'R', 'R', 'R', 'R', 'R', 'R', 'R', 'R', 'R',
    'R', 'R', 'R', 'R', 'R', 'R', 'R', 'R', 'R', 'R', 'R', 'R', 'R', 'R', 'R',
    'R', 'R', 'R', 'R', 'R', 'R', 'R', 'W'};

int32_t GetHeaderOffset(const CFX_RetainPtr<IFX_SeekableReadStream>& pFile) {
  const size_t kBufSize = 4;
  uint8_t buf[kBufSize];
  for (int32_t offset = 0; offset <= 1024; ++offset) {
    if (!pFile->ReadBlock(buf, offset, kBufSize))
      return -1;

    if (memcmp(buf, "%PDF", 4) == 0)
      return offset;
  }
  return -1;
}

int32_t GetDirectInteger(CPDF_Dictionary* pDict, const CFX_ByteString& key) {
  CPDF_Number* pObj = ToNumber(pDict->GetObjectFor(key));
  return pObj ? pObj->GetInteger() : 0;
}

CFX_ByteString PDF_NameDecode(const CFX_ByteStringC& bstr) {
  if (bstr.Find('#') == -1)
    return CFX_ByteString(bstr);

  int size = bstr.GetLength();
  CFX_ByteString result;
  FX_CHAR* pDestStart = result.GetBuffer(size);
  FX_CHAR* pDest = pDestStart;
  for (int i = 0; i < size; i++) {
    if (bstr[i] == '#' && i < size - 2) {
      *pDest++ =
          FXSYS_toHexDigit(bstr[i + 1]) * 16 + FXSYS_toHexDigit(bstr[i + 2]);
      i += 2;
    } else {
      *pDest++ = bstr[i];
    }
  }
  result.ReleaseBuffer((FX_STRSIZE)(pDest - pDestStart));
  return result;
}

CFX_ByteString PDF_NameDecode(const CFX_ByteString& orig) {
  if (orig.Find('#') == -1)
    return orig;
  return PDF_NameDecode(orig.AsStringC());
}

CFX_ByteString PDF_NameEncode(const CFX_ByteString& orig) {
  uint8_t* src_buf = (uint8_t*)orig.c_str();
  int src_len = orig.GetLength();
  int dest_len = 0;
  int i;
  for (i = 0; i < src_len; i++) {
    uint8_t ch = src_buf[i];
    if (ch >= 0x80 || PDFCharIsWhitespace(ch) || ch == '#' ||
        PDFCharIsDelimiter(ch)) {
      dest_len += 3;
    } else {
      dest_len++;
    }
  }
  if (dest_len == src_len)
    return orig;

  CFX_ByteString res;
  FX_CHAR* dest_buf = res.GetBuffer(dest_len);
  dest_len = 0;
  for (i = 0; i < src_len; i++) {
    uint8_t ch = src_buf[i];
    if (ch >= 0x80 || PDFCharIsWhitespace(ch) || ch == '#' ||
        PDFCharIsDelimiter(ch)) {
      dest_buf[dest_len++] = '#';
      dest_buf[dest_len++] = "0123456789ABCDEF"[ch / 16];
      dest_buf[dest_len++] = "0123456789ABCDEF"[ch % 16];
    } else {
      dest_buf[dest_len++] = ch;
    }
  }
  dest_buf[dest_len] = 0;
  res.ReleaseBuffer();
  return res;
}

CFX_ByteTextBuf& operator<<(CFX_ByteTextBuf& buf, const CPDF_Object* pObj) {
  if (!pObj) {
    buf << " null";
    return buf;
  }
  switch (pObj->GetType()) {
    case CPDF_Object::NULLOBJ:
      buf << " null";
      break;
    case CPDF_Object::BOOLEAN:
    case CPDF_Object::NUMBER:
      buf << " " << pObj->GetString();
      break;
    case CPDF_Object::STRING:
      buf << PDF_EncodeString(pObj->GetString(), pObj->AsString()->IsHex());
      break;
    case CPDF_Object::NAME: {
      CFX_ByteString str = pObj->GetString();
      buf << "/" << PDF_NameEncode(str);
      break;
    }
    case CPDF_Object::REFERENCE: {
      buf << " " << pObj->AsReference()->GetRefObjNum() << " 0 R ";
      break;
    }
    case CPDF_Object::ARRAY: {
      const CPDF_Array* p = pObj->AsArray();
      buf << "[";
      for (size_t i = 0; i < p->GetCount(); i++) {
        CPDF_Object* pElement = p->GetObjectAt(i);
        if (pElement && !pElement->IsInline()) {
          buf << " " << pElement->GetObjNum() << " 0 R";
        } else {
          buf << pElement;
        }
      }
      buf << "]";
      break;
    }
    case CPDF_Object::DICTIONARY: {
      const CPDF_Dictionary* p = pObj->AsDictionary();
      buf << "<<";
      for (const auto& it : *p) {
        const CFX_ByteString& key = it.first;
        CPDF_Object* pValue = it.second.get();
        buf << "/" << PDF_NameEncode(key);
        if (pValue && !pValue->IsInline()) {
          buf << " " << pValue->GetObjNum() << " 0 R ";
        } else {
          buf << pValue;
        }
      }
      buf << ">>";
      break;
    }
    case CPDF_Object::STREAM: {
      const CPDF_Stream* p = pObj->AsStream();
      buf << p->GetDict() << "stream\r\n";
      CPDF_StreamAcc acc;
      acc.LoadAllData(p, true);
      buf.AppendBlock(acc.GetData(), acc.GetSize());
      buf << "\r\nendstream";
      break;
    }
    default:
      ASSERT(false);
      break;
  }
  return buf;
}

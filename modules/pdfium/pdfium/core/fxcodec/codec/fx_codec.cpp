// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fxcodec/fx_codec.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <utility>

#include "core/fxcrt/fx_ext.h"
#include "core/fxcrt/fx_safe_types.h"
#include "third_party/base/logging.h"
#include "third_party/base/ptr_util.h"

CCodec_ModuleMgr::CCodec_ModuleMgr()
    : m_pBasicModule(new CCodec_BasicModule),
      m_pFaxModule(new CCodec_FaxModule),
      m_pJpegModule(new CCodec_JpegModule),
      m_pJpxModule(new CCodec_JpxModule),
      m_pJbig2Module(new CCodec_Jbig2Module),
      m_pIccModule(new CCodec_IccModule),
      m_pFlateModule(new CCodec_FlateModule) {
}

CCodec_ModuleMgr::~CCodec_ModuleMgr() {}

CCodec_ScanlineDecoder::CCodec_ScanlineDecoder()
    : CCodec_ScanlineDecoder(0, 0, 0, 0, 0, 0, 0) {}

CCodec_ScanlineDecoder::CCodec_ScanlineDecoder(int nOrigWidth,
                                               int nOrigHeight,
                                               int nOutputWidth,
                                               int nOutputHeight,
                                               int nComps,
                                               int nBpc,
                                               uint32_t nPitch)
    : m_OrigWidth(nOrigWidth),
      m_OrigHeight(nOrigHeight),
      m_OutputWidth(nOutputWidth),
      m_OutputHeight(nOutputHeight),
      m_nComps(nComps),
      m_bpc(nBpc),
      m_Pitch(nPitch),
      m_NextLine(-1),
      m_pLastScanline(nullptr) {}

CCodec_ScanlineDecoder::~CCodec_ScanlineDecoder() {}

const uint8_t* CCodec_ScanlineDecoder::GetScanline(int line) {
  if (m_NextLine == line + 1)
    return m_pLastScanline;

  if (m_NextLine < 0 || m_NextLine > line) {
    if (!v_Rewind())
      return nullptr;
    m_NextLine = 0;
  }
  while (m_NextLine < line) {
    ReadNextLine();
    m_NextLine++;
  }
  m_pLastScanline = ReadNextLine();
  m_NextLine++;
  return m_pLastScanline;
}

bool CCodec_ScanlineDecoder::SkipToScanline(int line, IFX_Pause* pPause) {
  if (m_NextLine == line || m_NextLine == line + 1)
    return false;

  if (m_NextLine < 0 || m_NextLine > line) {
    v_Rewind();
    m_NextLine = 0;
  }
  m_pLastScanline = nullptr;
  while (m_NextLine < line) {
    m_pLastScanline = ReadNextLine();
    m_NextLine++;
    if (pPause && pPause->NeedToPauseNow()) {
      return true;
    }
  }
  return false;
}

uint8_t* CCodec_ScanlineDecoder::ReadNextLine() {
  return v_GetNextLine();
}

bool CCodec_BasicModule::RunLengthEncode(const uint8_t* src_buf,
                                         uint32_t src_size,
                                         uint8_t** dest_buf,
                                         uint32_t* dest_size) {
  // Check inputs
  if (!src_buf || !dest_buf || !dest_size || src_size == 0)
    return false;

  // Edge case
  if (src_size == 1) {
    *dest_buf = FX_Alloc(uint8_t, 3);
    (*dest_buf)[0] = 0;
    (*dest_buf)[1] = src_buf[0];
    (*dest_buf)[2] = 128;
    *dest_size = 3;
    return true;
  }

  // Worst case: 1 nonmatch, 2 match, 1 nonmatch, 2 match, etc. This becomes
  // 4 output chars for every 3 input, plus up to 4 more for the 1-2 chars
  // rounded off plus the terminating character.
  uint32_t est_size = 4 * ((src_size + 2) / 3) + 1;
  *dest_buf = FX_Alloc(uint8_t, est_size);

  // Set up pointers.
  uint8_t* out = *dest_buf;
  uint32_t run_start = 0;
  uint32_t run_end = 1;
  uint8_t x = src_buf[run_start];
  uint8_t y = src_buf[run_end];
  while (run_end < src_size) {
    uint32_t max_len = std::min((uint32_t)128, src_size - run_start);
    while (x == y && (run_end - run_start < max_len - 1))
      y = src_buf[++run_end];

    // Reached end with matched run. Update variables to expected values.
    if (x == y) {
      run_end++;
      if (run_end < src_size)
        y = src_buf[run_end];
    }
    if (run_end - run_start > 1) {  // Matched run but not at end of input.
      out[0] = 257 - (run_end - run_start);
      out[1] = x;
      x = y;
      run_start = run_end;
      run_end++;
      if (run_end < src_size)
        y = src_buf[run_end];
      out += 2;
      continue;
    }
    // Mismatched run
    while (x != y && run_end <= run_start + max_len) {
      out[run_end - run_start] = x;
      x = y;
      run_end++;
      if (run_end == src_size) {
        if (run_end <= run_start + max_len) {
          out[run_end - run_start] = x;
          run_end++;
        }
        break;
      }
      y = src_buf[run_end];
    }
    out[0] = run_end - run_start - 2;
    out += run_end - run_start;
    run_start = run_end - 1;
  }
  if (run_start < src_size) {  // 1 leftover character
    out[0] = 0;
    out[1] = x;
    out += 2;
  }
  *out = 128;
  *dest_size = out + 1 - *dest_buf;
  return true;
}

bool CCodec_BasicModule::A85Encode(const uint8_t* src_buf,
                                   uint32_t src_size,
                                   uint8_t** dest_buf,
                                   uint32_t* dest_size) {
  // Check inputs.
  if (!src_buf || !dest_buf || !dest_size)
    return false;

  if (src_size == 0) {
    *dest_size = 0;
    return false;
  }

  // Worst case: 5 output for each 4 input (plus up to 4 from leftover), plus
  // 2 character new lines each 75 output chars plus 2 termination chars. May
  // have fewer if there are special "z" chars.
  uint32_t est_size = 5 * (src_size / 4) + 4 + src_size / 30 + 2;
  *dest_buf = FX_Alloc(uint8_t, est_size);

  // Set up pointers.
  uint8_t* out = *dest_buf;
  uint32_t pos = 0;
  uint32_t line_length = 0;
  while (src_size >= 4 && pos < src_size - 3) {
    uint32_t val = ((uint32_t)(src_buf[pos]) << 24) +
                   ((uint32_t)(src_buf[pos + 1]) << 16) +
                   ((uint32_t)(src_buf[pos + 2]) << 8) +
                   (uint32_t)(src_buf[pos + 3]);
    pos += 4;
    if (val == 0) {  // All zero special case
      *out = 'z';
      out++;
      line_length++;
    } else {  // Compute base 85 characters and add 33.
      for (int i = 4; i >= 0; i--) {
        out[i] = (uint8_t)(val % 85) + 33;
        val = val / 85;
      }
      out += 5;
      line_length += 5;
    }
    if (line_length >= 75) {  // Add a return.
      *out++ = '\r';
      *out++ = '\n';
      line_length = 0;
    }
  }
  if (pos < src_size) {  // Leftover bytes
    uint32_t val = 0;
    int count = 0;
    while (pos < src_size) {
      val += (uint32_t)(src_buf[pos]) << (8 * (3 - count));
      count++;
      pos++;
    }
    for (int i = 4; i >= 0; i--) {
      if (i <= count)
        out[i] = (uint8_t)(val % 85) + 33;
      val = val / 85;
    }
    out += count + 1;
  }

  // Terminating characters.
  out[0] = '~';
  out[1] = '>';
  out += 2;
  *dest_size = out - *dest_buf;
  return true;
}

#ifdef PDF_ENABLE_XFA
CFX_DIBAttribute::CFX_DIBAttribute()
    : m_nXDPI(-1),
      m_nYDPI(-1),
      m_fAspectRatio(-1.0f),
      m_wDPIUnit(0),
      m_nGifLeft(0),
      m_nGifTop(0),
      m_pGifLocalPalette(nullptr),
      m_nGifLocalPalNum(0),
      m_nBmpCompressType(0) {
  FXSYS_memset(m_strTime, 0, sizeof(m_strTime));
}
CFX_DIBAttribute::~CFX_DIBAttribute() {
  for (const auto& pair : m_Exif)
    FX_Free(pair.second);
}
#endif  // PDF_ENABLE_XFA

class CCodec_RLScanlineDecoder : public CCodec_ScanlineDecoder {
 public:
  CCodec_RLScanlineDecoder();
  ~CCodec_RLScanlineDecoder() override;

  bool Create(const uint8_t* src_buf,
              uint32_t src_size,
              int width,
              int height,
              int nComps,
              int bpc);

  // CCodec_ScanlineDecoder
  bool v_Rewind() override;
  uint8_t* v_GetNextLine() override;
  uint32_t GetSrcOffset() override { return m_SrcOffset; }

 protected:
  bool CheckDestSize();
  void GetNextOperator();
  void UpdateOperator(uint8_t used_bytes);

  uint8_t* m_pScanline;
  const uint8_t* m_pSrcBuf;
  uint32_t m_SrcSize;
  uint32_t m_dwLineBytes;
  uint32_t m_SrcOffset;
  bool m_bEOD;
  uint8_t m_Operator;
};
CCodec_RLScanlineDecoder::CCodec_RLScanlineDecoder()
    : m_pScanline(nullptr),
      m_pSrcBuf(nullptr),
      m_SrcSize(0),
      m_dwLineBytes(0),
      m_SrcOffset(0),
      m_bEOD(false),
      m_Operator(0) {}
CCodec_RLScanlineDecoder::~CCodec_RLScanlineDecoder() {
  FX_Free(m_pScanline);
}
bool CCodec_RLScanlineDecoder::CheckDestSize() {
  uint32_t i = 0;
  uint32_t old_size = 0;
  uint32_t dest_size = 0;
  while (i < m_SrcSize) {
    if (m_pSrcBuf[i] < 128) {
      old_size = dest_size;
      dest_size += m_pSrcBuf[i] + 1;
      if (dest_size < old_size) {
        return false;
      }
      i += m_pSrcBuf[i] + 2;
    } else if (m_pSrcBuf[i] > 128) {
      old_size = dest_size;
      dest_size += 257 - m_pSrcBuf[i];
      if (dest_size < old_size) {
        return false;
      }
      i += 2;
    } else {
      break;
    }
  }
  if (((uint32_t)m_OrigWidth * m_nComps * m_bpc * m_OrigHeight + 7) / 8 >
      dest_size) {
    return false;
  }
  return true;
}
bool CCodec_RLScanlineDecoder::Create(const uint8_t* src_buf,
                                      uint32_t src_size,
                                      int width,
                                      int height,
                                      int nComps,
                                      int bpc) {
  m_pSrcBuf = src_buf;
  m_SrcSize = src_size;
  m_OutputWidth = m_OrigWidth = width;
  m_OutputHeight = m_OrigHeight = height;
  m_nComps = nComps;
  m_bpc = bpc;
  // Aligning the pitch to 4 bytes requires an integer overflow check.
  FX_SAFE_UINT32 pitch = width;
  pitch *= nComps;
  pitch *= bpc;
  pitch += 31;
  pitch /= 32;
  pitch *= 4;
  if (!pitch.IsValid()) {
    return false;
  }
  m_Pitch = pitch.ValueOrDie();
  // Overflow should already have been checked before this is called.
  m_dwLineBytes = (static_cast<uint32_t>(width) * nComps * bpc + 7) / 8;
  m_pScanline = FX_Alloc(uint8_t, m_Pitch);
  return CheckDestSize();
}
bool CCodec_RLScanlineDecoder::v_Rewind() {
  FXSYS_memset(m_pScanline, 0, m_Pitch);
  m_SrcOffset = 0;
  m_bEOD = false;
  m_Operator = 0;
  return true;
}
uint8_t* CCodec_RLScanlineDecoder::v_GetNextLine() {
  if (m_SrcOffset == 0) {
    GetNextOperator();
  } else {
    if (m_bEOD) {
      return nullptr;
    }
  }
  FXSYS_memset(m_pScanline, 0, m_Pitch);
  uint32_t col_pos = 0;
  bool eol = false;
  while (m_SrcOffset < m_SrcSize && !eol) {
    if (m_Operator < 128) {
      uint32_t copy_len = m_Operator + 1;
      if (col_pos + copy_len >= m_dwLineBytes) {
        copy_len = m_dwLineBytes - col_pos;
        eol = true;
      }
      if (copy_len >= m_SrcSize - m_SrcOffset) {
        copy_len = m_SrcSize - m_SrcOffset;
        m_bEOD = true;
      }
      FXSYS_memcpy(m_pScanline + col_pos, m_pSrcBuf + m_SrcOffset, copy_len);
      col_pos += copy_len;
      UpdateOperator((uint8_t)copy_len);
    } else if (m_Operator > 128) {
      int fill = 0;
      if (m_SrcOffset - 1 < m_SrcSize - 1) {
        fill = m_pSrcBuf[m_SrcOffset];
      }
      uint32_t duplicate_len = 257 - m_Operator;
      if (col_pos + duplicate_len >= m_dwLineBytes) {
        duplicate_len = m_dwLineBytes - col_pos;
        eol = true;
      }
      FXSYS_memset(m_pScanline + col_pos, fill, duplicate_len);
      col_pos += duplicate_len;
      UpdateOperator((uint8_t)duplicate_len);
    } else {
      m_bEOD = true;
      break;
    }
  }
  return m_pScanline;
}
void CCodec_RLScanlineDecoder::GetNextOperator() {
  if (m_SrcOffset >= m_SrcSize) {
    m_Operator = 128;
    return;
  }
  m_Operator = m_pSrcBuf[m_SrcOffset];
  m_SrcOffset++;
}
void CCodec_RLScanlineDecoder::UpdateOperator(uint8_t used_bytes) {
  if (used_bytes == 0) {
    return;
  }
  if (m_Operator < 128) {
    ASSERT((uint32_t)m_Operator + 1 >= used_bytes);
    if (used_bytes == m_Operator + 1) {
      m_SrcOffset += used_bytes;
      GetNextOperator();
      return;
    }
    m_Operator -= used_bytes;
    m_SrcOffset += used_bytes;
    if (m_SrcOffset >= m_SrcSize) {
      m_Operator = 128;
    }
    return;
  }
  uint8_t count = 257 - m_Operator;
  ASSERT((uint32_t)count >= used_bytes);
  if (used_bytes == count) {
    m_SrcOffset++;
    GetNextOperator();
    return;
  }
  count -= used_bytes;
  m_Operator = 257 - count;
}

std::unique_ptr<CCodec_ScanlineDecoder>
CCodec_BasicModule::CreateRunLengthDecoder(const uint8_t* src_buf,
                                           uint32_t src_size,
                                           int width,
                                           int height,
                                           int nComps,
                                           int bpc) {
  auto pDecoder = pdfium::MakeUnique<CCodec_RLScanlineDecoder>();
  if (!pDecoder->Create(src_buf, src_size, width, height, nComps, bpc))
    return nullptr;

  return std::move(pDecoder);
}

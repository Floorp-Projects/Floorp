// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include <limits.h>

#include "core/fxcodec/jbig2/JBig2_Image.h"
#include "core/fxcrt/fx_coordinates.h"
#include "core/fxcrt/fx_safe_types.h"

namespace {

const int kMaxImagePixels = INT_MAX - 31;
const int kMaxImageBytes = kMaxImagePixels / 8;

}  // namespace

CJBig2_Image::CJBig2_Image(int32_t w, int32_t h)
    : m_pData(nullptr),
      m_nWidth(0),
      m_nHeight(0),
      m_nStride(0),
      m_bOwnsBuffer(true) {
  if (w <= 0 || h <= 0 || w > kMaxImagePixels)
    return;

  int32_t stride_pixels = (w + 31) & ~31;
  if (h > kMaxImagePixels / stride_pixels)
    return;

  m_nWidth = w;
  m_nHeight = h;
  m_nStride = stride_pixels / 8;
  m_pData = FX_Alloc2D(uint8_t, m_nStride, m_nHeight);
}

CJBig2_Image::CJBig2_Image(int32_t w, int32_t h, int32_t stride, uint8_t* pBuf)
    : m_pData(nullptr),
      m_nWidth(0),
      m_nHeight(0),
      m_nStride(0),
      m_bOwnsBuffer(false) {
  if (w < 0 || h < 0 || stride < 0 || stride > kMaxImageBytes)
    return;

  int32_t stride_pixels = 8 * stride;
  if (stride_pixels < w || h > kMaxImagePixels / stride_pixels)
    return;

  m_nWidth = w;
  m_nHeight = h;
  m_nStride = stride;
  m_pData = pBuf;
}

CJBig2_Image::CJBig2_Image(const CJBig2_Image& other)
    : m_pData(nullptr),
      m_nWidth(other.m_nWidth),
      m_nHeight(other.m_nHeight),
      m_nStride(other.m_nStride),
      m_bOwnsBuffer(true) {
  if (other.m_pData) {
    m_pData = FX_Alloc2D(uint8_t, m_nStride, m_nHeight);
    JBIG2_memcpy(m_pData, other.m_pData, m_nStride * m_nHeight);
  }
}

CJBig2_Image::~CJBig2_Image() {
  if (m_bOwnsBuffer) {
    FX_Free(m_pData);
  }
}

int CJBig2_Image::getPixel(int32_t x, int32_t y) {
  if (!m_pData)
    return 0;

  if (x < 0 || x >= m_nWidth)
    return 0;

  if (y < 0 || y >= m_nHeight)
    return 0;

  int32_t m = y * m_nStride + (x >> 3);
  int32_t n = x & 7;
  return ((m_pData[m] >> (7 - n)) & 1);
}

int32_t CJBig2_Image::setPixel(int32_t x, int32_t y, int v) {
  if (!m_pData)
    return 0;

  if (x < 0 || x >= m_nWidth)
    return 0;

  if (y < 0 || y >= m_nHeight)
    return 0;

  int32_t m = y * m_nStride + (x >> 3);
  int32_t n = x & 7;
  if (v)
    m_pData[m] |= 1 << (7 - n);
  else
    m_pData[m] &= ~(1 << (7 - n));

  return 1;
}

void CJBig2_Image::copyLine(int32_t hTo, int32_t hFrom) {
  if (!m_pData) {
    return;
  }
  if (hFrom < 0 || hFrom >= m_nHeight) {
    JBIG2_memset(m_pData + hTo * m_nStride, 0, m_nStride);
  } else {
    JBIG2_memcpy(m_pData + hTo * m_nStride, m_pData + hFrom * m_nStride,
                 m_nStride);
  }
}
void CJBig2_Image::fill(bool v) {
  if (!m_pData) {
    return;
  }
  JBIG2_memset(m_pData, v ? 0xff : 0, m_nStride * m_nHeight);
}
bool CJBig2_Image::composeTo(CJBig2_Image* pDst,
                             int32_t x,
                             int32_t y,
                             JBig2ComposeOp op) {
  if (!m_pData) {
    return false;
  }
  return composeTo_opt2(pDst, x, y, op);
}
bool CJBig2_Image::composeTo(CJBig2_Image* pDst,
                             int32_t x,
                             int32_t y,
                             JBig2ComposeOp op,
                             const FX_RECT* pSrcRect) {
  if (!m_pData)
    return false;

  if (!pSrcRect || *pSrcRect == FX_RECT(0, 0, m_nWidth, m_nHeight))
    return composeTo_opt2(pDst, x, y, op);

  return composeTo_opt2(pDst, x, y, op, pSrcRect);
}

bool CJBig2_Image::composeFrom(int32_t x,
                               int32_t y,
                               CJBig2_Image* pSrc,
                               JBig2ComposeOp op) {
  if (!m_pData) {
    return false;
  }
  return pSrc->composeTo(this, x, y, op);
}
bool CJBig2_Image::composeFrom(int32_t x,
                               int32_t y,
                               CJBig2_Image* pSrc,
                               JBig2ComposeOp op,
                               const FX_RECT* pSrcRect) {
  if (!m_pData) {
    return false;
  }
  return pSrc->composeTo(this, x, y, op, pSrcRect);
}
#define JBIG2_GETDWORD(buf) \
  ((uint32_t)(((buf)[0] << 24) | ((buf)[1] << 16) | ((buf)[2] << 8) | (buf)[3]))
CJBig2_Image* CJBig2_Image::subImage(int32_t x,
                                     int32_t y,
                                     int32_t w,
                                     int32_t h) {
  int32_t m, n, j;
  uint8_t *pLineSrc, *pLineDst;
  uint32_t wTmp;
  uint8_t *pSrc, *pSrcEnd, *pDst, *pDstEnd;
  if (w == 0 || h == 0) {
    return nullptr;
  }
  CJBig2_Image* pImage = new CJBig2_Image(w, h);
  if (!m_pData) {
    pImage->fill(0);
    return pImage;
  }
  if (!pImage->m_pData) {
    return pImage;
  }
  pLineSrc = m_pData + m_nStride * y;
  pLineDst = pImage->m_pData;
  m = (x >> 5) << 2;
  n = x & 31;
  if (n == 0) {
    for (j = 0; j < h; j++) {
      pSrc = pLineSrc + m;
      pSrcEnd = pLineSrc + m_nStride;
      pDst = pLineDst;
      pDstEnd = pLineDst + pImage->m_nStride;
      for (; pDst < pDstEnd; pSrc += 4, pDst += 4) {
        *((uint32_t*)pDst) = *((uint32_t*)pSrc);
      }
      pLineSrc += m_nStride;
      pLineDst += pImage->m_nStride;
    }
  } else {
    for (j = 0; j < h; j++) {
      pSrc = pLineSrc + m;
      pSrcEnd = pLineSrc + m_nStride;
      pDst = pLineDst;
      pDstEnd = pLineDst + pImage->m_nStride;
      for (; pDst < pDstEnd; pSrc += 4, pDst += 4) {
        if (pSrc + 4 < pSrcEnd) {
          wTmp = (JBIG2_GETDWORD(pSrc) << n) |
                 (JBIG2_GETDWORD(pSrc + 4) >> (32 - n));
        } else {
          wTmp = JBIG2_GETDWORD(pSrc) << n;
        }
        pDst[0] = (uint8_t)(wTmp >> 24);
        pDst[1] = (uint8_t)(wTmp >> 16);
        pDst[2] = (uint8_t)(wTmp >> 8);
        pDst[3] = (uint8_t)wTmp;
      }
      pLineSrc += m_nStride;
      pLineDst += pImage->m_nStride;
    }
  }
  return pImage;
}

void CJBig2_Image::expand(int32_t h, bool v) {
  if (!m_pData || h <= m_nHeight || h > kMaxImageBytes / m_nStride)
    return;

  if (m_bOwnsBuffer) {
    m_pData = FX_Realloc(uint8_t, m_pData, h * m_nStride);
  } else {
    uint8_t* pExternalBuffer = m_pData;
    m_pData = FX_Alloc(uint8_t, h * m_nStride);
    JBIG2_memcpy(m_pData, pExternalBuffer, m_nHeight * m_nStride);
    m_bOwnsBuffer = true;
  }
  JBIG2_memset(m_pData + m_nHeight * m_nStride, v ? 0xff : 0,
               (h - m_nHeight) * m_nStride);
  m_nHeight = h;
}

bool CJBig2_Image::composeTo_opt2(CJBig2_Image* pDst,
                                  int32_t x,
                                  int32_t y,
                                  JBig2ComposeOp op) {
  int32_t xs0 = 0, ys0 = 0, xs1 = 0, ys1 = 0, xd0 = 0, yd0 = 0, xd1 = 0,
          yd1 = 0, xx = 0, yy = 0, w = 0, h = 0, middleDwords = 0, lineLeft = 0;

  uint32_t s1 = 0, d1 = 0, d2 = 0, shift = 0, shift1 = 0, shift2 = 0, tmp = 0,
           tmp1 = 0, tmp2 = 0, maskL = 0, maskR = 0, maskM = 0;

  if (!m_pData)
    return false;

  if (x < -1048576 || x > 1048576 || y < -1048576 || y > 1048576)
    return false;

  if (y < 0) {
    ys0 = -y;
  }
  if (y + m_nHeight > pDst->m_nHeight) {
    ys1 = pDst->m_nHeight - y;
  } else {
    ys1 = m_nHeight;
  }
  if (x < 0) {
    xs0 = -x;
  }
  if (x + m_nWidth > pDst->m_nWidth) {
    xs1 = pDst->m_nWidth - x;
  } else {
    xs1 = m_nWidth;
  }
  if ((ys0 >= ys1) || (xs0 >= xs1)) {
    return 0;
  }
  w = xs1 - xs0;
  h = ys1 - ys0;
  if (y >= 0) {
    yd0 = y;
  }
  if (x >= 0) {
    xd0 = x;
  }
  xd1 = xd0 + w;
  yd1 = yd0 + h;
  d1 = xd0 & 31;
  d2 = xd1 & 31;
  s1 = xs0 & 31;
  maskL = 0xffffffff >> d1;
  maskR = 0xffffffff << ((32 - (xd1 & 31)) % 32);
  maskM = maskL & maskR;
  uint8_t* lineSrc = m_pData + ys0 * m_nStride + ((xs0 >> 5) << 2);
  lineLeft = m_nStride - ((xs0 >> 5) << 2);
  uint8_t* lineDst = pDst->m_pData + yd0 * pDst->m_nStride + ((xd0 >> 5) << 2);
  if ((xd0 & ~31) == ((xd1 - 1) & ~31)) {
    if ((xs0 & ~31) == ((xs1 - 1) & ~31)) {
      if (s1 > d1) {
        shift = s1 - d1;
        for (yy = yd0; yy < yd1; yy++) {
          tmp1 = JBIG2_GETDWORD(lineSrc) << shift;
          tmp2 = JBIG2_GETDWORD(lineDst);
          switch (op) {
            case JBIG2_COMPOSE_OR:
              tmp = (tmp2 & ~maskM) | ((tmp1 | tmp2) & maskM);
              break;
            case JBIG2_COMPOSE_AND:
              tmp = (tmp2 & ~maskM) | ((tmp1 & tmp2) & maskM);
              break;
            case JBIG2_COMPOSE_XOR:
              tmp = (tmp2 & ~maskM) | ((tmp1 ^ tmp2) & maskM);
              break;
            case JBIG2_COMPOSE_XNOR:
              tmp = (tmp2 & ~maskM) | ((~(tmp1 ^ tmp2)) & maskM);
              break;
            case JBIG2_COMPOSE_REPLACE:
              tmp = (tmp2 & ~maskM) | (tmp1 & maskM);
              break;
          }
          lineDst[0] = (uint8_t)(tmp >> 24);
          lineDst[1] = (uint8_t)(tmp >> 16);
          lineDst[2] = (uint8_t)(tmp >> 8);
          lineDst[3] = (uint8_t)tmp;
          lineSrc += m_nStride;
          lineDst += pDst->m_nStride;
        }
      } else {
        shift = d1 - s1;
        for (yy = yd0; yy < yd1; yy++) {
          tmp1 = JBIG2_GETDWORD(lineSrc) >> shift;
          tmp2 = JBIG2_GETDWORD(lineDst);
          switch (op) {
            case JBIG2_COMPOSE_OR:
              tmp = (tmp2 & ~maskM) | ((tmp1 | tmp2) & maskM);
              break;
            case JBIG2_COMPOSE_AND:
              tmp = (tmp2 & ~maskM) | ((tmp1 & tmp2) & maskM);
              break;
            case JBIG2_COMPOSE_XOR:
              tmp = (tmp2 & ~maskM) | ((tmp1 ^ tmp2) & maskM);
              break;
            case JBIG2_COMPOSE_XNOR:
              tmp = (tmp2 & ~maskM) | ((~(tmp1 ^ tmp2)) & maskM);
              break;
            case JBIG2_COMPOSE_REPLACE:
              tmp = (tmp2 & ~maskM) | (tmp1 & maskM);
              break;
          }
          lineDst[0] = (uint8_t)(tmp >> 24);
          lineDst[1] = (uint8_t)(tmp >> 16);
          lineDst[2] = (uint8_t)(tmp >> 8);
          lineDst[3] = (uint8_t)tmp;
          lineSrc += m_nStride;
          lineDst += pDst->m_nStride;
        }
      }
    } else {
      shift1 = s1 - d1;
      shift2 = 32 - shift1;
      for (yy = yd0; yy < yd1; yy++) {
        tmp1 = (JBIG2_GETDWORD(lineSrc) << shift1) |
               (JBIG2_GETDWORD(lineSrc + 4) >> shift2);
        tmp2 = JBIG2_GETDWORD(lineDst);
        switch (op) {
          case JBIG2_COMPOSE_OR:
            tmp = (tmp2 & ~maskM) | ((tmp1 | tmp2) & maskM);
            break;
          case JBIG2_COMPOSE_AND:
            tmp = (tmp2 & ~maskM) | ((tmp1 & tmp2) & maskM);
            break;
          case JBIG2_COMPOSE_XOR:
            tmp = (tmp2 & ~maskM) | ((tmp1 ^ tmp2) & maskM);
            break;
          case JBIG2_COMPOSE_XNOR:
            tmp = (tmp2 & ~maskM) | ((~(tmp1 ^ tmp2)) & maskM);
            break;
          case JBIG2_COMPOSE_REPLACE:
            tmp = (tmp2 & ~maskM) | (tmp1 & maskM);
            break;
        }
        lineDst[0] = (uint8_t)(tmp >> 24);
        lineDst[1] = (uint8_t)(tmp >> 16);
        lineDst[2] = (uint8_t)(tmp >> 8);
        lineDst[3] = (uint8_t)tmp;
        lineSrc += m_nStride;
        lineDst += pDst->m_nStride;
      }
    }
  } else {
    uint8_t* sp = nullptr;
    uint8_t* dp = nullptr;

    if (s1 > d1) {
      shift1 = s1 - d1;
      shift2 = 32 - shift1;
      middleDwords = (xd1 >> 5) - ((xd0 + 31) >> 5);
      for (yy = yd0; yy < yd1; yy++) {
        sp = lineSrc;
        dp = lineDst;
        if (d1 != 0) {
          tmp1 = (JBIG2_GETDWORD(sp) << shift1) |
                 (JBIG2_GETDWORD(sp + 4) >> shift2);
          tmp2 = JBIG2_GETDWORD(dp);
          switch (op) {
            case JBIG2_COMPOSE_OR:
              tmp = (tmp2 & ~maskL) | ((tmp1 | tmp2) & maskL);
              break;
            case JBIG2_COMPOSE_AND:
              tmp = (tmp2 & ~maskL) | ((tmp1 & tmp2) & maskL);
              break;
            case JBIG2_COMPOSE_XOR:
              tmp = (tmp2 & ~maskL) | ((tmp1 ^ tmp2) & maskL);
              break;
            case JBIG2_COMPOSE_XNOR:
              tmp = (tmp2 & ~maskL) | ((~(tmp1 ^ tmp2)) & maskL);
              break;
            case JBIG2_COMPOSE_REPLACE:
              tmp = (tmp2 & ~maskL) | (tmp1 & maskL);
              break;
          }
          dp[0] = (uint8_t)(tmp >> 24);
          dp[1] = (uint8_t)(tmp >> 16);
          dp[2] = (uint8_t)(tmp >> 8);
          dp[3] = (uint8_t)tmp;
          sp += 4;
          dp += 4;
        }
        for (xx = 0; xx < middleDwords; xx++) {
          tmp1 = (JBIG2_GETDWORD(sp) << shift1) |
                 (JBIG2_GETDWORD(sp + 4) >> shift2);
          tmp2 = JBIG2_GETDWORD(dp);
          switch (op) {
            case JBIG2_COMPOSE_OR:
              tmp = tmp1 | tmp2;
              break;
            case JBIG2_COMPOSE_AND:
              tmp = tmp1 & tmp2;
              break;
            case JBIG2_COMPOSE_XOR:
              tmp = tmp1 ^ tmp2;
              break;
            case JBIG2_COMPOSE_XNOR:
              tmp = ~(tmp1 ^ tmp2);
              break;
            case JBIG2_COMPOSE_REPLACE:
              tmp = tmp1;
              break;
          }
          dp[0] = (uint8_t)(tmp >> 24);
          dp[1] = (uint8_t)(tmp >> 16);
          dp[2] = (uint8_t)(tmp >> 8);
          dp[3] = (uint8_t)tmp;
          sp += 4;
          dp += 4;
        }
        if (d2 != 0) {
          tmp1 =
              (JBIG2_GETDWORD(sp) << shift1) |
              (((sp + 4) < lineSrc + lineLeft ? JBIG2_GETDWORD(sp + 4) : 0) >>
               shift2);
          tmp2 = JBIG2_GETDWORD(dp);
          switch (op) {
            case JBIG2_COMPOSE_OR:
              tmp = (tmp2 & ~maskR) | ((tmp1 | tmp2) & maskR);
              break;
            case JBIG2_COMPOSE_AND:
              tmp = (tmp2 & ~maskR) | ((tmp1 & tmp2) & maskR);
              break;
            case JBIG2_COMPOSE_XOR:
              tmp = (tmp2 & ~maskR) | ((tmp1 ^ tmp2) & maskR);
              break;
            case JBIG2_COMPOSE_XNOR:
              tmp = (tmp2 & ~maskR) | ((~(tmp1 ^ tmp2)) & maskR);
              break;
            case JBIG2_COMPOSE_REPLACE:
              tmp = (tmp2 & ~maskR) | (tmp1 & maskR);
              break;
          }
          dp[0] = (uint8_t)(tmp >> 24);
          dp[1] = (uint8_t)(tmp >> 16);
          dp[2] = (uint8_t)(tmp >> 8);
          dp[3] = (uint8_t)tmp;
        }
        lineSrc += m_nStride;
        lineDst += pDst->m_nStride;
      }
    } else if (s1 == d1) {
      middleDwords = (xd1 >> 5) - ((xd0 + 31) >> 5);
      for (yy = yd0; yy < yd1; yy++) {
        sp = lineSrc;
        dp = lineDst;
        if (d1 != 0) {
          tmp1 = JBIG2_GETDWORD(sp);
          tmp2 = JBIG2_GETDWORD(dp);
          switch (op) {
            case JBIG2_COMPOSE_OR:
              tmp = (tmp2 & ~maskL) | ((tmp1 | tmp2) & maskL);
              break;
            case JBIG2_COMPOSE_AND:
              tmp = (tmp2 & ~maskL) | ((tmp1 & tmp2) & maskL);
              break;
            case JBIG2_COMPOSE_XOR:
              tmp = (tmp2 & ~maskL) | ((tmp1 ^ tmp2) & maskL);
              break;
            case JBIG2_COMPOSE_XNOR:
              tmp = (tmp2 & ~maskL) | ((~(tmp1 ^ tmp2)) & maskL);
              break;
            case JBIG2_COMPOSE_REPLACE:
              tmp = (tmp2 & ~maskL) | (tmp1 & maskL);
              break;
          }
          dp[0] = (uint8_t)(tmp >> 24);
          dp[1] = (uint8_t)(tmp >> 16);
          dp[2] = (uint8_t)(tmp >> 8);
          dp[3] = (uint8_t)tmp;
          sp += 4;
          dp += 4;
        }
        for (xx = 0; xx < middleDwords; xx++) {
          tmp1 = JBIG2_GETDWORD(sp);
          tmp2 = JBIG2_GETDWORD(dp);
          switch (op) {
            case JBIG2_COMPOSE_OR:
              tmp = tmp1 | tmp2;
              break;
            case JBIG2_COMPOSE_AND:
              tmp = tmp1 & tmp2;
              break;
            case JBIG2_COMPOSE_XOR:
              tmp = tmp1 ^ tmp2;
              break;
            case JBIG2_COMPOSE_XNOR:
              tmp = ~(tmp1 ^ tmp2);
              break;
            case JBIG2_COMPOSE_REPLACE:
              tmp = tmp1;
              break;
          }
          dp[0] = (uint8_t)(tmp >> 24);
          dp[1] = (uint8_t)(tmp >> 16);
          dp[2] = (uint8_t)(tmp >> 8);
          dp[3] = (uint8_t)tmp;
          sp += 4;
          dp += 4;
        }
        if (d2 != 0) {
          tmp1 = JBIG2_GETDWORD(sp);
          tmp2 = JBIG2_GETDWORD(dp);
          switch (op) {
            case JBIG2_COMPOSE_OR:
              tmp = (tmp2 & ~maskR) | ((tmp1 | tmp2) & maskR);
              break;
            case JBIG2_COMPOSE_AND:
              tmp = (tmp2 & ~maskR) | ((tmp1 & tmp2) & maskR);
              break;
            case JBIG2_COMPOSE_XOR:
              tmp = (tmp2 & ~maskR) | ((tmp1 ^ tmp2) & maskR);
              break;
            case JBIG2_COMPOSE_XNOR:
              tmp = (tmp2 & ~maskR) | ((~(tmp1 ^ tmp2)) & maskR);
              break;
            case JBIG2_COMPOSE_REPLACE:
              tmp = (tmp2 & ~maskR) | (tmp1 & maskR);
              break;
          }
          dp[0] = (uint8_t)(tmp >> 24);
          dp[1] = (uint8_t)(tmp >> 16);
          dp[2] = (uint8_t)(tmp >> 8);
          dp[3] = (uint8_t)tmp;
        }
        lineSrc += m_nStride;
        lineDst += pDst->m_nStride;
      }
    } else {
      shift1 = d1 - s1;
      shift2 = 32 - shift1;
      middleDwords = (xd1 >> 5) - ((xd0 + 31) >> 5);
      for (yy = yd0; yy < yd1; yy++) {
        sp = lineSrc;
        dp = lineDst;
        if (d1 != 0) {
          tmp1 = JBIG2_GETDWORD(sp) >> shift1;
          tmp2 = JBIG2_GETDWORD(dp);
          switch (op) {
            case JBIG2_COMPOSE_OR:
              tmp = (tmp2 & ~maskL) | ((tmp1 | tmp2) & maskL);
              break;
            case JBIG2_COMPOSE_AND:
              tmp = (tmp2 & ~maskL) | ((tmp1 & tmp2) & maskL);
              break;
            case JBIG2_COMPOSE_XOR:
              tmp = (tmp2 & ~maskL) | ((tmp1 ^ tmp2) & maskL);
              break;
            case JBIG2_COMPOSE_XNOR:
              tmp = (tmp2 & ~maskL) | ((~(tmp1 ^ tmp2)) & maskL);
              break;
            case JBIG2_COMPOSE_REPLACE:
              tmp = (tmp2 & ~maskL) | (tmp1 & maskL);
              break;
          }
          dp[0] = (uint8_t)(tmp >> 24);
          dp[1] = (uint8_t)(tmp >> 16);
          dp[2] = (uint8_t)(tmp >> 8);
          dp[3] = (uint8_t)tmp;
          dp += 4;
        }
        for (xx = 0; xx < middleDwords; xx++) {
          tmp1 = (JBIG2_GETDWORD(sp) << shift2) |
                 ((JBIG2_GETDWORD(sp + 4)) >> shift1);
          tmp2 = JBIG2_GETDWORD(dp);
          switch (op) {
            case JBIG2_COMPOSE_OR:
              tmp = tmp1 | tmp2;
              break;
            case JBIG2_COMPOSE_AND:
              tmp = tmp1 & tmp2;
              break;
            case JBIG2_COMPOSE_XOR:
              tmp = tmp1 ^ tmp2;
              break;
            case JBIG2_COMPOSE_XNOR:
              tmp = ~(tmp1 ^ tmp2);
              break;
            case JBIG2_COMPOSE_REPLACE:
              tmp = tmp1;
              break;
          }
          dp[0] = (uint8_t)(tmp >> 24);
          dp[1] = (uint8_t)(tmp >> 16);
          dp[2] = (uint8_t)(tmp >> 8);
          dp[3] = (uint8_t)tmp;
          sp += 4;
          dp += 4;
        }
        if (d2 != 0) {
          tmp1 =
              (JBIG2_GETDWORD(sp) << shift2) |
              (((sp + 4) < lineSrc + lineLeft ? JBIG2_GETDWORD(sp + 4) : 0) >>
               shift1);
          tmp2 = JBIG2_GETDWORD(dp);
          switch (op) {
            case JBIG2_COMPOSE_OR:
              tmp = (tmp2 & ~maskR) | ((tmp1 | tmp2) & maskR);
              break;
            case JBIG2_COMPOSE_AND:
              tmp = (tmp2 & ~maskR) | ((tmp1 & tmp2) & maskR);
              break;
            case JBIG2_COMPOSE_XOR:
              tmp = (tmp2 & ~maskR) | ((tmp1 ^ tmp2) & maskR);
              break;
            case JBIG2_COMPOSE_XNOR:
              tmp = (tmp2 & ~maskR) | ((~(tmp1 ^ tmp2)) & maskR);
              break;
            case JBIG2_COMPOSE_REPLACE:
              tmp = (tmp2 & ~maskR) | (tmp1 & maskR);
              break;
          }
          dp[0] = (uint8_t)(tmp >> 24);
          dp[1] = (uint8_t)(tmp >> 16);
          dp[2] = (uint8_t)(tmp >> 8);
          dp[3] = (uint8_t)tmp;
        }
        lineSrc += m_nStride;
        lineDst += pDst->m_nStride;
      }
    }
  }
  return 1;
}
bool CJBig2_Image::composeTo_opt2(CJBig2_Image* pDst,
                                  int32_t x,
                                  int32_t y,
                                  JBig2ComposeOp op,
                                  const FX_RECT* pSrcRect) {
  if (!m_pData) {
    return false;
  }
  // TODO(weili): Check whether the range check is correct. Should x>=1048576?
  if (x < -1048576 || x > 1048576 || y < -1048576 || y > 1048576) {
    return false;
  }
  int32_t sw = pSrcRect->Width();
  int32_t sh = pSrcRect->Height();
  int32_t ys0 = y < 0 ? -y : 0;
  int32_t ys1 = y + sh > pDst->m_nHeight ? pDst->m_nHeight - y : sh;
  int32_t xs0 = x < 0 ? -x : 0;
  int32_t xs1 = x + sw > pDst->m_nWidth ? pDst->m_nWidth - x : sw;
  if ((ys0 >= ys1) || (xs0 >= xs1)) {
    return 0;
  }
  int32_t w = xs1 - xs0;
  int32_t h = ys1 - ys0;
  int32_t yd0 = y < 0 ? 0 : y;
  int32_t xd0 = x < 0 ? 0 : x;
  int32_t xd1 = xd0 + w;
  int32_t yd1 = yd0 + h;
  int32_t d1 = xd0 & 31;
  int32_t d2 = xd1 & 31;
  int32_t s1 = xs0 & 31;
  int32_t maskL = 0xffffffff >> d1;
  int32_t maskR = 0xffffffff << ((32 - (xd1 & 31)) % 32);
  int32_t maskM = maskL & maskR;
  uint8_t* lineSrc = m_pData + (pSrcRect->top + ys0) * m_nStride +
                     (((xs0 + pSrcRect->left) >> 5) << 2);
  int32_t lineLeft = m_nStride - ((xs0 >> 5) << 2);
  uint8_t* lineDst = pDst->m_pData + yd0 * pDst->m_nStride + ((xd0 >> 5) << 2);
  if ((xd0 & ~31) == ((xd1 - 1) & ~31)) {
    if ((xs0 & ~31) == ((xs1 - 1) & ~31)) {
      if (s1 > d1) {
        uint32_t shift = s1 - d1;
        for (int32_t yy = yd0; yy < yd1; yy++) {
          uint32_t tmp1 = JBIG2_GETDWORD(lineSrc) << shift;
          uint32_t tmp2 = JBIG2_GETDWORD(lineDst);
          uint32_t tmp = 0;
          switch (op) {
            case JBIG2_COMPOSE_OR:
              tmp = (tmp2 & ~maskM) | ((tmp1 | tmp2) & maskM);
              break;
            case JBIG2_COMPOSE_AND:
              tmp = (tmp2 & ~maskM) | ((tmp1 & tmp2) & maskM);
              break;
            case JBIG2_COMPOSE_XOR:
              tmp = (tmp2 & ~maskM) | ((tmp1 ^ tmp2) & maskM);
              break;
            case JBIG2_COMPOSE_XNOR:
              tmp = (tmp2 & ~maskM) | ((~(tmp1 ^ tmp2)) & maskM);
              break;
            case JBIG2_COMPOSE_REPLACE:
              tmp = (tmp2 & ~maskM) | (tmp1 & maskM);
              break;
          }
          lineDst[0] = (uint8_t)(tmp >> 24);
          lineDst[1] = (uint8_t)(tmp >> 16);
          lineDst[2] = (uint8_t)(tmp >> 8);
          lineDst[3] = (uint8_t)tmp;
          lineSrc += m_nStride;
          lineDst += pDst->m_nStride;
        }
      } else {
        uint32_t shift = d1 - s1;
        for (int32_t yy = yd0; yy < yd1; yy++) {
          uint32_t tmp1 = JBIG2_GETDWORD(lineSrc) >> shift;
          uint32_t tmp2 = JBIG2_GETDWORD(lineDst);
          uint32_t tmp = 0;
          switch (op) {
            case JBIG2_COMPOSE_OR:
              tmp = (tmp2 & ~maskM) | ((tmp1 | tmp2) & maskM);
              break;
            case JBIG2_COMPOSE_AND:
              tmp = (tmp2 & ~maskM) | ((tmp1 & tmp2) & maskM);
              break;
            case JBIG2_COMPOSE_XOR:
              tmp = (tmp2 & ~maskM) | ((tmp1 ^ tmp2) & maskM);
              break;
            case JBIG2_COMPOSE_XNOR:
              tmp = (tmp2 & ~maskM) | ((~(tmp1 ^ tmp2)) & maskM);
              break;
            case JBIG2_COMPOSE_REPLACE:
              tmp = (tmp2 & ~maskM) | (tmp1 & maskM);
              break;
          }
          lineDst[0] = (uint8_t)(tmp >> 24);
          lineDst[1] = (uint8_t)(tmp >> 16);
          lineDst[2] = (uint8_t)(tmp >> 8);
          lineDst[3] = (uint8_t)tmp;
          lineSrc += m_nStride;
          lineDst += pDst->m_nStride;
        }
      }
    } else {
      uint32_t shift1 = s1 - d1;
      uint32_t shift2 = 32 - shift1;
      for (int32_t yy = yd0; yy < yd1; yy++) {
        uint32_t tmp1 = (JBIG2_GETDWORD(lineSrc) << shift1) |
                        (JBIG2_GETDWORD(lineSrc + 4) >> shift2);
        uint32_t tmp2 = JBIG2_GETDWORD(lineDst);
        uint32_t tmp = 0;
        switch (op) {
          case JBIG2_COMPOSE_OR:
            tmp = (tmp2 & ~maskM) | ((tmp1 | tmp2) & maskM);
            break;
          case JBIG2_COMPOSE_AND:
            tmp = (tmp2 & ~maskM) | ((tmp1 & tmp2) & maskM);
            break;
          case JBIG2_COMPOSE_XOR:
            tmp = (tmp2 & ~maskM) | ((tmp1 ^ tmp2) & maskM);
            break;
          case JBIG2_COMPOSE_XNOR:
            tmp = (tmp2 & ~maskM) | ((~(tmp1 ^ tmp2)) & maskM);
            break;
          case JBIG2_COMPOSE_REPLACE:
            tmp = (tmp2 & ~maskM) | (tmp1 & maskM);
            break;
        }
        lineDst[0] = (uint8_t)(tmp >> 24);
        lineDst[1] = (uint8_t)(tmp >> 16);
        lineDst[2] = (uint8_t)(tmp >> 8);
        lineDst[3] = (uint8_t)tmp;
        lineSrc += m_nStride;
        lineDst += pDst->m_nStride;
      }
    }
  } else {
    if (s1 > d1) {
      uint32_t shift1 = s1 - d1;
      uint32_t shift2 = 32 - shift1;
      int32_t middleDwords = (xd1 >> 5) - ((xd0 + 31) >> 5);
      for (int32_t yy = yd0; yy < yd1; yy++) {
        uint8_t* sp = lineSrc;
        uint8_t* dp = lineDst;
        if (d1 != 0) {
          uint32_t tmp1 = (JBIG2_GETDWORD(sp) << shift1) |
                          (JBIG2_GETDWORD(sp + 4) >> shift2);
          uint32_t tmp2 = JBIG2_GETDWORD(dp);
          uint32_t tmp = 0;
          switch (op) {
            case JBIG2_COMPOSE_OR:
              tmp = (tmp2 & ~maskL) | ((tmp1 | tmp2) & maskL);
              break;
            case JBIG2_COMPOSE_AND:
              tmp = (tmp2 & ~maskL) | ((tmp1 & tmp2) & maskL);
              break;
            case JBIG2_COMPOSE_XOR:
              tmp = (tmp2 & ~maskL) | ((tmp1 ^ tmp2) & maskL);
              break;
            case JBIG2_COMPOSE_XNOR:
              tmp = (tmp2 & ~maskL) | ((~(tmp1 ^ tmp2)) & maskL);
              break;
            case JBIG2_COMPOSE_REPLACE:
              tmp = (tmp2 & ~maskL) | (tmp1 & maskL);
              break;
          }
          dp[0] = (uint8_t)(tmp >> 24);
          dp[1] = (uint8_t)(tmp >> 16);
          dp[2] = (uint8_t)(tmp >> 8);
          dp[3] = (uint8_t)tmp;
          sp += 4;
          dp += 4;
        }
        for (int32_t xx = 0; xx < middleDwords; xx++) {
          uint32_t tmp1 = (JBIG2_GETDWORD(sp) << shift1) |
                          (JBIG2_GETDWORD(sp + 4) >> shift2);
          uint32_t tmp2 = JBIG2_GETDWORD(dp);
          uint32_t tmp = 0;
          switch (op) {
            case JBIG2_COMPOSE_OR:
              tmp = tmp1 | tmp2;
              break;
            case JBIG2_COMPOSE_AND:
              tmp = tmp1 & tmp2;
              break;
            case JBIG2_COMPOSE_XOR:
              tmp = tmp1 ^ tmp2;
              break;
            case JBIG2_COMPOSE_XNOR:
              tmp = ~(tmp1 ^ tmp2);
              break;
            case JBIG2_COMPOSE_REPLACE:
              tmp = tmp1;
              break;
          }
          dp[0] = (uint8_t)(tmp >> 24);
          dp[1] = (uint8_t)(tmp >> 16);
          dp[2] = (uint8_t)(tmp >> 8);
          dp[3] = (uint8_t)tmp;
          sp += 4;
          dp += 4;
        }
        if (d2 != 0) {
          uint32_t tmp1 =
              (JBIG2_GETDWORD(sp) << shift1) |
              (((sp + 4) < lineSrc + lineLeft ? JBIG2_GETDWORD(sp + 4) : 0) >>
               shift2);
          uint32_t tmp2 = JBIG2_GETDWORD(dp);
          uint32_t tmp = 0;
          switch (op) {
            case JBIG2_COMPOSE_OR:
              tmp = (tmp2 & ~maskR) | ((tmp1 | tmp2) & maskR);
              break;
            case JBIG2_COMPOSE_AND:
              tmp = (tmp2 & ~maskR) | ((tmp1 & tmp2) & maskR);
              break;
            case JBIG2_COMPOSE_XOR:
              tmp = (tmp2 & ~maskR) | ((tmp1 ^ tmp2) & maskR);
              break;
            case JBIG2_COMPOSE_XNOR:
              tmp = (tmp2 & ~maskR) | ((~(tmp1 ^ tmp2)) & maskR);
              break;
            case JBIG2_COMPOSE_REPLACE:
              tmp = (tmp2 & ~maskR) | (tmp1 & maskR);
              break;
          }
          dp[0] = (uint8_t)(tmp >> 24);
          dp[1] = (uint8_t)(tmp >> 16);
          dp[2] = (uint8_t)(tmp >> 8);
          dp[3] = (uint8_t)tmp;
        }
        lineSrc += m_nStride;
        lineDst += pDst->m_nStride;
      }
    } else if (s1 == d1) {
      int32_t middleDwords = (xd1 >> 5) - ((xd0 + 31) >> 5);
      for (int32_t yy = yd0; yy < yd1; yy++) {
        uint8_t* sp = lineSrc;
        uint8_t* dp = lineDst;
        if (d1 != 0) {
          uint32_t tmp1 = JBIG2_GETDWORD(sp);
          uint32_t tmp2 = JBIG2_GETDWORD(dp);
          uint32_t tmp = 0;
          switch (op) {
            case JBIG2_COMPOSE_OR:
              tmp = (tmp2 & ~maskL) | ((tmp1 | tmp2) & maskL);
              break;
            case JBIG2_COMPOSE_AND:
              tmp = (tmp2 & ~maskL) | ((tmp1 & tmp2) & maskL);
              break;
            case JBIG2_COMPOSE_XOR:
              tmp = (tmp2 & ~maskL) | ((tmp1 ^ tmp2) & maskL);
              break;
            case JBIG2_COMPOSE_XNOR:
              tmp = (tmp2 & ~maskL) | ((~(tmp1 ^ tmp2)) & maskL);
              break;
            case JBIG2_COMPOSE_REPLACE:
              tmp = (tmp2 & ~maskL) | (tmp1 & maskL);
              break;
          }
          dp[0] = (uint8_t)(tmp >> 24);
          dp[1] = (uint8_t)(tmp >> 16);
          dp[2] = (uint8_t)(tmp >> 8);
          dp[3] = (uint8_t)tmp;
          sp += 4;
          dp += 4;
        }
        for (int32_t xx = 0; xx < middleDwords; xx++) {
          uint32_t tmp1 = JBIG2_GETDWORD(sp);
          uint32_t tmp2 = JBIG2_GETDWORD(dp);
          uint32_t tmp = 0;
          switch (op) {
            case JBIG2_COMPOSE_OR:
              tmp = tmp1 | tmp2;
              break;
            case JBIG2_COMPOSE_AND:
              tmp = tmp1 & tmp2;
              break;
            case JBIG2_COMPOSE_XOR:
              tmp = tmp1 ^ tmp2;
              break;
            case JBIG2_COMPOSE_XNOR:
              tmp = ~(tmp1 ^ tmp2);
              break;
            case JBIG2_COMPOSE_REPLACE:
              tmp = tmp1;
              break;
          }
          dp[0] = (uint8_t)(tmp >> 24);
          dp[1] = (uint8_t)(tmp >> 16);
          dp[2] = (uint8_t)(tmp >> 8);
          dp[3] = (uint8_t)tmp;
          sp += 4;
          dp += 4;
        }
        if (d2 != 0) {
          uint32_t tmp1 = JBIG2_GETDWORD(sp);
          uint32_t tmp2 = JBIG2_GETDWORD(dp);
          uint32_t tmp = 0;
          switch (op) {
            case JBIG2_COMPOSE_OR:
              tmp = (tmp2 & ~maskR) | ((tmp1 | tmp2) & maskR);
              break;
            case JBIG2_COMPOSE_AND:
              tmp = (tmp2 & ~maskR) | ((tmp1 & tmp2) & maskR);
              break;
            case JBIG2_COMPOSE_XOR:
              tmp = (tmp2 & ~maskR) | ((tmp1 ^ tmp2) & maskR);
              break;
            case JBIG2_COMPOSE_XNOR:
              tmp = (tmp2 & ~maskR) | ((~(tmp1 ^ tmp2)) & maskR);
              break;
            case JBIG2_COMPOSE_REPLACE:
              tmp = (tmp2 & ~maskR) | (tmp1 & maskR);
              break;
          }
          dp[0] = (uint8_t)(tmp >> 24);
          dp[1] = (uint8_t)(tmp >> 16);
          dp[2] = (uint8_t)(tmp >> 8);
          dp[3] = (uint8_t)tmp;
        }
        lineSrc += m_nStride;
        lineDst += pDst->m_nStride;
      }
    } else {
      uint32_t shift1 = d1 - s1;
      uint32_t shift2 = 32 - shift1;
      int32_t middleDwords = (xd1 >> 5) - ((xd0 + 31) >> 5);
      for (int32_t yy = yd0; yy < yd1; yy++) {
        uint8_t* sp = lineSrc;
        uint8_t* dp = lineDst;
        if (d1 != 0) {
          uint32_t tmp1 = JBIG2_GETDWORD(sp) >> shift1;
          uint32_t tmp2 = JBIG2_GETDWORD(dp);
          uint32_t tmp = 0;
          switch (op) {
            case JBIG2_COMPOSE_OR:
              tmp = (tmp2 & ~maskL) | ((tmp1 | tmp2) & maskL);
              break;
            case JBIG2_COMPOSE_AND:
              tmp = (tmp2 & ~maskL) | ((tmp1 & tmp2) & maskL);
              break;
            case JBIG2_COMPOSE_XOR:
              tmp = (tmp2 & ~maskL) | ((tmp1 ^ tmp2) & maskL);
              break;
            case JBIG2_COMPOSE_XNOR:
              tmp = (tmp2 & ~maskL) | ((~(tmp1 ^ tmp2)) & maskL);
              break;
            case JBIG2_COMPOSE_REPLACE:
              tmp = (tmp2 & ~maskL) | (tmp1 & maskL);
              break;
          }
          dp[0] = (uint8_t)(tmp >> 24);
          dp[1] = (uint8_t)(tmp >> 16);
          dp[2] = (uint8_t)(tmp >> 8);
          dp[3] = (uint8_t)tmp;
          dp += 4;
        }
        for (int32_t xx = 0; xx < middleDwords; xx++) {
          uint32_t tmp1 = (JBIG2_GETDWORD(sp) << shift2) |
                          ((JBIG2_GETDWORD(sp + 4)) >> shift1);
          uint32_t tmp2 = JBIG2_GETDWORD(dp);
          uint32_t tmp = 0;
          switch (op) {
            case JBIG2_COMPOSE_OR:
              tmp = tmp1 | tmp2;
              break;
            case JBIG2_COMPOSE_AND:
              tmp = tmp1 & tmp2;
              break;
            case JBIG2_COMPOSE_XOR:
              tmp = tmp1 ^ tmp2;
              break;
            case JBIG2_COMPOSE_XNOR:
              tmp = ~(tmp1 ^ tmp2);
              break;
            case JBIG2_COMPOSE_REPLACE:
              tmp = tmp1;
              break;
          }
          dp[0] = (uint8_t)(tmp >> 24);
          dp[1] = (uint8_t)(tmp >> 16);
          dp[2] = (uint8_t)(tmp >> 8);
          dp[3] = (uint8_t)tmp;
          sp += 4;
          dp += 4;
        }
        if (d2 != 0) {
          uint32_t tmp1 =
              (JBIG2_GETDWORD(sp) << shift2) |
              (((sp + 4) < lineSrc + lineLeft ? JBIG2_GETDWORD(sp + 4) : 0) >>
               shift1);
          uint32_t tmp2 = JBIG2_GETDWORD(dp);
          uint32_t tmp = 0;
          switch (op) {
            case JBIG2_COMPOSE_OR:
              tmp = (tmp2 & ~maskR) | ((tmp1 | tmp2) & maskR);
              break;
            case JBIG2_COMPOSE_AND:
              tmp = (tmp2 & ~maskR) | ((tmp1 & tmp2) & maskR);
              break;
            case JBIG2_COMPOSE_XOR:
              tmp = (tmp2 & ~maskR) | ((tmp1 ^ tmp2) & maskR);
              break;
            case JBIG2_COMPOSE_XNOR:
              tmp = (tmp2 & ~maskR) | ((~(tmp1 ^ tmp2)) & maskR);
              break;
            case JBIG2_COMPOSE_REPLACE:
              tmp = (tmp2 & ~maskR) | (tmp1 & maskR);
              break;
          }
          dp[0] = (uint8_t)(tmp >> 24);
          dp[1] = (uint8_t)(tmp >> 16);
          dp[2] = (uint8_t)(tmp >> 8);
          dp[3] = (uint8_t)tmp;
        }
        lineSrc += m_nStride;
        lineDst += pDst->m_nStride;
      }
    }
  }
  return 1;
}

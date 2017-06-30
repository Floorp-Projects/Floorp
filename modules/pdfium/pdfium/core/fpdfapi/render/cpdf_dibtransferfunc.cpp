// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fpdfapi/render/cpdf_dibtransferfunc.h"

#include <vector>

#include "core/fpdfapi/parser/cpdf_dictionary.h"
#include "core/fpdfapi/render/cpdf_transferfunc.h"

CPDF_DIBTransferFunc::CPDF_DIBTransferFunc(
    const CPDF_TransferFunc* pTransferFunc) {
  m_RampR = pTransferFunc->m_Samples;
  m_RampG = &pTransferFunc->m_Samples[256];
  m_RampB = &pTransferFunc->m_Samples[512];
}

CPDF_DIBTransferFunc::~CPDF_DIBTransferFunc() {}

FXDIB_Format CPDF_DIBTransferFunc::GetDestFormat() {
  if (m_pSrc->IsAlphaMask())
    return FXDIB_8bppMask;

#if _FXM_PLATFORM_ == _FXM_PLATFORM_APPLE_
  return (m_pSrc->HasAlpha()) ? FXDIB_Argb : FXDIB_Rgb32;
#else
  return (m_pSrc->HasAlpha()) ? FXDIB_Argb : FXDIB_Rgb;
#endif
}

FX_ARGB* CPDF_DIBTransferFunc::GetDestPalette() {
  return nullptr;
}

void CPDF_DIBTransferFunc::TranslateScanline(
    const uint8_t* src_buf,
    std::vector<uint8_t>* dest_buf) const {
  bool bSkip = false;
  switch (m_pSrc->GetFormat()) {
    case FXDIB_1bppRgb: {
      int r0 = m_RampR[0];
      int g0 = m_RampG[0];
      int b0 = m_RampB[0];
      int r1 = m_RampR[255];
      int g1 = m_RampG[255];
      int b1 = m_RampB[255];
      int index = 0;
      for (int i = 0; i < m_Width; i++) {
        if (src_buf[i / 8] & (1 << (7 - i % 8))) {
          (*dest_buf)[index++] = b1;
          (*dest_buf)[index++] = g1;
          (*dest_buf)[index++] = r1;
        } else {
          (*dest_buf)[index++] = b0;
          (*dest_buf)[index++] = g0;
          (*dest_buf)[index++] = r0;
        }
#if _FXM_PLATFORM_ == _FXM_PLATFORM_APPLE_
        index++;
#endif
      }
      break;
    }
    case FXDIB_1bppMask: {
      int m0 = m_RampR[0];
      int m1 = m_RampR[255];
      int index = 0;
      for (int i = 0; i < m_Width; i++) {
        if (src_buf[i / 8] & (1 << (7 - i % 8)))
          (*dest_buf)[index++] = m1;
        else
          (*dest_buf)[index++] = m0;
      }
      break;
    }
    case FXDIB_8bppRgb: {
      FX_ARGB* pPal = m_pSrc->GetPalette();
      int index = 0;
      for (int i = 0; i < m_Width; i++) {
        if (pPal) {
          FX_ARGB src_argb = pPal[*src_buf];
          (*dest_buf)[index++] = m_RampB[FXARGB_R(src_argb)];
          (*dest_buf)[index++] = m_RampG[FXARGB_G(src_argb)];
          (*dest_buf)[index++] = m_RampR[FXARGB_B(src_argb)];
        } else {
          uint32_t src_byte = *src_buf;
          (*dest_buf)[index++] = m_RampB[src_byte];
          (*dest_buf)[index++] = m_RampG[src_byte];
          (*dest_buf)[index++] = m_RampR[src_byte];
        }
        src_buf++;
#if _FXM_PLATFORM_ == _FXM_PLATFORM_APPLE_
        index++;
#endif
      }
      break;
    }
    case FXDIB_8bppMask: {
      int index = 0;
      for (int i = 0; i < m_Width; i++)
        (*dest_buf)[index++] = m_RampR[*(src_buf++)];
      break;
    }
    case FXDIB_Rgb: {
      int index = 0;
      for (int i = 0; i < m_Width; i++) {
        (*dest_buf)[index++] = m_RampB[*(src_buf++)];
        (*dest_buf)[index++] = m_RampG[*(src_buf++)];
        (*dest_buf)[index++] = m_RampR[*(src_buf++)];
#if _FXM_PLATFORM_ == _FXM_PLATFORM_APPLE_
        index++;
#endif
      }
      break;
    }
    case FXDIB_Rgb32:
      bSkip = true;
    case FXDIB_Argb: {
      int index = 0;
      for (int i = 0; i < m_Width; i++) {
        (*dest_buf)[index++] = m_RampB[*(src_buf++)];
        (*dest_buf)[index++] = m_RampG[*(src_buf++)];
        (*dest_buf)[index++] = m_RampR[*(src_buf++)];
        if (!bSkip) {
          (*dest_buf)[index++] = *src_buf;
#if _FXM_PLATFORM_ == _FXM_PLATFORM_APPLE_
        } else {
          index++;
#endif
        }
        src_buf++;
      }
      break;
    }
    default:
      break;
  }
}

void CPDF_DIBTransferFunc::TranslateDownSamples(uint8_t* dest_buf,
                                                const uint8_t* src_buf,
                                                int pixels,
                                                int Bpp) const {
  if (Bpp == 8) {
    for (int i = 0; i < pixels; i++)
      *dest_buf++ = m_RampR[*(src_buf++)];
  } else if (Bpp == 24) {
    for (int i = 0; i < pixels; i++) {
      *dest_buf++ = m_RampB[*(src_buf++)];
      *dest_buf++ = m_RampG[*(src_buf++)];
      *dest_buf++ = m_RampR[*(src_buf++)];
    }
  } else {
#if _FXM_PLATFORM_ == _FXM_PLATFORM_APPLE_
    if (!m_pSrc->HasAlpha()) {
      for (int i = 0; i < pixels; i++) {
        *dest_buf++ = m_RampB[*(src_buf++)];
        *dest_buf++ = m_RampG[*(src_buf++)];
        *dest_buf++ = m_RampR[*(src_buf++)];
        dest_buf++;
        src_buf++;
      }
    } else {
#endif
      for (int i = 0; i < pixels; i++) {
        *dest_buf++ = m_RampB[*(src_buf++)];
        *dest_buf++ = m_RampG[*(src_buf++)];
        *dest_buf++ = m_RampR[*(src_buf++)];
        *dest_buf++ = *(src_buf++);
      }
#if _FXM_PLATFORM_ == _FXM_PLATFORM_APPLE_
    }
#endif
  }
}

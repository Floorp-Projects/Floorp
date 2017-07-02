// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FXGE_DIB_DIB_INT_H_
#define CORE_FXGE_DIB_DIB_INT_H_

#include <stdint.h>

#include "core/fxcrt/fx_coordinates.h"
#include "core/fxge/fx_dib.h"

class IFX_ScanlineComposer;

extern const int16_t SDP_Table[513];

class CPDF_FixedMatrix {
 public:
  CPDF_FixedMatrix(const CFX_Matrix& src, int bits) {
    base = 1 << bits;
    a = FXSYS_round(src.a * base);
    b = FXSYS_round(src.b * base);
    c = FXSYS_round(src.c * base);
    d = FXSYS_round(src.d * base);
    e = FXSYS_round(src.e * base);
    f = FXSYS_round(src.f * base);
  }
  inline void Transform(int x, int y, int& x1, int& y1) {
    x1 = (a * x + c * y + e + base / 2) / base;
    y1 = (b * x + d * y + f + base / 2) / base;
  }
  int a, b, c, d, e, f;
  int base;
};
#define FPDF_HUGE_IMAGE_SIZE 60000000
struct PixelWeight {
  int m_SrcStart;
  int m_SrcEnd;
  int m_Weights[1];
};

class CWeightTable {
 public:
  CWeightTable();
  ~CWeightTable();

  bool Calc(int dest_len,
            int dest_min,
            int dest_max,
            int src_len,
            int src_min,
            int src_max,
            int flags);
  PixelWeight* GetPixelWeight(int pixel) const;
  int* GetValueFromPixelWeight(PixelWeight* pWeight, int index) const;
  size_t GetPixelWeightSize() const;

 private:
  int m_DestMin;
  int m_ItemSize;
  uint8_t* m_pWeightTables;
  size_t m_dwWeightTablesSize;
};

class CStretchEngine {
 public:
  CStretchEngine(IFX_ScanlineComposer* pDestBitmap,
                 FXDIB_Format dest_format,
                 int dest_width,
                 int dest_height,
                 const FX_RECT& clip_rect,
                 const CFX_DIBSource* pSrcBitmap,
                 int flags);
  ~CStretchEngine();

  bool Continue(IFX_Pause* pPause);

  bool StartStretchHorz();
  bool ContinueStretchHorz(IFX_Pause* pPause);
  void StretchVert();

  FXDIB_Format m_DestFormat;
  int m_DestBpp;
  int m_SrcBpp;
  int m_bHasAlpha;
  IFX_ScanlineComposer* m_pDestBitmap;
  int m_DestWidth, m_DestHeight;
  FX_RECT m_DestClip;
  uint8_t* m_pDestScanline;
  uint8_t* m_pDestMaskScanline;
  FX_RECT m_SrcClip;
  const CFX_DIBSource* m_pSource;
  uint32_t* m_pSrcPalette;
  int m_SrcWidth;
  int m_SrcHeight;
  int m_SrcPitch;
  int m_InterPitch;
  int m_ExtraMaskPitch;
  uint8_t* m_pInterBuf;
  uint8_t* m_pExtraAlphaBuf;
  int m_TransMethod;
  int m_Flags;
  CWeightTable m_WeightTable;
  int m_CurRow;
  int m_State;
};

FX_RECT FXDIB_SwapClipBox(FX_RECT& clip,
                          int width,
                          int height,
                          bool bFlipX,
                          bool bFlipY);

#endif  // CORE_FXGE_DIB_DIB_INT_H_

// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FXGE_GE_CFX_CLIPRGN_H_
#define CORE_FXGE_GE_CFX_CLIPRGN_H_

#include "core/fxge/fx_dib.h"

class CFX_ClipRgn {
 public:
  enum ClipType { RectI, MaskF };

  CFX_ClipRgn(int device_width, int device_height);
  CFX_ClipRgn(const CFX_ClipRgn& src);
  ~CFX_ClipRgn();

  ClipType GetType() const { return m_Type; }
  const FX_RECT& GetBox() const { return m_Box; }
  CFX_DIBitmapRef GetMask() const { return m_Mask; }

  void Reset(const FX_RECT& rect);
  void IntersectRect(const FX_RECT& rect);
  void IntersectMaskF(int left, int top, CFX_DIBitmapRef Mask);

 private:
  void IntersectMaskRect(FX_RECT rect, FX_RECT mask_box, CFX_DIBitmapRef Mask);

  ClipType m_Type;
  FX_RECT m_Box;
  CFX_DIBitmapRef m_Mask;
};

#endif  // CORE_FXGE_GE_CFX_CLIPRGN_H_

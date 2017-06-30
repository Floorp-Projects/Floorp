// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fxge/ge/cfx_cliprgn.h"

CFX_ClipRgn::CFX_ClipRgn(int width, int height)
    : m_Type(RectI), m_Box(0, 0, width, height) {}

CFX_ClipRgn::CFX_ClipRgn(const CFX_ClipRgn& src) {
  m_Type = src.m_Type;
  m_Box = src.m_Box;
  m_Mask = src.m_Mask;
}

CFX_ClipRgn::~CFX_ClipRgn() {}

void CFX_ClipRgn::Reset(const FX_RECT& rect) {
  m_Type = RectI;
  m_Box = rect;
  m_Mask.SetNull();
}

void CFX_ClipRgn::IntersectRect(const FX_RECT& rect) {
  if (m_Type == RectI) {
    m_Box.Intersect(rect);
    return;
  }
  if (m_Type == MaskF) {
    IntersectMaskRect(rect, m_Box, m_Mask);
    return;
  }
}

void CFX_ClipRgn::IntersectMaskRect(FX_RECT rect,
                                    FX_RECT mask_rect,
                                    CFX_DIBitmapRef Mask) {
  const CFX_DIBitmap* mask_dib = Mask.GetObject();
  m_Type = MaskF;
  m_Box = rect;
  m_Box.Intersect(mask_rect);
  if (m_Box.IsEmpty()) {
    m_Type = RectI;
    return;
  }
  if (m_Box == mask_rect) {
    m_Mask = Mask;
    return;
  }
  CFX_DIBitmap* new_dib = m_Mask.Emplace();
  new_dib->Create(m_Box.Width(), m_Box.Height(), FXDIB_8bppMask);
  for (int row = m_Box.top; row < m_Box.bottom; row++) {
    uint8_t* dest_scan =
        new_dib->GetBuffer() + new_dib->GetPitch() * (row - m_Box.top);
    uint8_t* src_scan =
        mask_dib->GetBuffer() + mask_dib->GetPitch() * (row - mask_rect.top);
    for (int col = m_Box.left; col < m_Box.right; col++)
      dest_scan[col - m_Box.left] = src_scan[col - mask_rect.left];
  }
}

void CFX_ClipRgn::IntersectMaskF(int left, int top, CFX_DIBitmapRef Mask) {
  const CFX_DIBitmap* mask_dib = Mask.GetObject();
  ASSERT(mask_dib->GetFormat() == FXDIB_8bppMask);
  FX_RECT mask_box(left, top, left + mask_dib->GetWidth(),
                   top + mask_dib->GetHeight());
  if (m_Type == RectI) {
    IntersectMaskRect(m_Box, mask_box, Mask);
    return;
  }
  if (m_Type == MaskF) {
    FX_RECT new_box = m_Box;
    new_box.Intersect(mask_box);
    if (new_box.IsEmpty()) {
      m_Type = RectI;
      m_Mask.SetNull();
      m_Box = new_box;
      return;
    }
    CFX_DIBitmapRef new_mask;
    CFX_DIBitmap* new_dib = new_mask.Emplace();
    new_dib->Create(new_box.Width(), new_box.Height(), FXDIB_8bppMask);
    const CFX_DIBitmap* old_dib = m_Mask.GetObject();
    for (int row = new_box.top; row < new_box.bottom; row++) {
      uint8_t* old_scan =
          old_dib->GetBuffer() + (row - m_Box.top) * old_dib->GetPitch();
      uint8_t* mask_scan =
          mask_dib->GetBuffer() + (row - top) * mask_dib->GetPitch();
      uint8_t* new_scan =
          new_dib->GetBuffer() + (row - new_box.top) * new_dib->GetPitch();
      for (int col = new_box.left; col < new_box.right; col++) {
        new_scan[col - new_box.left] =
            old_scan[col - m_Box.left] * mask_scan[col - left] / 255;
      }
    }
    m_Box = new_box;
    m_Mask = new_mask;
    return;
  }
  ASSERT(false);
}

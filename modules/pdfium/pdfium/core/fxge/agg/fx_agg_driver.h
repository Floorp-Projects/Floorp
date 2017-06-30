// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FXGE_AGG_FX_AGG_DRIVER_H_
#define CORE_FXGE_AGG_FX_AGG_DRIVER_H_

#include <memory>
#include <vector>

#include "core/fxge/ifx_renderdevicedriver.h"
#include "third_party/agg23/agg_clip_liang_barsky.h"
#include "third_party/agg23/agg_path_storage.h"
#include "third_party/agg23/agg_rasterizer_scanline_aa.h"

class CFX_ClipRgn;
class CFX_GraphStateData;
class CFX_Matrix;
class CFX_PathData;

class CAgg_PathData {
 public:
  CAgg_PathData() {}
  ~CAgg_PathData() {}
  void BuildPath(const CFX_PathData* pPathData,
                 const CFX_Matrix* pObject2Device);

  agg::path_storage m_PathData;
};

class CFX_AggDeviceDriver : public IFX_RenderDeviceDriver {
 public:
  CFX_AggDeviceDriver(CFX_DIBitmap* pBitmap,
                      bool bRgbByteOrder,
                      CFX_DIBitmap* pOriDevice,
                      bool bGroupKnockout);
  ~CFX_AggDeviceDriver() override;

  void InitPlatform();
  void DestroyPlatform();

  // IFX_RenderDeviceDriver
  int GetDeviceCaps(int caps_id) const override;
  void SaveState() override;
  void RestoreState(bool bKeepSaved) override;
  bool SetClip_PathFill(const CFX_PathData* pPathData,
                        const CFX_Matrix* pObject2Device,
                        int fill_mode) override;
  bool SetClip_PathStroke(const CFX_PathData* pPathData,
                          const CFX_Matrix* pObject2Device,
                          const CFX_GraphStateData* pGraphState) override;
  bool DrawPath(const CFX_PathData* pPathData,
                const CFX_Matrix* pObject2Device,
                const CFX_GraphStateData* pGraphState,
                uint32_t fill_color,
                uint32_t stroke_color,
                int fill_mode,
                int blend_type) override;
  bool SetPixel(int x, int y, uint32_t color) override;
  bool FillRectWithBlend(const FX_RECT* pRect,
                         uint32_t fill_color,
                         int blend_type) override;
  bool GetClipBox(FX_RECT* pRect) override;
  bool GetDIBits(CFX_DIBitmap* pBitmap, int left, int top) override;
  CFX_DIBitmap* GetBackDrop() override;
  bool SetDIBits(const CFX_DIBSource* pBitmap,
                 uint32_t color,
                 const FX_RECT* pSrcRect,
                 int left,
                 int top,
                 int blend_type) override;
  bool StretchDIBits(const CFX_DIBSource* pBitmap,
                     uint32_t color,
                     int dest_left,
                     int dest_top,
                     int dest_width,
                     int dest_height,
                     const FX_RECT* pClipRect,
                     uint32_t flags,
                     int blend_type) override;
  bool StartDIBits(const CFX_DIBSource* pBitmap,
                   int bitmap_alpha,
                   uint32_t color,
                   const CFX_Matrix* pMatrix,
                   uint32_t flags,
                   void*& handle,
                   int blend_type) override;
  bool ContinueDIBits(void* handle, IFX_Pause* pPause) override;
  void CancelDIBits(void* handle) override;
  bool DrawDeviceText(int nChars,
                      const FXTEXT_CHARPOS* pCharPos,
                      CFX_Font* pFont,
                      const CFX_Matrix* pObject2Device,
                      FX_FLOAT font_size,
                      uint32_t color) override;
  int GetDriverType() const override;

  bool RenderRasterizer(agg::rasterizer_scanline_aa& rasterizer,
                        uint32_t color,
                        bool bFullCover,
                        bool bGroupKnockout,
                        int alpha_flag,
                        void* pIccTransform);

  void SetClipMask(agg::rasterizer_scanline_aa& rasterizer);

  virtual uint8_t* GetBuffer() const;

 private:
  CFX_DIBitmap* m_pBitmap;
  std::unique_ptr<CFX_ClipRgn> m_pClipRgn;
  std::vector<std::unique_ptr<CFX_ClipRgn>> m_StateStack;
#if _FXM_PLATFORM_ == _FXM_PLATFORM_APPLE_
  void* m_pPlatformGraphics;
#endif
  int m_FillFlags;
  bool m_bRgbByteOrder;
  CFX_DIBitmap* m_pOriDevice;
  bool m_bGroupKnockout;
};

#endif  // CORE_FXGE_AGG_FX_AGG_DRIVER_H_

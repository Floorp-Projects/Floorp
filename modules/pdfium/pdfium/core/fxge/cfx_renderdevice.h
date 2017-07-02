// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FXGE_CFX_RENDERDEVICE_H_
#define CORE_FXGE_CFX_RENDERDEVICE_H_

#include <memory>

#include "core/fxge/cfx_gemodule.h"
#include "core/fxge/fx_dib.h"
#include "core/fxge/fx_font.h"

class CFX_Font;
class CFX_GraphStateData;
class IFX_RenderDeviceDriver;

#define FXDC_DEVICE_CLASS 1
#define FXDC_PIXEL_WIDTH 2
#define FXDC_PIXEL_HEIGHT 3
#define FXDC_BITS_PIXEL 4
#define FXDC_HORZ_SIZE 5
#define FXDC_VERT_SIZE 6
#define FXDC_RENDER_CAPS 7
#define FXDC_DISPLAY 1
#define FXDC_PRINTER 2

#define FXRC_GET_BITS 0x01
#define FXRC_BIT_MASK 0x02
#define FXRC_ALPHA_MASK 0x04
#define FXRC_ALPHA_PATH 0x10
#define FXRC_ALPHA_IMAGE 0x20
#define FXRC_ALPHA_OUTPUT 0x40
#define FXRC_BLEND_MODE 0x80
#define FXRC_SOFT_CLIP 0x100
#define FXRC_CMYK_OUTPUT 0x200
#define FXRC_BITMASK_OUTPUT 0x400
#define FXRC_BYTEMASK_OUTPUT 0x800
#define FXRENDER_IMAGE_LOSSY 0x1000
#define FXRC_FILLSTROKE_PATH 0x2000
#define FXRC_SHADING 0x4000

#define FXFILL_ALTERNATE 1
#define FXFILL_WINDING 2
#define FXFILL_FULLCOVER 4
#define FXFILL_RECT_AA 8
#define FX_FILL_STROKE 16
#define FX_STROKE_ADJUST 32
#define FX_STROKE_TEXT_MODE 64
#define FX_FILL_TEXT_MODE 128
#define FX_ZEROAREA_FILL 256
#define FXFILL_NOPATHSMOOTH 512

#define FXTEXT_CLEARTYPE 0x01
#define FXTEXT_BGR_STRIPE 0x02
#define FXTEXT_PRINTGRAPHICTEXT 0x04
#define FXTEXT_NO_NATIVETEXT 0x08
#define FXTEXT_PRINTIMAGETEXT 0x10
#define FXTEXT_NOSMOOTH 0x20

enum class FXPT_TYPE : uint8_t { LineTo, BezierTo, MoveTo };

class FXTEXT_CHARPOS {
 public:
  FXTEXT_CHARPOS();
  FXTEXT_CHARPOS(const FXTEXT_CHARPOS&);
  ~FXTEXT_CHARPOS();

  FX_FLOAT m_AdjustMatrix[4];
  CFX_PointF m_Origin;
  uint32_t m_GlyphIndex;
  int32_t m_FontCharWidth;
#if _FXM_PLATFORM_ == _FXM_PLATFORM_APPLE_
  uint32_t m_ExtGID;
#endif
  int32_t m_FallbackFontPosition;
  bool m_bGlyphAdjust;
  bool m_bFontStyle;
};

class CFX_RenderDevice {
 public:
  CFX_RenderDevice();
  virtual ~CFX_RenderDevice();

  // Take ownership of |pDriver|.
  void SetDeviceDriver(std::unique_ptr<IFX_RenderDeviceDriver> pDriver);
  IFX_RenderDeviceDriver* GetDeviceDriver() const {
    return m_pDeviceDriver.get();
  }

  void SaveState();
  void RestoreState(bool bKeepSaved);

  int GetWidth() const { return m_Width; }
  int GetHeight() const { return m_Height; }
  int GetDeviceClass() const { return m_DeviceClass; }
  int GetRenderCaps() const { return m_RenderCaps; }
  int GetDeviceCaps(int id) const;
  CFX_Matrix GetCTM() const;
  CFX_DIBitmap* GetBitmap() const { return m_pBitmap; }
  void SetBitmap(CFX_DIBitmap* pBitmap) { m_pBitmap = pBitmap; }
  bool CreateCompatibleBitmap(CFX_DIBitmap* pDIB, int width, int height) const;
  const FX_RECT& GetClipBox() const { return m_ClipBox; }
  bool SetClip_PathFill(const CFX_PathData* pPathData,
                        const CFX_Matrix* pObject2Device,
                        int fill_mode);
  bool SetClip_Rect(const FX_RECT& pRect);
  bool SetClip_PathStroke(const CFX_PathData* pPathData,
                          const CFX_Matrix* pObject2Device,
                          const CFX_GraphStateData* pGraphState);
  bool DrawPath(const CFX_PathData* pPathData,
                const CFX_Matrix* pObject2Device,
                const CFX_GraphStateData* pGraphState,
                uint32_t fill_color,
                uint32_t stroke_color,
                int fill_mode) {
    return DrawPathWithBlend(pPathData, pObject2Device, pGraphState, fill_color,
                             stroke_color, fill_mode, FXDIB_BLEND_NORMAL);
  }
  bool DrawPathWithBlend(const CFX_PathData* pPathData,
                         const CFX_Matrix* pObject2Device,
                         const CFX_GraphStateData* pGraphState,
                         uint32_t fill_color,
                         uint32_t stroke_color,
                         int fill_mode,
                         int blend_type);
  bool SetPixel(int x, int y, uint32_t color);
  bool FillRect(const FX_RECT* pRect, uint32_t color) {
    return FillRectWithBlend(pRect, color, FXDIB_BLEND_NORMAL);
  }
  bool FillRectWithBlend(const FX_RECT* pRect, uint32_t color, int blend_type);
  bool DrawCosmeticLine(FX_FLOAT x1,
                        FX_FLOAT y1,
                        FX_FLOAT x2,
                        FX_FLOAT y2,
                        uint32_t color,
                        int fill_mode,
                        int blend_type);

  bool GetDIBits(CFX_DIBitmap* pBitmap, int left, int top);
  CFX_DIBitmap* GetBackDrop();
  bool SetDIBits(const CFX_DIBSource* pBitmap, int left, int top) {
    return SetDIBitsWithBlend(pBitmap, left, top, FXDIB_BLEND_NORMAL);
  }
  bool SetDIBitsWithBlend(const CFX_DIBSource* pBitmap,
                          int left,
                          int top,
                          int blend_type);
  bool StretchDIBits(const CFX_DIBSource* pBitmap,
                     int left,
                     int top,
                     int dest_width,
                     int dest_height) {
    return StretchDIBitsWithFlagsAndBlend(pBitmap, left, top, dest_width,
                                          dest_height, 0, FXDIB_BLEND_NORMAL);
  }
  bool StretchDIBitsWithFlagsAndBlend(const CFX_DIBSource* pBitmap,
                                      int left,
                                      int top,
                                      int dest_width,
                                      int dest_height,
                                      uint32_t flags,
                                      int blend_type);
  bool SetBitMask(const CFX_DIBSource* pBitmap,
                  int left,
                  int top,
                  uint32_t color);
  bool StretchBitMask(const CFX_DIBSource* pBitmap,
                      int left,
                      int top,
                      int dest_width,
                      int dest_height,
                      uint32_t color);
  bool StretchBitMaskWithFlags(const CFX_DIBSource* pBitmap,
                               int left,
                               int top,
                               int dest_width,
                               int dest_height,
                               uint32_t color,
                               uint32_t flags);
  bool StartDIBits(const CFX_DIBSource* pBitmap,
                   int bitmap_alpha,
                   uint32_t color,
                   const CFX_Matrix* pMatrix,
                   uint32_t flags,
                   void*& handle) {
    return StartDIBitsWithBlend(pBitmap, bitmap_alpha, color, pMatrix, flags,
                                handle, FXDIB_BLEND_NORMAL);
  }
  bool StartDIBitsWithBlend(const CFX_DIBSource* pBitmap,
                            int bitmap_alpha,
                            uint32_t color,
                            const CFX_Matrix* pMatrix,
                            uint32_t flags,
                            void*& handle,
                            int blend_type);
  bool ContinueDIBits(void* handle, IFX_Pause* pPause);
  void CancelDIBits(void* handle);

  bool DrawNormalText(int nChars,
                      const FXTEXT_CHARPOS* pCharPos,
                      CFX_Font* pFont,
                      FX_FLOAT font_size,
                      const CFX_Matrix* pText2Device,
                      uint32_t fill_color,
                      uint32_t text_flags);
  bool DrawTextPath(int nChars,
                    const FXTEXT_CHARPOS* pCharPos,
                    CFX_Font* pFont,
                    FX_FLOAT font_size,
                    const CFX_Matrix* pText2User,
                    const CFX_Matrix* pUser2Device,
                    const CFX_GraphStateData* pGraphState,
                    uint32_t fill_color,
                    uint32_t stroke_color,
                    CFX_PathData* pClippingPath,
                    int nFlag);

#ifdef _SKIA_SUPPORT_
  virtual void DebugVerifyBitmapIsPreMultiplied() const;
  virtual bool SetBitsWithMask(const CFX_DIBSource* pBitmap,
                               const CFX_DIBSource* pMask,
                               int left,
                               int top,
                               int bitmap_alpha,
                               int blend_type);
#endif
#if defined _SKIA_SUPPORT_ || defined _SKIA_SUPPORT_PATHS_
  void Flush();
#endif

 private:
  void InitDeviceInfo();
  void UpdateClipBox();
  bool DrawFillStrokePath(const CFX_PathData* pPathData,
                          const CFX_Matrix* pObject2Device,
                          const CFX_GraphStateData* pGraphState,
                          uint32_t fill_color,
                          uint32_t stroke_color,
                          int fill_mode,
                          int blend_type);

  CFX_DIBitmap* m_pBitmap;
  int m_Width;
  int m_Height;
  int m_bpp;
  int m_RenderCaps;
  int m_DeviceClass;
  FX_RECT m_ClipBox;
  std::unique_ptr<IFX_RenderDeviceDriver> m_pDeviceDriver;
};

#endif  // CORE_FXGE_CFX_RENDERDEVICE_H_

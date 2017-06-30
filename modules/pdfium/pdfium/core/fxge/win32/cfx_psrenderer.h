// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FXGE_WIN32_CFX_PSRENDERER_H_
#define CORE_FXGE_WIN32_CFX_PSRENDERER_H_

#include <memory>
#include <vector>

#include "core/fxcrt/fx_coordinates.h"
#include "core/fxcrt/fx_system.h"
#include "core/fxge/cfx_graphstatedata.h"
#include "core/fxge/win32/cpsoutput.h"

class CFX_DIBSource;
class CFX_FaceCache;
class CFX_Font;
class CFX_FontCache;
class CFX_Matrix;
class CFX_PathData;
class CPSFont;
class FXTEXT_CHARPOS;

class CFX_PSRenderer {
 public:
  CFX_PSRenderer();
  ~CFX_PSRenderer();

  void Init(CPSOutput* pOutput,
            int pslevel,
            int width,
            int height,
            bool bCmykOutput);
  bool StartRendering();
  void EndRendering();
  void SaveState();
  void RestoreState(bool bKeepSaved);
  void SetClip_PathFill(const CFX_PathData* pPathData,
                        const CFX_Matrix* pObject2Device,
                        int fill_mode);
  void SetClip_PathStroke(const CFX_PathData* pPathData,
                          const CFX_Matrix* pObject2Device,
                          const CFX_GraphStateData* pGraphState);
  FX_RECT GetClipBox() { return m_ClipBox; }
  bool DrawPath(const CFX_PathData* pPathData,
                const CFX_Matrix* pObject2Device,
                const CFX_GraphStateData* pGraphState,
                uint32_t fill_color,
                uint32_t stroke_color,
                int fill_mode);
  bool SetDIBits(const CFX_DIBSource* pBitmap,
                 uint32_t color,
                 int dest_left,
                 int dest_top);
  bool StretchDIBits(const CFX_DIBSource* pBitmap,
                     uint32_t color,
                     int dest_left,
                     int dest_top,
                     int dest_width,
                     int dest_height,
                     uint32_t flags);
  bool DrawDIBits(const CFX_DIBSource* pBitmap,
                  uint32_t color,
                  const CFX_Matrix* pMatrix,
                  uint32_t flags);
  bool DrawText(int nChars,
                const FXTEXT_CHARPOS* pCharPos,
                CFX_Font* pFont,
                const CFX_Matrix* pObject2Device,
                FX_FLOAT font_size,
                uint32_t color);

 private:
  void OutputPath(const CFX_PathData* pPathData,
                  const CFX_Matrix* pObject2Device);
  void SetGraphState(const CFX_GraphStateData* pGraphState);
  void SetColor(uint32_t color);
  void FindPSFontGlyph(CFX_FaceCache* pFaceCache,
                       CFX_Font* pFont,
                       const FXTEXT_CHARPOS& charpos,
                       int* ps_fontnum,
                       int* ps_glyphindex);
  void WritePSBinary(const uint8_t* data, int len);

  CPSOutput* m_pOutput;
  int m_PSLevel;
  CFX_GraphStateData m_CurGraphState;
  bool m_bGraphStateSet;
  bool m_bCmykOutput;
  bool m_bColorSet;
  uint32_t m_LastColor;
  FX_RECT m_ClipBox;
  std::vector<std::unique_ptr<CPSFont>> m_PSFontList;
  std::vector<FX_RECT> m_ClipBoxStack;
  bool m_bInited;
};

#endif  // CORE_FXGE_WIN32_CFX_PSRENDERER_H_

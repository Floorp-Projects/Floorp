// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FPDFAPI_RENDER_CPDF_RENDERSTATUS_H_
#define CORE_FPDFAPI_RENDER_CPDF_RENDERSTATUS_H_

#include <memory>
#include <vector>

#include "core/fpdfapi/page/cpdf_clippath.h"
#include "core/fpdfapi/page/cpdf_graphicstates.h"
#include "core/fpdfapi/render/cpdf_renderoptions.h"
#include "core/fxge/cfx_renderdevice.h"

class CFX_PathData;
class CPDF_Color;
class CPDF_Dictionary;
class CPDF_Font;
class CPDF_FormObject;
class CPDF_ImageCacheEntry;
class CPDF_ImageObject;
class CPDF_ImageRenderer;
class CPDF_Object;
class CPDF_PageObject;
class CPDF_PageObjectHolder;
class CPDF_PathObject;
class CPDF_ShadingObject;
class CPDF_ShadingPattern;
class CPDF_TilingPattern;
class CPDF_TransferFunc;
class CPDF_Type3Cache;
class CPDF_Type3Char;
class CPDF_Type3Font;

class CPDF_RenderStatus {
 public:
  CPDF_RenderStatus();
  ~CPDF_RenderStatus();

  bool Initialize(class CPDF_RenderContext* pContext,
                  CFX_RenderDevice* pDevice,
                  const CFX_Matrix* pDeviceMatrix,
                  const CPDF_PageObject* pStopObj,
                  const CPDF_RenderStatus* pParentStatus,
                  const CPDF_GraphicStates* pInitialStates,
                  const CPDF_RenderOptions* pOptions,
                  int transparency,
                  bool bDropObjects,
                  CPDF_Dictionary* pFormResource = nullptr,
                  bool bStdCS = false,
                  CPDF_Type3Char* pType3Char = nullptr,
                  FX_ARGB fill_color = 0,
                  uint32_t GroupFamily = 0,
                  bool bLoadMask = false);
  void RenderObjectList(const CPDF_PageObjectHolder* pObjectHolder,
                        const CFX_Matrix* pObj2Device);
  void RenderSingleObject(CPDF_PageObject* pObj, const CFX_Matrix* pObj2Device);
  bool ContinueSingleObject(CPDF_PageObject* pObj,
                            const CFX_Matrix* pObj2Device,
                            IFX_Pause* pPause);
  CPDF_RenderContext* GetContext() { return m_pContext; }

#if defined _SKIA_SUPPORT_
  void DebugVerifyDeviceIsPreMultiplied() const;
#endif

  CPDF_RenderOptions m_Options;
  CPDF_Dictionary* m_pFormResource;
  CPDF_Dictionary* m_pPageResource;
  std::vector<CPDF_Type3Font*> m_Type3FontCache;

 private:
  friend class CPDF_ImageRenderer;
  friend class CPDF_RenderContext;

  void ProcessClipPath(CPDF_ClipPath ClipPath, const CFX_Matrix* pObj2Device);
  bool ProcessTransparency(CPDF_PageObject* PageObj,
                           const CFX_Matrix* pObj2Device);
  void ProcessObjectNoClip(CPDF_PageObject* PageObj,
                           const CFX_Matrix* pObj2Device);
  void DrawObjWithBackground(CPDF_PageObject* pObj,
                             const CFX_Matrix* pObj2Device);
  bool DrawObjWithBlend(CPDF_PageObject* pObj, const CFX_Matrix* pObj2Device);
  bool ProcessPath(CPDF_PathObject* pPathObj, const CFX_Matrix* pObj2Device);
  void ProcessPathPattern(CPDF_PathObject* pPathObj,
                          const CFX_Matrix* pObj2Device,
                          int& filltype,
                          bool& bStroke);
  void DrawPathWithPattern(CPDF_PathObject* pPathObj,
                           const CFX_Matrix* pObj2Device,
                           const CPDF_Color* pColor,
                           bool bStroke);
  void DrawTilingPattern(CPDF_TilingPattern* pPattern,
                         CPDF_PageObject* pPageObj,
                         const CFX_Matrix* pObj2Device,
                         bool bStroke);
  void DrawShadingPattern(CPDF_ShadingPattern* pPattern,
                          const CPDF_PageObject* pPageObj,
                          const CFX_Matrix* pObj2Device,
                          bool bStroke);
  bool SelectClipPath(const CPDF_PathObject* pPathObj,
                      const CFX_Matrix* pObj2Device,
                      bool bStroke);
  bool ProcessImage(CPDF_ImageObject* pImageObj, const CFX_Matrix* pObj2Device);
  void CompositeDIBitmap(CFX_DIBitmap* pDIBitmap,
                         int left,
                         int top,
                         FX_ARGB mask_argb,
                         int bitmap_alpha,
                         int blend_mode,
                         int bIsolated);
  void ProcessShading(const CPDF_ShadingObject* pShadingObj,
                      const CFX_Matrix* pObj2Device);
  void DrawShading(CPDF_ShadingPattern* pPattern,
                   CFX_Matrix* pMatrix,
                   FX_RECT& clip_rect,
                   int alpha,
                   bool bAlphaMode);
  bool ProcessType3Text(CPDF_TextObject* textobj,
                        const CFX_Matrix* pObj2Device);
  bool ProcessText(CPDF_TextObject* textobj,
                   const CFX_Matrix* pObj2Device,
                   CFX_PathData* pClippingPath);
  void DrawTextPathWithPattern(const CPDF_TextObject* textobj,
                               const CFX_Matrix* pObj2Device,
                               CPDF_Font* pFont,
                               FX_FLOAT font_size,
                               const CFX_Matrix* pTextMatrix,
                               bool bFill,
                               bool bStroke);
  bool ProcessForm(const CPDF_FormObject* pFormObj,
                   const CFX_Matrix* pObj2Device);
  std::unique_ptr<CFX_DIBitmap> GetBackdrop(const CPDF_PageObject* pObj,
                                            const FX_RECT& rect,
                                            int& left,
                                            int& top,
                                            bool bBackAlphaRequired);
  std::unique_ptr<CFX_DIBitmap> LoadSMask(CPDF_Dictionary* pSMaskDict,
                                          FX_RECT* pClipRect,
                                          const CFX_Matrix* pMatrix);
  static CPDF_Type3Cache* GetCachedType3(CPDF_Type3Font* pFont);
  static CPDF_GraphicStates* CloneObjStates(const CPDF_GraphicStates* pPathObj,
                                            bool bStroke);
  CPDF_TransferFunc* GetTransferFunc(CPDF_Object* pObject) const;
  FX_ARGB GetFillArgb(CPDF_PageObject* pObj, bool bType3 = false) const;
  FX_ARGB GetStrokeArgb(CPDF_PageObject* pObj) const;
  bool GetObjectClippedRect(const CPDF_PageObject* pObj,
                            const CFX_Matrix* pObj2Device,
                            bool bLogical,
                            FX_RECT& rect) const;
  void GetScaledMatrix(CFX_Matrix& matrix) const;

  static const int kRenderMaxRecursionDepth = 64;
  static int s_CurrentRecursionDepth;

  CPDF_RenderContext* m_pContext;
  bool m_bStopped;
  CFX_RenderDevice* m_pDevice;
  CFX_Matrix m_DeviceMatrix;
  CPDF_ClipPath m_LastClipPath;
  const CPDF_PageObject* m_pCurObj;
  const CPDF_PageObject* m_pStopObj;
  CPDF_GraphicStates m_InitialStates;
  std::unique_ptr<CPDF_ImageRenderer> m_pImageRenderer;
  bool m_bPrint;
  int m_Transparency;
  bool m_bDropObjects;
  bool m_bStdCS;
  uint32_t m_GroupFamily;
  bool m_bLoadMask;
  CPDF_Type3Char* m_pType3Char;
  FX_ARGB m_T3FillColor;
  int m_curBlend;
};

#endif  // CORE_FPDFAPI_RENDER_CPDF_RENDERSTATUS_H_

// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fpdfapi/render/cpdf_imagerenderer.h"

#include <algorithm>
#include <memory>

#include "core/fpdfapi/page/cpdf_docpagedata.h"
#include "core/fpdfapi/page/cpdf_image.h"
#include "core/fpdfapi/page/cpdf_imageobject.h"
#include "core/fpdfapi/page/cpdf_page.h"
#include "core/fpdfapi/page/cpdf_pageobject.h"
#include "core/fpdfapi/page/cpdf_shadingpattern.h"
#include "core/fpdfapi/page/cpdf_tilingpattern.h"
#include "core/fpdfapi/parser/cpdf_array.h"
#include "core/fpdfapi/parser/cpdf_dictionary.h"
#include "core/fpdfapi/parser/cpdf_document.h"
#include "core/fpdfapi/render/cpdf_dibsource.h"
#include "core/fpdfapi/render/cpdf_pagerendercache.h"
#include "core/fpdfapi/render/cpdf_rendercontext.h"
#include "core/fpdfapi/render/cpdf_renderstatus.h"
#include "core/fpdfapi/render/cpdf_transferfunc.h"
#include "core/fpdfdoc/cpdf_occontext.h"
#include "core/fxcrt/cfx_maybe_owned.h"
#include "core/fxcrt/fx_safe_types.h"
#include "core/fxge/cfx_fxgedevice.h"
#include "core/fxge/cfx_pathdata.h"
#include "third_party/base/ptr_util.h"

#ifdef _SKIA_SUPPORT_
#include "core/fxge/skia/fx_skia_device.h"
#endif

CPDF_ImageRenderer::CPDF_ImageRenderer() {
  m_pRenderStatus = nullptr;
  m_pImageObject = nullptr;
  m_Result = true;
  m_Status = 0;
  m_DeviceHandle = nullptr;
  m_bStdCS = false;
  m_bPatternColor = false;
  m_BlendType = FXDIB_BLEND_NORMAL;
  m_pPattern = nullptr;
  m_pObj2Device = nullptr;
}

CPDF_ImageRenderer::~CPDF_ImageRenderer() {
  if (m_DeviceHandle)
    m_pRenderStatus->m_pDevice->CancelDIBits(m_DeviceHandle);
}

bool CPDF_ImageRenderer::StartLoadDIBSource() {
  CFX_FloatRect image_rect_f = m_ImageMatrix.GetUnitRect();
  FX_RECT image_rect = image_rect_f.GetOuterRect();
  if (!image_rect.Valid())
    return false;

  int dest_width =
      m_ImageMatrix.a >= 0 ? image_rect.Width() : -image_rect.Width();
  int dest_height =
      m_ImageMatrix.d <= 0 ? image_rect.Height() : -image_rect.Height();
  if (m_Loader.Start(
          m_pImageObject, m_pRenderStatus->m_pContext->GetPageCache(), m_bStdCS,
          m_pRenderStatus->m_GroupFamily, m_pRenderStatus->m_bLoadMask,
          m_pRenderStatus, dest_width, dest_height)) {
    m_Status = 4;
    return true;
  }
  return false;
}

bool CPDF_ImageRenderer::StartRenderDIBSource() {
  if (!m_Loader.m_pBitmap)
    return false;

  m_BitmapAlpha =
      FXSYS_round(255 * m_pImageObject->m_GeneralState.GetFillAlpha());
  m_pDIBSource = m_Loader.m_pBitmap;
  if (m_pRenderStatus->m_Options.m_ColorMode == RENDER_COLOR_ALPHA &&
      !m_Loader.m_pMask) {
    return StartBitmapAlpha();
  }
  if (m_pImageObject->m_GeneralState.GetTR()) {
    if (!m_pImageObject->m_GeneralState.GetTransferFunc()) {
      m_pImageObject->m_GeneralState.SetTransferFunc(
          m_pRenderStatus->GetTransferFunc(
              m_pImageObject->m_GeneralState.GetTR()));
    }
    if (m_pImageObject->m_GeneralState.GetTransferFunc() &&
        !m_pImageObject->m_GeneralState.GetTransferFunc()->m_bIdentity) {
      m_pDIBSource = m_Loader.m_pBitmap =
          m_pImageObject->m_GeneralState.GetTransferFunc()->TranslateImage(
              m_Loader.m_pBitmap, !m_Loader.m_bCached);
      if (m_Loader.m_bCached && m_Loader.m_pMask)
        m_Loader.m_pMask = m_Loader.m_pMask->Clone().release();
      m_Loader.m_bCached = false;
    }
  }
  m_FillArgb = 0;
  m_bPatternColor = false;
  m_pPattern = nullptr;
  if (m_pDIBSource->IsAlphaMask()) {
    const CPDF_Color* pColor = m_pImageObject->m_ColorState.GetFillColor();
    if (pColor && pColor->IsPattern()) {
      m_pPattern = pColor->GetPattern();
      if (m_pPattern)
        m_bPatternColor = true;
    }
    m_FillArgb = m_pRenderStatus->GetFillArgb(m_pImageObject);
  } else if (m_pRenderStatus->m_Options.m_ColorMode == RENDER_COLOR_GRAY) {
    m_pClone = m_pDIBSource->Clone();
    m_pClone->ConvertColorScale(m_pRenderStatus->m_Options.m_BackColor,
                                m_pRenderStatus->m_Options.m_ForeColor);
    m_pDIBSource = m_pClone.get();
  }
  m_Flags = 0;
  if (m_pRenderStatus->m_Options.m_Flags & RENDER_FORCE_DOWNSAMPLE)
    m_Flags |= RENDER_FORCE_DOWNSAMPLE;
  else if (m_pRenderStatus->m_Options.m_Flags & RENDER_FORCE_HALFTONE)
    m_Flags |= RENDER_FORCE_HALFTONE;
  if (m_pRenderStatus->m_pDevice->GetDeviceClass() != FXDC_DISPLAY) {
    CPDF_Object* pFilters =
        m_pImageObject->GetImage()->GetStream()->GetDict()->GetDirectObjectFor(
            "Filter");
    if (pFilters) {
      if (pFilters->IsName()) {
        CFX_ByteString bsDecodeType = pFilters->GetString();
        if (bsDecodeType == "DCTDecode" || bsDecodeType == "JPXDecode")
          m_Flags |= FXRENDER_IMAGE_LOSSY;
      } else if (CPDF_Array* pArray = pFilters->AsArray()) {
        for (size_t i = 0; i < pArray->GetCount(); i++) {
          CFX_ByteString bsDecodeType = pArray->GetStringAt(i);
          if (bsDecodeType == "DCTDecode" || bsDecodeType == "JPXDecode") {
            m_Flags |= FXRENDER_IMAGE_LOSSY;
            break;
          }
        }
      }
    }
  }
  if (m_pRenderStatus->m_Options.m_Flags & RENDER_NOIMAGESMOOTH)
    m_Flags |= FXDIB_NOSMOOTH;
  else if (m_pImageObject->GetImage()->IsInterpol())
    m_Flags |= FXDIB_INTERPOL;
  if (m_Loader.m_pMask)
    return DrawMaskedImage();

  if (m_bPatternColor)
    return DrawPatternImage(m_pObj2Device);

  if (m_BitmapAlpha != 255 || !m_pImageObject->m_GeneralState ||
      !m_pImageObject->m_GeneralState.GetFillOP() ||
      m_pImageObject->m_GeneralState.GetOPMode() != 0 ||
      m_pImageObject->m_GeneralState.GetBlendType() != FXDIB_BLEND_NORMAL ||
      m_pImageObject->m_GeneralState.GetStrokeAlpha() != 1.0f ||
      m_pImageObject->m_GeneralState.GetFillAlpha() != 1.0f) {
    return StartDIBSource();
  }
  CPDF_Document* pDocument = nullptr;
  CPDF_Page* pPage = nullptr;
  if (m_pRenderStatus->m_pContext->GetPageCache()) {
    pPage = m_pRenderStatus->m_pContext->GetPageCache()->GetPage();
    pDocument = pPage->m_pDocument;
  } else {
    pDocument = m_pImageObject->GetImage()->GetDocument();
  }
  CPDF_Dictionary* pPageResources = pPage ? pPage->m_pPageResources : nullptr;
  CPDF_Object* pCSObj =
      m_pImageObject->GetImage()->GetStream()->GetDict()->GetDirectObjectFor(
          "ColorSpace");
  CPDF_ColorSpace* pColorSpace =
      pDocument->LoadColorSpace(pCSObj, pPageResources);
  if (!pColorSpace)
    return StartDIBSource();
  int format = pColorSpace->GetFamily();
  if (format == PDFCS_DEVICECMYK || format == PDFCS_SEPARATION ||
      format == PDFCS_DEVICEN) {
    m_BlendType = FXDIB_BLEND_DARKEN;
  }
  pDocument->GetPageData()->ReleaseColorSpace(pCSObj);
  return StartDIBSource();
}

bool CPDF_ImageRenderer::Start(CPDF_RenderStatus* pStatus,
                               CPDF_PageObject* pObj,
                               const CFX_Matrix* pObj2Device,
                               bool bStdCS,
                               int blendType) {
  m_pRenderStatus = pStatus;
  m_bStdCS = bStdCS;
  m_pImageObject = pObj->AsImage();
  m_BlendType = blendType;
  m_pObj2Device = pObj2Device;
  CPDF_Dictionary* pOC = m_pImageObject->GetImage()->GetOC();
  if (pOC && m_pRenderStatus->m_Options.m_pOCContext &&
      !m_pRenderStatus->m_Options.m_pOCContext->CheckOCGVisible(pOC)) {
    return false;
  }
  m_ImageMatrix = m_pImageObject->matrix();
  m_ImageMatrix.Concat(*pObj2Device);
  if (StartLoadDIBSource())
    return true;
  return StartRenderDIBSource();
}

bool CPDF_ImageRenderer::Start(CPDF_RenderStatus* pStatus,
                               const CFX_DIBSource* pDIBSource,
                               FX_ARGB bitmap_argb,
                               int bitmap_alpha,
                               const CFX_Matrix* pImage2Device,
                               uint32_t flags,
                               bool bStdCS,
                               int blendType) {
  m_pRenderStatus = pStatus;
  m_pDIBSource = pDIBSource;
  m_FillArgb = bitmap_argb;
  m_BitmapAlpha = bitmap_alpha;
  m_ImageMatrix = *pImage2Device;
  m_Flags = flags;
  m_bStdCS = bStdCS;
  m_BlendType = blendType;
  return StartDIBSource();
}

bool CPDF_ImageRenderer::NotDrawing() const {
  return m_pRenderStatus->m_bPrint &&
         !(m_pRenderStatus->m_pDevice->GetRenderCaps() & FXRC_BLEND_MODE);
}

FX_RECT CPDF_ImageRenderer::GetDrawRect() const {
  FX_RECT rect = m_ImageMatrix.GetUnitRect().GetOuterRect();
  rect.Intersect(m_pRenderStatus->m_pDevice->GetClipBox());
  return rect;
}

CFX_Matrix CPDF_ImageRenderer::GetDrawMatrix(const FX_RECT& rect) const {
  CFX_Matrix new_matrix = m_ImageMatrix;
  new_matrix.Translate(-rect.left, -rect.top);
  return new_matrix;
}

void CPDF_ImageRenderer::CalculateDrawImage(CFX_FxgeDevice* pBitmapDevice1,
                                            CFX_FxgeDevice* pBitmapDevice2,
                                            const CFX_DIBSource* pDIBSource,
                                            CFX_Matrix* pNewMatrix,
                                            const FX_RECT& rect) const {
  CPDF_RenderStatus bitmap_render;
  bitmap_render.Initialize(m_pRenderStatus->m_pContext, pBitmapDevice2, nullptr,
                           nullptr, nullptr, nullptr, nullptr, 0,
                           m_pRenderStatus->m_bDropObjects, nullptr, true);
  CPDF_ImageRenderer image_render;
  if (image_render.Start(&bitmap_render, pDIBSource, 0xffffffff, 255,
                         pNewMatrix, m_Flags, true, FXDIB_BLEND_NORMAL)) {
    image_render.Continue(nullptr);
  }
  if (m_Loader.m_MatteColor == 0xffffffff)
    return;
  int matte_r = FXARGB_R(m_Loader.m_MatteColor);
  int matte_g = FXARGB_G(m_Loader.m_MatteColor);
  int matte_b = FXARGB_B(m_Loader.m_MatteColor);
  for (int row = 0; row < rect.Height(); row++) {
    uint8_t* dest_scan =
        const_cast<uint8_t*>(pBitmapDevice1->GetBitmap()->GetScanline(row));
    const uint8_t* mask_scan = pBitmapDevice2->GetBitmap()->GetScanline(row);
    for (int col = 0; col < rect.Width(); col++) {
      int alpha = *mask_scan++;
      if (!alpha) {
        dest_scan += 4;
        continue;
      }
      int orig = (*dest_scan - matte_b) * 255 / alpha + matte_b;
      *dest_scan++ = std::min(std::max(orig, 0), 255);
      orig = (*dest_scan - matte_g) * 255 / alpha + matte_g;
      *dest_scan++ = std::min(std::max(orig, 0), 255);
      orig = (*dest_scan - matte_r) * 255 / alpha + matte_r;
      *dest_scan++ = std::min(std::max(orig, 0), 255);
      dest_scan++;
    }
  }
}

bool CPDF_ImageRenderer::DrawPatternImage(const CFX_Matrix* pObj2Device) {
  if (NotDrawing()) {
    m_Result = false;
    return false;
  }

  FX_RECT rect = GetDrawRect();
  if (rect.IsEmpty())
    return false;

  CFX_Matrix new_matrix = GetDrawMatrix(rect);
  CFX_FxgeDevice bitmap_device1;
  if (!bitmap_device1.Create(rect.Width(), rect.Height(), FXDIB_Rgb32, nullptr))
    return true;

  bitmap_device1.GetBitmap()->Clear(0xffffff);
  CPDF_RenderStatus bitmap_render;
  bitmap_render.Initialize(m_pRenderStatus->m_pContext, &bitmap_device1,
                           nullptr, nullptr, nullptr, nullptr,
                           &m_pRenderStatus->m_Options, 0,
                           m_pRenderStatus->m_bDropObjects, nullptr, true);
  CFX_Matrix patternDevice = *pObj2Device;
  patternDevice.Translate((FX_FLOAT)-rect.left, (FX_FLOAT)-rect.top);
  if (CPDF_TilingPattern* pTilingPattern = m_pPattern->AsTilingPattern()) {
    bitmap_render.DrawTilingPattern(pTilingPattern, m_pImageObject,
                                    &patternDevice, false);
  } else if (CPDF_ShadingPattern* pShadingPattern =
                 m_pPattern->AsShadingPattern()) {
    bitmap_render.DrawShadingPattern(pShadingPattern, m_pImageObject,
                                     &patternDevice, false);
  }

  CFX_FxgeDevice bitmap_device2;
  if (!bitmap_device2.Create(rect.Width(), rect.Height(), FXDIB_8bppRgb,
                             nullptr)) {
    return true;
  }
  bitmap_device2.GetBitmap()->Clear(0);
  CalculateDrawImage(&bitmap_device1, &bitmap_device2, m_pDIBSource,
                     &new_matrix, rect);
  bitmap_device2.GetBitmap()->ConvertFormat(FXDIB_8bppMask);
  bitmap_device1.GetBitmap()->MultiplyAlpha(bitmap_device2.GetBitmap());
  bitmap_device1.GetBitmap()->MultiplyAlpha(255);
  m_pRenderStatus->m_pDevice->SetDIBitsWithBlend(
      bitmap_device1.GetBitmap(), rect.left, rect.top, m_BlendType);
  return false;
}

bool CPDF_ImageRenderer::DrawMaskedImage() {
  if (NotDrawing()) {
    m_Result = false;
    return false;
  }

  FX_RECT rect = GetDrawRect();
  if (rect.IsEmpty())
    return false;

  CFX_Matrix new_matrix = GetDrawMatrix(rect);
  CFX_FxgeDevice bitmap_device1;
  if (!bitmap_device1.Create(rect.Width(), rect.Height(), FXDIB_Rgb32, nullptr))
    return true;

#if defined _SKIA_SUPPORT_
  bitmap_device1.Clear(0xffffff);
#else
  bitmap_device1.GetBitmap()->Clear(0xffffff);
#endif
  CPDF_RenderStatus bitmap_render;
  bitmap_render.Initialize(m_pRenderStatus->m_pContext, &bitmap_device1,
                           nullptr, nullptr, nullptr, nullptr, nullptr, 0,
                           m_pRenderStatus->m_bDropObjects, nullptr, true);
  CPDF_ImageRenderer image_render;
  if (image_render.Start(&bitmap_render, m_pDIBSource, 0, 255, &new_matrix,
                         m_Flags, true, FXDIB_BLEND_NORMAL)) {
    image_render.Continue(nullptr);
  }
  CFX_FxgeDevice bitmap_device2;
  if (!bitmap_device2.Create(rect.Width(), rect.Height(), FXDIB_8bppRgb,
                             nullptr))
    return true;

#if defined _SKIA_SUPPORT_
  bitmap_device2.Clear(0);
#else
  bitmap_device2.GetBitmap()->Clear(0);
#endif
  CalculateDrawImage(&bitmap_device1, &bitmap_device2, m_Loader.m_pMask,
                     &new_matrix, rect);
#ifdef _SKIA_SUPPORT_
  m_pRenderStatus->m_pDevice->SetBitsWithMask(
      bitmap_device1.GetBitmap(), bitmap_device2.GetBitmap(), rect.left,
      rect.top, m_BitmapAlpha, m_BlendType);
#else
  bitmap_device2.GetBitmap()->ConvertFormat(FXDIB_8bppMask);
  bitmap_device1.GetBitmap()->MultiplyAlpha(bitmap_device2.GetBitmap());
  if (m_BitmapAlpha < 255)
    bitmap_device1.GetBitmap()->MultiplyAlpha(m_BitmapAlpha);
  m_pRenderStatus->m_pDevice->SetDIBitsWithBlend(
      bitmap_device1.GetBitmap(), rect.left, rect.top, m_BlendType);
#endif  //  _SKIA_SUPPORT_
  return false;
}

bool CPDF_ImageRenderer::StartDIBSource() {
  if (!(m_Flags & RENDER_FORCE_DOWNSAMPLE) && m_pDIBSource->GetBPP() > 1) {
    FX_SAFE_SIZE_T image_size = m_pDIBSource->GetBPP();
    image_size /= 8;
    image_size *= m_pDIBSource->GetWidth();
    image_size *= m_pDIBSource->GetHeight();
    if (!image_size.IsValid())
      return false;

    if (image_size.ValueOrDie() > FPDF_HUGE_IMAGE_SIZE &&
        !(m_Flags & RENDER_FORCE_HALFTONE)) {
      m_Flags |= RENDER_FORCE_DOWNSAMPLE;
    }
  }
#ifdef _SKIA_SUPPORT_
  CFX_DIBitmap* premultiplied = m_pDIBSource->Clone().release();
  if (m_pDIBSource->HasAlpha())
    CFX_SkiaDeviceDriver::PreMultiply(premultiplied);
  if (m_pRenderStatus->m_pDevice->StartDIBitsWithBlend(
          premultiplied, m_BitmapAlpha, m_FillArgb, &m_ImageMatrix, m_Flags,
          m_DeviceHandle, m_BlendType)) {
    if (m_DeviceHandle) {
      m_Status = 3;
      return true;
    }
    return false;
  }
#else
  if (m_pRenderStatus->m_pDevice->StartDIBitsWithBlend(
          m_pDIBSource, m_BitmapAlpha, m_FillArgb, &m_ImageMatrix, m_Flags,
          m_DeviceHandle, m_BlendType)) {
    if (m_DeviceHandle) {
      m_Status = 3;
      return true;
    }
    return false;
  }
#endif
  CFX_FloatRect image_rect_f = m_ImageMatrix.GetUnitRect();
  FX_RECT image_rect = image_rect_f.GetOuterRect();
  int dest_width = image_rect.Width();
  int dest_height = image_rect.Height();
  if ((FXSYS_fabs(m_ImageMatrix.b) >= 0.5f || m_ImageMatrix.a == 0) ||
      (FXSYS_fabs(m_ImageMatrix.c) >= 0.5f || m_ImageMatrix.d == 0)) {
    if (NotDrawing()) {
      m_Result = false;
      return false;
    }

    FX_RECT clip_box = m_pRenderStatus->m_pDevice->GetClipBox();
    clip_box.Intersect(image_rect);
    m_Status = 2;
    m_pTransformer = pdfium::MakeUnique<CFX_ImageTransformer>(
        m_pDIBSource, &m_ImageMatrix, m_Flags, &clip_box);
    m_pTransformer->Start();
    return true;
  }
  if (m_ImageMatrix.a < 0)
    dest_width = -dest_width;

  if (m_ImageMatrix.d > 0)
    dest_height = -dest_height;

  int dest_left = dest_width > 0 ? image_rect.left : image_rect.right;
  int dest_top = dest_height > 0 ? image_rect.top : image_rect.bottom;
  if (m_pDIBSource->IsOpaqueImage() && m_BitmapAlpha == 255) {
    if (m_pRenderStatus->m_pDevice->StretchDIBitsWithFlagsAndBlend(
            m_pDIBSource, dest_left, dest_top, dest_width, dest_height, m_Flags,
            m_BlendType)) {
      return false;
    }
  }
  if (m_pDIBSource->IsAlphaMask()) {
    if (m_BitmapAlpha != 255)
      m_FillArgb = FXARGB_MUL_ALPHA(m_FillArgb, m_BitmapAlpha);
    if (m_pRenderStatus->m_pDevice->StretchBitMaskWithFlags(
            m_pDIBSource, dest_left, dest_top, dest_width, dest_height,
            m_FillArgb, m_Flags)) {
      return false;
    }
  }
  if (NotDrawing()) {
    m_Result = false;
    return true;
  }

  FX_RECT clip_box = m_pRenderStatus->m_pDevice->GetClipBox();
  FX_RECT dest_rect = clip_box;
  dest_rect.Intersect(image_rect);
  FX_RECT dest_clip(
      dest_rect.left - image_rect.left, dest_rect.top - image_rect.top,
      dest_rect.right - image_rect.left, dest_rect.bottom - image_rect.top);
  std::unique_ptr<CFX_DIBitmap> pStretched(
      m_pDIBSource->StretchTo(dest_width, dest_height, m_Flags, &dest_clip));
  if (pStretched) {
    m_pRenderStatus->CompositeDIBitmap(pStretched.get(), dest_rect.left,
                                       dest_rect.top, m_FillArgb, m_BitmapAlpha,
                                       m_BlendType, false);
  }
  return false;
}

bool CPDF_ImageRenderer::StartBitmapAlpha() {
  if (m_pDIBSource->IsOpaqueImage()) {
    CFX_PathData path;
    path.AppendRect(0, 0, 1, 1);
    path.Transform(&m_ImageMatrix);
    uint32_t fill_color =
        ArgbEncode(0xff, m_BitmapAlpha, m_BitmapAlpha, m_BitmapAlpha);
    m_pRenderStatus->m_pDevice->DrawPath(&path, nullptr, nullptr, fill_color, 0,
                                         FXFILL_WINDING);
    return false;
  }
  CFX_MaybeOwned<CFX_DIBSource> pAlphaMask;
  if (m_pDIBSource->IsAlphaMask())
    pAlphaMask = const_cast<CFX_DIBSource*>(m_pDIBSource);
  else
    pAlphaMask = m_pDIBSource->CloneAlphaMask();

  if (FXSYS_fabs(m_ImageMatrix.b) >= 0.5f ||
      FXSYS_fabs(m_ImageMatrix.c) >= 0.5f) {
    int left;
    int top;
    std::unique_ptr<CFX_DIBitmap> pTransformed =
        pAlphaMask->TransformTo(&m_ImageMatrix, left, top);
    if (!pTransformed)
      return true;

    m_pRenderStatus->m_pDevice->SetBitMask(
        pTransformed.get(), left, top,
        ArgbEncode(0xff, m_BitmapAlpha, m_BitmapAlpha, m_BitmapAlpha));
    return false;
  }
  CFX_FloatRect image_rect_f = m_ImageMatrix.GetUnitRect();
  FX_RECT image_rect = image_rect_f.GetOuterRect();
  int dest_width =
      m_ImageMatrix.a > 0 ? image_rect.Width() : -image_rect.Width();
  int dest_height =
      m_ImageMatrix.d > 0 ? -image_rect.Height() : image_rect.Height();
  int left = dest_width > 0 ? image_rect.left : image_rect.right;
  int top = dest_height > 0 ? image_rect.top : image_rect.bottom;
  m_pRenderStatus->m_pDevice->StretchBitMask(
      pAlphaMask.Get(), left, top, dest_width, dest_height,
      ArgbEncode(0xff, m_BitmapAlpha, m_BitmapAlpha, m_BitmapAlpha));
  return false;
}

bool CPDF_ImageRenderer::Continue(IFX_Pause* pPause) {
  if (m_Status == 2) {
    if (m_pTransformer->Continue(pPause))
      return true;

    std::unique_ptr<CFX_DIBitmap> pBitmap(m_pTransformer->DetachBitmap());
    if (!pBitmap)
      return false;

    if (pBitmap->IsAlphaMask()) {
      if (m_BitmapAlpha != 255)
        m_FillArgb = FXARGB_MUL_ALPHA(m_FillArgb, m_BitmapAlpha);
      m_Result = m_pRenderStatus->m_pDevice->SetBitMask(
          pBitmap.get(), m_pTransformer->result().left,
          m_pTransformer->result().top, m_FillArgb);
    } else {
      if (m_BitmapAlpha != 255)
        pBitmap->MultiplyAlpha(m_BitmapAlpha);
      m_Result = m_pRenderStatus->m_pDevice->SetDIBitsWithBlend(
          pBitmap.get(), m_pTransformer->result().left,
          m_pTransformer->result().top, m_BlendType);
    }
    return false;
  }
  if (m_Status == 3)
    return m_pRenderStatus->m_pDevice->ContinueDIBits(m_DeviceHandle, pPause);

  if (m_Status == 4) {
    if (m_Loader.Continue(pPause))
      return true;

    if (StartRenderDIBSource())
      return Continue(pPause);
  }
  return false;
}

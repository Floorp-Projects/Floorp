// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fpdfapi/font/cpdf_type3char.h"

#include "core/fpdfapi/page/cpdf_form.h"
#include "core/fpdfapi/page/cpdf_image.h"
#include "core/fpdfapi/page/cpdf_imageobject.h"
#include "core/fpdfapi/page/cpdf_pageobject.h"
#include "core/fxge/fx_dib.h"

CPDF_Type3Char::CPDF_Type3Char(CPDF_Form* pForm)
    : m_pForm(pForm), m_bColored(false) {}

CPDF_Type3Char::~CPDF_Type3Char() {}

bool CPDF_Type3Char::LoadBitmap(CPDF_RenderContext* pContext) {
  if (m_pBitmap || !m_pForm)
    return true;

  if (m_pForm->GetPageObjectList()->size() != 1 || m_bColored)
    return false;

  auto& pPageObj = m_pForm->GetPageObjectList()->front();
  if (!pPageObj->IsImage())
    return false;

  m_ImageMatrix = pPageObj->AsImage()->matrix();
  std::unique_ptr<CFX_DIBSource> pSource =
      pPageObj->AsImage()->GetImage()->LoadDIBSource();
  if (pSource)
    m_pBitmap = pSource->Clone();
  m_pForm.reset();
  return true;
}

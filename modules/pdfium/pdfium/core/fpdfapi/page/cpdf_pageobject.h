// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FPDFAPI_PAGE_CPDF_PAGEOBJECT_H_
#define CORE_FPDFAPI_PAGE_CPDF_PAGEOBJECT_H_

#include "core/fpdfapi/page/cpdf_contentmark.h"
#include "core/fpdfapi/page/cpdf_graphicstates.h"
#include "core/fxcrt/fx_coordinates.h"
#include "core/fxcrt/fx_system.h"

class CPDF_TextObject;
class CPDF_PathObject;
class CPDF_ImageObject;
class CPDF_ShadingObject;
class CPDF_FormObject;

class CPDF_PageObject : public CPDF_GraphicStates {
 public:
  enum Type {
    TEXT = 1,
    PATH,
    IMAGE,
    SHADING,
    FORM,
  };

  CPDF_PageObject();
  ~CPDF_PageObject() override;

  virtual Type GetType() const = 0;
  virtual void Transform(const CFX_Matrix& matrix) = 0;
  virtual bool IsText() const;
  virtual bool IsPath() const;
  virtual bool IsImage() const;
  virtual bool IsShading() const;
  virtual bool IsForm() const;
  virtual CPDF_TextObject* AsText();
  virtual const CPDF_TextObject* AsText() const;
  virtual CPDF_PathObject* AsPath();
  virtual const CPDF_PathObject* AsPath() const;
  virtual CPDF_ImageObject* AsImage();
  virtual const CPDF_ImageObject* AsImage() const;
  virtual CPDF_ShadingObject* AsShading();
  virtual const CPDF_ShadingObject* AsShading() const;
  virtual CPDF_FormObject* AsForm();
  virtual const CPDF_FormObject* AsForm() const;

  void TransformClipPath(CFX_Matrix& matrix);
  void TransformGeneralState(CFX_Matrix& matrix);

  CFX_FloatRect GetRect() const {
    return CFX_FloatRect(m_Left, m_Bottom, m_Right, m_Top);
  }
  FX_RECT GetBBox(const CFX_Matrix* pMatrix) const;

  FX_FLOAT m_Left;
  FX_FLOAT m_Right;
  FX_FLOAT m_Top;
  FX_FLOAT m_Bottom;
  CPDF_ContentMark m_ContentMark;

 protected:
  void CopyData(const CPDF_PageObject* pSrcObject);

 private:
  CPDF_PageObject(const CPDF_PageObject& src) = delete;
  void operator=(const CPDF_PageObject& src) = delete;
};

#endif  // CORE_FPDFAPI_PAGE_CPDF_PAGEOBJECT_H_

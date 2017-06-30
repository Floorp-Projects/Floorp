// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FPDFDOC_CPDF_DEST_H_
#define CORE_FPDFDOC_CPDF_DEST_H_

#include "core/fxcrt/fx_string.h"
#include "core/fxcrt/fx_system.h"

class CPDF_Document;
class CPDF_Object;

class CPDF_Dest {
 public:
  CPDF_Dest() : m_pObj(nullptr) {}
  explicit CPDF_Dest(CPDF_Object* pObj) : m_pObj(pObj) {}

  CPDF_Object* GetObject() const { return m_pObj; }
  CFX_ByteString GetRemoteName();
  int GetPageIndex(CPDF_Document* pDoc);
  uint32_t GetPageObjNum();
  int GetZoomMode();
  FX_FLOAT GetParam(int index);

  bool GetXYZ(bool* pHasX,
              bool* pHasY,
              bool* pHasZoom,
              float* pX,
              float* pY,
              float* pZoom) const;

 private:
  CPDF_Object* m_pObj;
};

#endif  // CORE_FPDFDOC_CPDF_DEST_H_

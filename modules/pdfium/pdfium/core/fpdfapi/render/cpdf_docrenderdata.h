// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FPDFAPI_RENDER_CPDF_DOCRENDERDATA_H_
#define CORE_FPDFAPI_RENDER_CPDF_DOCRENDERDATA_H_

#include <map>

#include "core/fpdfapi/page/cpdf_countedobject.h"

class CPDF_Document;
class CPDF_Font;
class CPDF_Object;
class CPDF_TransferFunc;
class CPDF_Type3Cache;
class CPDF_Type3Font;

class CPDF_DocRenderData {
 public:
  explicit CPDF_DocRenderData(CPDF_Document* pPDFDoc);
  ~CPDF_DocRenderData();

  CPDF_Type3Cache* GetCachedType3(CPDF_Type3Font* pFont);
  void ReleaseCachedType3(CPDF_Type3Font* pFont);
  CPDF_TransferFunc* GetTransferFunc(CPDF_Object* pObj);
  void ReleaseTransferFunc(CPDF_Object* pObj);
  void Clear(bool bRelease);

 private:
  using CPDF_Type3CacheMap =
      std::map<CPDF_Font*, CPDF_CountedObject<CPDF_Type3Cache>*>;
  using CPDF_TransferFuncMap =
      std::map<CPDF_Object*, CPDF_CountedObject<CPDF_TransferFunc>*>;

  CPDF_Document* m_pPDFDoc;  // Not Owned
  CPDF_Type3CacheMap m_Type3FaceMap;
  CPDF_TransferFuncMap m_TransferFuncMap;
};

#endif  // CORE_FPDFAPI_RENDER_CPDF_DOCRENDERDATA_H_

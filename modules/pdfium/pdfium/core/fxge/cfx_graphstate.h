// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FXGE_CFX_GRAPHSTATE_H_
#define CORE_FXGE_CFX_GRAPHSTATE_H_

#include "core/fxcrt/cfx_shared_copy_on_write.h"
#include "core/fxge/cfx_graphstatedata.h"

class CPDF_Array;

class CFX_GraphState {
 public:
  CFX_GraphState();
  CFX_GraphState(const CFX_GraphState& that);
  ~CFX_GraphState();

  void Emplace();

  void SetLineDash(CPDF_Array* pArray, FX_FLOAT phase, FX_FLOAT scale);

  FX_FLOAT GetLineWidth() const;
  void SetLineWidth(FX_FLOAT width);

  CFX_GraphStateData::LineCap GetLineCap() const;
  void SetLineCap(CFX_GraphStateData::LineCap cap);

  CFX_GraphStateData::LineJoin GetLineJoin() const;
  void SetLineJoin(CFX_GraphStateData::LineJoin join);

  FX_FLOAT GetMiterLimit() const;
  void SetMiterLimit(FX_FLOAT limit);

  // FIXME(tsepez): remove when all GraphStateData usage gone.
  const CFX_GraphStateData* GetObject() const { return m_Ref.GetObject(); }

 private:
  CFX_SharedCopyOnWrite<CFX_GraphStateData> m_Ref;
};

#endif  // CORE_FXGE_CFX_GRAPHSTATE_H_

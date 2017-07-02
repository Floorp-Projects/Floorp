// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fpdfapi/page/cpdf_clippath.h"

#include <utility>

#include "core/fpdfapi/page/cpdf_path.h"
#include "core/fpdfapi/page/cpdf_textobject.h"
#include "third_party/base/stl_util.h"

#define FPDF_CLIPPATH_MAX_TEXTS 1024

CPDF_ClipPath::CPDF_ClipPath() {}

CPDF_ClipPath::CPDF_ClipPath(const CPDF_ClipPath& that) : m_Ref(that.m_Ref) {}

CPDF_ClipPath::~CPDF_ClipPath() {}

uint32_t CPDF_ClipPath::GetPathCount() const {
  return pdfium::CollectionSize<uint32_t>(m_Ref.GetObject()->m_PathAndTypeList);
}

CPDF_Path CPDF_ClipPath::GetPath(size_t i) const {
  return m_Ref.GetObject()->m_PathAndTypeList[i].first;
}

uint8_t CPDF_ClipPath::GetClipType(size_t i) const {
  return m_Ref.GetObject()->m_PathAndTypeList[i].second;
}

uint32_t CPDF_ClipPath::GetTextCount() const {
  return pdfium::CollectionSize<uint32_t>(m_Ref.GetObject()->m_TextList);
}

CPDF_TextObject* CPDF_ClipPath::GetText(size_t i) const {
  return m_Ref.GetObject()->m_TextList[i].get();
}

CFX_FloatRect CPDF_ClipPath::GetClipBox() const {
  CFX_FloatRect rect;
  bool bStarted = false;
  int count = GetPathCount();
  if (count) {
    rect = GetPath(0).GetBoundingBox();
    for (int i = 1; i < count; i++) {
      CFX_FloatRect path_rect = GetPath(i).GetBoundingBox();
      rect.Intersect(path_rect);
    }
    bStarted = true;
  }
  count = GetTextCount();
  if (count) {
    CFX_FloatRect layer_rect;
    bool bLayerStarted = false;
    for (int i = 0; i < count; i++) {
      CPDF_TextObject* pTextObj = GetText(i);
      if (!pTextObj) {
        if (!bStarted) {
          rect = layer_rect;
          bStarted = true;
        } else {
          rect.Intersect(layer_rect);
        }
        bLayerStarted = false;
      } else {
        if (!bLayerStarted) {
          layer_rect = CFX_FloatRect(pTextObj->GetBBox(nullptr));
          bLayerStarted = true;
        } else {
          layer_rect.Union(CFX_FloatRect(pTextObj->GetBBox(nullptr)));
        }
      }
    }
  }
  return rect;
}

void CPDF_ClipPath::AppendPath(CPDF_Path path, uint8_t type, bool bAutoMerge) {
  PathData* pData = m_Ref.GetPrivateCopy();
  if (!pData->m_PathAndTypeList.empty() && bAutoMerge) {
    const CPDF_Path& old_path = pData->m_PathAndTypeList.back().first;
    if (old_path.IsRect()) {
      CFX_PointF point0 = old_path.GetPoint(0);
      CFX_PointF point2 = old_path.GetPoint(2);
      CFX_FloatRect old_rect(point0.x, point0.y, point2.x, point2.y);
      CFX_FloatRect new_rect = path.GetBoundingBox();
      if (old_rect.Contains(new_rect))
        pData->m_PathAndTypeList.pop_back();
    }
  }
  pData->m_PathAndTypeList.push_back(std::make_pair(path, type));
}

void CPDF_ClipPath::AppendTexts(
    std::vector<std::unique_ptr<CPDF_TextObject>>* pTexts) {
  PathData* pData = m_Ref.GetPrivateCopy();
  if (pData->m_TextList.size() + pTexts->size() <= FPDF_CLIPPATH_MAX_TEXTS) {
    for (size_t i = 0; i < pTexts->size(); i++)
      pData->m_TextList.push_back(std::move((*pTexts)[i]));
    pData->m_TextList.push_back(nullptr);
  }
  pTexts->clear();
}

void CPDF_ClipPath::Transform(const CFX_Matrix& matrix) {
  PathData* pData = m_Ref.GetPrivateCopy();
  for (auto& obj : pData->m_PathAndTypeList)
    obj.first.Transform(&matrix);
  for (auto& text : pData->m_TextList) {
    if (text)
      text->Transform(matrix);
  }
}

CPDF_ClipPath::PathData::PathData() {}

CPDF_ClipPath::PathData::PathData(const PathData& that) {
  m_PathAndTypeList = that.m_PathAndTypeList;

  m_TextList.resize(that.m_TextList.size());
  for (size_t i = 0; i < that.m_TextList.size(); ++i) {
    if (that.m_TextList[i])
      m_TextList[i] = that.m_TextList[i]->Clone();
  }
}

CPDF_ClipPath::PathData::~PathData() {}

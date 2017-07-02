// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fpdfapi/page/cpdf_contentmark.h"

#include <memory>
#include <utility>

#include "core/fpdfapi/parser/cpdf_dictionary.h"
#include "third_party/base/stl_util.h"

CPDF_ContentMark::CPDF_ContentMark() {}

CPDF_ContentMark::CPDF_ContentMark(const CPDF_ContentMark& that)
    : m_Ref(that.m_Ref) {}

CPDF_ContentMark::~CPDF_ContentMark() {}

void CPDF_ContentMark::SetNull() {
  m_Ref.SetNull();
}

int CPDF_ContentMark::CountItems() const {
  return m_Ref.GetObject()->CountItems();
}

const CPDF_ContentMarkItem& CPDF_ContentMark::GetItem(int i) const {
  return m_Ref.GetObject()->GetItem(i);
}

int CPDF_ContentMark::GetMCID() const {
  const MarkData* pData = m_Ref.GetObject();
  return pData ? pData->GetMCID() : -1;
}

void CPDF_ContentMark::AddMark(const CFX_ByteString& name,
                               CPDF_Dictionary* pDict,
                               bool bDirect) {
  m_Ref.GetPrivateCopy()->AddMark(name, pDict, bDirect);
}

void CPDF_ContentMark::DeleteLastMark() {
  m_Ref.GetPrivateCopy()->DeleteLastMark();
  if (CountItems() == 0)
    m_Ref.SetNull();
}

bool CPDF_ContentMark::HasMark(const CFX_ByteStringC& mark) const {
  const MarkData* pData = m_Ref.GetObject();
  if (!pData)
    return false;

  for (int i = 0; i < pData->CountItems(); i++) {
    if (pData->GetItem(i).GetName() == mark)
      return true;
  }
  return false;
}

bool CPDF_ContentMark::LookupMark(const CFX_ByteStringC& mark,
                                  CPDF_Dictionary*& pDict) const {
  const MarkData* pData = m_Ref.GetObject();
  if (!pData)
    return false;

  for (int i = 0; i < pData->CountItems(); i++) {
    const CPDF_ContentMarkItem& item = pData->GetItem(i);
    if (item.GetName() == mark) {
      pDict = item.GetParam();
      return true;
    }
  }
  return false;
}

CPDF_ContentMark::MarkData::MarkData() {}

CPDF_ContentMark::MarkData::MarkData(const MarkData& src)
    : m_Marks(src.m_Marks) {}

CPDF_ContentMark::MarkData::~MarkData() {}

int CPDF_ContentMark::MarkData::CountItems() const {
  return pdfium::CollectionSize<int>(m_Marks);
}

CPDF_ContentMarkItem& CPDF_ContentMark::MarkData::GetItem(int index) {
  return m_Marks[index];
}

const CPDF_ContentMarkItem& CPDF_ContentMark::MarkData::GetItem(
    int index) const {
  return m_Marks[index];
}

int CPDF_ContentMark::MarkData::GetMCID() const {
  for (const auto& mark : m_Marks) {
    CPDF_Dictionary* pDict = mark.GetParam();
    if (pDict && pDict->KeyExist("MCID"))
      return pDict->GetIntegerFor("MCID");
  }
  return -1;
}

void CPDF_ContentMark::MarkData::AddMark(const CFX_ByteString& name,
                                         CPDF_Dictionary* pDict,
                                         bool bDirect) {
  CPDF_ContentMarkItem item;
  item.SetName(name);
  if (pDict) {
    if (bDirect) {
      item.SetDirectDict(
          std::unique_ptr<CPDF_Dictionary>(ToDictionary(pDict->Clone())));
    } else {
      item.SetPropertiesDict(pDict);
    }
  }
  m_Marks.push_back(std::move(item));
}

void CPDF_ContentMark::MarkData::DeleteLastMark() {
  if (!m_Marks.empty())
    m_Marks.pop_back();
}

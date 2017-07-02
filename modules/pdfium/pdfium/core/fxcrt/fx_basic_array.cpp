// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fxcrt/fx_basic.h"
#include "third_party/base/numerics/safe_math.h"

CFX_BasicArray::CFX_BasicArray(int unit_size)
    : m_pData(nullptr), m_nSize(0), m_nMaxSize(0) {
  if (unit_size < 0 || unit_size > (1 << 28)) {
    m_nUnitSize = 4;
  } else {
    m_nUnitSize = unit_size;
  }
}
CFX_BasicArray::~CFX_BasicArray() {
  FX_Free(m_pData);
}
bool CFX_BasicArray::SetSize(int nNewSize) {
  if (nNewSize <= 0) {
    FX_Free(m_pData);
    m_pData = nullptr;
    m_nSize = m_nMaxSize = 0;
    return 0 == nNewSize;
  }

  if (!m_pData) {
    pdfium::base::CheckedNumeric<int> totalSize = nNewSize;
    totalSize *= m_nUnitSize;
    if (!totalSize.IsValid()) {
      m_nSize = m_nMaxSize = 0;
      return false;
    }
    m_pData =
        FX_Alloc(uint8_t, pdfium::base::ValueOrDieForType<size_t>(totalSize));
    m_nSize = m_nMaxSize = nNewSize;
  } else if (nNewSize <= m_nMaxSize) {
    if (nNewSize > m_nSize) {
      FXSYS_memset(m_pData + m_nSize * m_nUnitSize, 0,
                   (nNewSize - m_nSize) * m_nUnitSize);
    }
    m_nSize = nNewSize;
  } else {
    int nNewMax = nNewSize < m_nMaxSize ? m_nMaxSize : nNewSize;
    pdfium::base::CheckedNumeric<int> totalSize = nNewMax;
    totalSize *= m_nUnitSize;
    if (!totalSize.IsValid() || nNewMax < m_nSize) {
      return false;
    }
    uint8_t* pNewData = FX_Realloc(
        uint8_t, m_pData, pdfium::base::ValueOrDieForType<size_t>(totalSize));
    if (!pNewData) {
      return false;
    }
    FXSYS_memset(pNewData + m_nSize * m_nUnitSize, 0,
                 (nNewMax - m_nSize) * m_nUnitSize);
    m_pData = pNewData;
    m_nSize = nNewSize;
    m_nMaxSize = nNewMax;
  }
  return true;
}
bool CFX_BasicArray::Append(const CFX_BasicArray& src) {
  int nOldSize = m_nSize;
  pdfium::base::CheckedNumeric<int> newSize = m_nSize;
  newSize += src.m_nSize;
  if (m_nUnitSize != src.m_nUnitSize || !newSize.IsValid() ||
      !SetSize(newSize.ValueOrDie())) {
    return false;
  }

  FXSYS_memcpy(m_pData + nOldSize * m_nUnitSize, src.m_pData,
               src.m_nSize * m_nUnitSize);
  return true;
}
bool CFX_BasicArray::Copy(const CFX_BasicArray& src) {
  if (!SetSize(src.m_nSize)) {
    return false;
  }
  FXSYS_memcpy(m_pData, src.m_pData, src.m_nSize * m_nUnitSize);
  return true;
}
uint8_t* CFX_BasicArray::InsertSpaceAt(int nIndex, int nCount) {
  if (nIndex < 0 || nCount <= 0) {
    return nullptr;
  }
  if (nIndex >= m_nSize) {
    if (!SetSize(nIndex + nCount)) {
      return nullptr;
    }
  } else {
    int nOldSize = m_nSize;
    if (!SetSize(m_nSize + nCount)) {
      return nullptr;
    }
    FXSYS_memmove(m_pData + (nIndex + nCount) * m_nUnitSize,
                  m_pData + nIndex * m_nUnitSize,
                  (nOldSize - nIndex) * m_nUnitSize);
    FXSYS_memset(m_pData + nIndex * m_nUnitSize, 0, nCount * m_nUnitSize);
  }
  return m_pData + nIndex * m_nUnitSize;
}
bool CFX_BasicArray::RemoveAt(int nIndex, int nCount) {
  if (nIndex < 0 || nCount <= 0 || m_nSize < nIndex + nCount) {
    return false;
  }
  int nMoveCount = m_nSize - (nIndex + nCount);
  if (nMoveCount) {
    FXSYS_memmove(m_pData + nIndex * m_nUnitSize,
                  m_pData + (nIndex + nCount) * m_nUnitSize,
                  nMoveCount * m_nUnitSize);
  }
  m_nSize -= nCount;
  return true;
}
bool CFX_BasicArray::InsertAt(int nStartIndex,
                              const CFX_BasicArray* pNewArray) {
  if (!pNewArray) {
    return false;
  }
  if (pNewArray->m_nSize == 0) {
    return true;
  }
  if (!InsertSpaceAt(nStartIndex, pNewArray->m_nSize)) {
    return false;
  }
  FXSYS_memcpy(m_pData + nStartIndex * m_nUnitSize, pNewArray->m_pData,
               pNewArray->m_nSize * m_nUnitSize);
  return true;
}
const void* CFX_BasicArray::GetDataPtr(int index) const {
  if (index < 0 || index >= m_nSize || !m_pData) {
    return nullptr;
  }
  return m_pData + index * m_nUnitSize;
}

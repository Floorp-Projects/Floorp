// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fxcodec/jbig2/JBig2_HuffmanTable.h"

#include <algorithm>
#include <vector>

#include "core/fxcodec/jbig2/JBig2_BitStream.h"
#include "core/fxcodec/jbig2/JBig2_Define.h"
#include "core/fxcodec/jbig2/JBig2_HuffmanTable_Standard.h"
#include "core/fxcrt/fx_memory.h"
#include "third_party/base/numerics/safe_math.h"

CJBig2_HuffmanTable::CJBig2_HuffmanTable(const JBig2TableLine* pTable,
                                         uint32_t nLines,
                                         bool bHTOOB)
    : m_bOK(true), HTOOB(bHTOOB), NTEMP(nLines) {
  ParseFromStandardTable(pTable);
}

CJBig2_HuffmanTable::CJBig2_HuffmanTable(CJBig2_BitStream* pStream)
    : HTOOB(false), NTEMP(0) {
  m_bOK = ParseFromCodedBuffer(pStream);
}

CJBig2_HuffmanTable::~CJBig2_HuffmanTable() {}

void CJBig2_HuffmanTable::ParseFromStandardTable(const JBig2TableLine* pTable) {
  PREFLEN.resize(NTEMP);
  RANGELEN.resize(NTEMP);
  RANGELOW.resize(NTEMP);
  for (uint32_t i = 0; i < NTEMP; ++i) {
    PREFLEN[i] = pTable[i].PREFLEN;
    RANGELEN[i] = pTable[i].RANDELEN;
    RANGELOW[i] = pTable[i].RANGELOW;
  }
  InitCodes();
}

bool CJBig2_HuffmanTable::ParseFromCodedBuffer(CJBig2_BitStream* pStream) {
  unsigned char cTemp;
  if (pStream->read1Byte(&cTemp) == -1)
    return false;

  HTOOB = !!(cTemp & 0x01);
  unsigned char HTPS = ((cTemp >> 1) & 0x07) + 1;
  unsigned char HTRS = ((cTemp >> 4) & 0x07) + 1;
  uint32_t HTLOW;
  uint32_t HTHIGH;
  if (pStream->readInteger(&HTLOW) == -1 ||
      pStream->readInteger(&HTHIGH) == -1) {
    return false;
  }

  const int low = static_cast<int>(HTLOW);
  const int high = static_cast<int>(HTHIGH);
  if (low > high)
    return false;

  ExtendBuffers(false);
  pdfium::base::CheckedNumeric<int> cur_low = low;
  do {
    if ((pStream->readNBits(HTPS, &PREFLEN[NTEMP]) == -1) ||
        (pStream->readNBits(HTRS, &RANGELEN[NTEMP]) == -1) ||
        (static_cast<size_t>(RANGELEN[NTEMP]) >= 8 * sizeof(cur_low))) {
      return false;
    }
    RANGELOW[NTEMP] = cur_low.ValueOrDie();
    cur_low += (1 << RANGELEN[NTEMP]);
    if (!cur_low.IsValid())
      return false;
    ExtendBuffers(true);
  } while (cur_low.ValueOrDie() < high);

  if (pStream->readNBits(HTPS, &PREFLEN[NTEMP]) == -1)
    return false;

  RANGELEN[NTEMP] = 32;
  RANGELOW[NTEMP] = low - 1;
  ExtendBuffers(true);

  if (pStream->readNBits(HTPS, &PREFLEN[NTEMP]) == -1)
    return false;

  RANGELEN[NTEMP] = 32;
  RANGELOW[NTEMP] = high;
  ExtendBuffers(true);

  if (HTOOB) {
    if (pStream->readNBits(HTPS, &PREFLEN[NTEMP]) == -1)
      return false;

    ++NTEMP;
  }

  InitCodes();
  return true;
}

void CJBig2_HuffmanTable::InitCodes() {
  int lenmax = 0;
  for (uint32_t i = 0; i < NTEMP; ++i)
    lenmax = std::max(PREFLEN[i], lenmax);

  CODES.resize(NTEMP);
  std::vector<int> LENCOUNT(lenmax + 1);
  std::vector<int> FIRSTCODE(lenmax + 1);
  for (int len : PREFLEN)
    ++LENCOUNT[len];

  FIRSTCODE[0] = 0;
  LENCOUNT[0] = 0;
  for (int i = 1; i <= lenmax; ++i) {
    FIRSTCODE[i] = (FIRSTCODE[i - 1] + LENCOUNT[i - 1]) << 1;
    int CURCODE = FIRSTCODE[i];
    for (uint32_t j = 0; j < NTEMP; ++j) {
      if (PREFLEN[j] == i)
        CODES[j] = CURCODE++;
    }
  }
}

void CJBig2_HuffmanTable::ExtendBuffers(bool increment) {
  if (increment)
    ++NTEMP;

  size_t size = PREFLEN.size();
  if (NTEMP < size)
    return;

  size += 16;
  ASSERT(NTEMP < size);
  PREFLEN.resize(size);
  RANGELEN.resize(size);
  RANGELOW.resize(size);
}

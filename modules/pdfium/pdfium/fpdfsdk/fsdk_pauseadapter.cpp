// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "fpdfsdk/fsdk_pauseadapter.h"

IFSDK_PAUSE_Adapter::IFSDK_PAUSE_Adapter(IFSDK_PAUSE* IPause)
    : m_IPause(IPause) {}

IFSDK_PAUSE_Adapter::~IFSDK_PAUSE_Adapter() {}

bool IFSDK_PAUSE_Adapter::NeedToPauseNow() {
  return m_IPause->NeedToPauseNow && m_IPause->NeedToPauseNow(m_IPause);
}

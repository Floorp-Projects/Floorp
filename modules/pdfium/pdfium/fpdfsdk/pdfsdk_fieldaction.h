// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef FPDFSDK_PDFSDK_FIELDACTION_H_
#define FPDFSDK_PDFSDK_FIELDACTION_H_

#include "core/fxcrt/fx_string.h"

#ifdef PDF_ENABLE_XFA
typedef enum {
  PDFSDK_XFA_Click = 0,
  PDFSDK_XFA_Full,
  PDFSDK_XFA_PreOpen,
  PDFSDK_XFA_PostOpen
} PDFSDK_XFAAActionType;
#endif  // PDF_ENABLE_XFA

struct PDFSDK_FieldAction {
  PDFSDK_FieldAction();
  PDFSDK_FieldAction(const PDFSDK_FieldAction& other) = delete;

  bool bModifier;
  bool bShift;
  int nCommitKey;
  CFX_WideString sChange;
  CFX_WideString sChangeEx;
  bool bKeyDown;
  int nSelEnd;
  int nSelStart;
  CFX_WideString sValue;
  bool bWillCommit;
  bool bFieldFull;
  bool bRC;
};

#endif  // FPDFSDK_PDFSDK_FIELDACTION_H_

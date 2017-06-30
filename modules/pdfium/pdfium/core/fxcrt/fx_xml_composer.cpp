// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fxcrt/xml_int.h"

#include "core/fxcrt/fx_xml.h"

void FX_XML_SplitQualifiedName(const CFX_ByteStringC& bsFullName,
                               CFX_ByteStringC& bsSpace,
                               CFX_ByteStringC& bsName) {
  if (bsFullName.IsEmpty())
    return;

  FX_STRSIZE iStart = bsFullName.Find(':');
  if (iStart == -1) {
    bsName = bsFullName;
  } else {
    bsSpace = bsFullName.Mid(0, iStart);
    bsName = bsFullName.Mid(iStart + 1);
  }
}

void CXML_Element::SetTag(const CFX_ByteStringC& qTagName) {
  ASSERT(!qTagName.IsEmpty());
  CFX_ByteStringC bsSpace;
  CFX_ByteStringC bsName;
  FX_XML_SplitQualifiedName(qTagName, bsSpace, bsName);
  m_QSpaceName = bsSpace;
  m_TagName = bsName;
}

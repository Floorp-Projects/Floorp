// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FPDFDOC_CSECTION_H_
#define CORE_FPDFDOC_CSECTION_H_

#include "core/fpdfdoc/clines.h"
#include "core/fpdfdoc/cpvt_sectioninfo.h"
#include "core/fpdfdoc/ctypeset.h"
#include "core/fxcrt/fx_coordinates.h"
#include "core/fxcrt/fx_system.h"

class CPDF_VariableText;
class CPVT_LineInfo;
struct CPVT_WordLine;
struct CPVT_WordPlace;

class CSection final {
 public:
  explicit CSection(CPDF_VariableText* pVT);
  ~CSection();

  void ResetAll();
  void ResetLineArray();
  void ResetWordArray();
  void ResetLinePlace();
  CPVT_WordPlace AddWord(const CPVT_WordPlace& place,
                         const CPVT_WordInfo& wordinfo);
  CPVT_WordPlace AddLine(const CPVT_LineInfo& lineinfo);
  void ClearWords(const CPVT_WordRange& PlaceRange);
  void ClearWord(const CPVT_WordPlace& place);
  CPVT_FloatRect Rearrange();
  CFX_SizeF GetSectionSize(FX_FLOAT fFontSize);
  CPVT_WordPlace GetBeginWordPlace() const;
  CPVT_WordPlace GetEndWordPlace() const;
  CPVT_WordPlace GetPrevWordPlace(const CPVT_WordPlace& place) const;
  CPVT_WordPlace GetNextWordPlace(const CPVT_WordPlace& place) const;
  void UpdateWordPlace(CPVT_WordPlace& place) const;
  CPVT_WordPlace SearchWordPlace(const CFX_PointF& point) const;
  CPVT_WordPlace SearchWordPlace(FX_FLOAT fx,
                                 const CPVT_WordPlace& lineplace) const;
  CPVT_WordPlace SearchWordPlace(FX_FLOAT fx,
                                 const CPVT_WordRange& range) const;

  CPVT_WordPlace SecPlace;
  CPVT_SectionInfo m_SecInfo;
  CLines m_LineArray;
  CPVT_ArrayTemplate<CPVT_WordInfo*> m_WordArray;

 private:
  friend class CTypeset;

  void ClearLeftWords(int32_t nWordIndex);
  void ClearRightWords(int32_t nWordIndex);
  void ClearMidWords(int32_t nBeginIndex, int32_t nEndIndex);

  CPDF_VariableText* const m_pVT;
};

#endif  // CORE_FPDFDOC_CSECTION_H_

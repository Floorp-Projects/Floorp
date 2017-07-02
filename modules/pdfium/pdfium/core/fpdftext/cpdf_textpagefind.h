// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FPDFTEXT_CPDF_TEXTPAGEFIND_H_
#define CORE_FPDFTEXT_CPDF_TEXTPAGEFIND_H_

#include <vector>

#include "core/fxcrt/fx_coordinates.h"
#include "core/fxcrt/fx_string.h"
#include "core/fxcrt/fx_system.h"

class CPDF_TextPage;

class CPDF_TextPageFind {
 public:
  explicit CPDF_TextPageFind(const CPDF_TextPage* pTextPage);
  ~CPDF_TextPageFind();

  bool FindFirst(const CFX_WideString& findwhat, int flags, int startPos = 0);
  bool FindNext();
  bool FindPrev();
  int GetCurOrder() const;
  int GetMatchedCount() const;

 protected:
  void ExtractFindWhat(const CFX_WideString& findwhat);
  bool IsMatchWholeWord(const CFX_WideString& csPageText,
                        int startPos,
                        int endPos);
  bool ExtractSubString(CFX_WideString& rString,
                        const FX_WCHAR* lpszFullString,
                        int iSubString,
                        FX_WCHAR chSep);
  CFX_WideString MakeReverse(const CFX_WideString& str);
  int GetCharIndex(int index) const;

 private:
  std::vector<uint16_t> m_CharIndex;
  const CPDF_TextPage* m_pTextPage;
  CFX_WideString m_strText;
  CFX_WideString m_findWhat;
  int m_flags;
  std::vector<CFX_WideString> m_csFindWhatArray;
  int m_findNextStart;
  int m_findPreStart;
  bool m_bMatchCase;
  bool m_bMatchWholeWord;
  int m_resStart;
  int m_resEnd;
  std::vector<CFX_FloatRect> m_resArray;
  bool m_IsFind;
};

#endif  // CORE_FPDFTEXT_CPDF_TEXTPAGEFIND_H_

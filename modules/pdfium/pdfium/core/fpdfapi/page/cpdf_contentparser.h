// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FPDFAPI_PAGE_CPDF_CONTENTPARSER_H_
#define CORE_FPDFAPI_PAGE_CPDF_CONTENTPARSER_H_

#include <memory>
#include <vector>

#include "core/fpdfapi/page/cpdf_pageobjectholder.h"
#include "core/fpdfapi/page/cpdf_streamcontentparser.h"

class CPDF_AllStates;
class CPDF_Form;
class CPDF_Page;
class CPDF_StreamAcc;
class CPDF_Type3Char;

class CPDF_ContentParser {
 public:
  enum ParseStatus { Ready, ToBeContinued, Done };

  CPDF_ContentParser();
  ~CPDF_ContentParser();

  ParseStatus GetStatus() const { return m_Status; }
  void Start(CPDF_Page* pPage);
  void Start(CPDF_Form* pForm,
             CPDF_AllStates* pGraphicStates,
             const CFX_Matrix* pParentMatrix,
             CPDF_Type3Char* pType3Char,
             int level);
  void Continue(IFX_Pause* pPause);

 private:
  enum InternalStage {
    STAGE_GETCONTENT = 1,
    STAGE_PARSE,
    STAGE_CHECKCLIP,
  };

  ParseStatus m_Status;
  InternalStage m_InternalStage;
  CPDF_PageObjectHolder* m_pObjectHolder;
  bool m_bForm;
  CPDF_Type3Char* m_pType3Char;
  uint32_t m_nStreams;
  std::unique_ptr<CPDF_StreamAcc> m_pSingleStream;
  std::vector<std::unique_ptr<CPDF_StreamAcc>> m_StreamArray;
  uint8_t* m_pData;
  uint32_t m_Size;
  uint32_t m_CurrentOffset;
  std::unique_ptr<CPDF_StreamContentParser> m_pParser;
};

#endif  // CORE_FPDFAPI_PAGE_CPDF_CONTENTPARSER_H_

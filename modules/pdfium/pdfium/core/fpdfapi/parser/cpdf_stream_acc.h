// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FPDFAPI_PARSER_CPDF_STREAM_ACC_H_
#define CORE_FPDFAPI_PARSER_CPDF_STREAM_ACC_H_

#include <memory>

#include "core/fpdfapi/parser/cpdf_dictionary.h"
#include "core/fpdfapi/parser/cpdf_stream.h"
#include "core/fxcrt/fx_string.h"
#include "core/fxcrt/fx_system.h"

class CPDF_StreamAcc {
 public:
  CPDF_StreamAcc();
  ~CPDF_StreamAcc();

  CPDF_StreamAcc(const CPDF_StreamAcc&) = delete;
  CPDF_StreamAcc& operator=(const CPDF_StreamAcc&) = delete;

  void LoadAllData(const CPDF_Stream* pStream,
                   bool bRawAccess = false,
                   uint32_t estimated_size = 0,
                   bool bImageAcc = false);

  const CPDF_Stream* GetStream() const { return m_pStream; }
  CPDF_Dictionary* GetDict() const {
    return m_pStream ? m_pStream->GetDict() : nullptr;
  }

  const uint8_t* GetData() const;
  uint32_t GetSize() const;
  const CFX_ByteString& GetImageDecoder() const { return m_ImageDecoder; }
  const CPDF_Dictionary* GetImageParam() const { return m_pImageParam; }
  std::unique_ptr<uint8_t, FxFreeDeleter> DetachData();

 protected:
  uint8_t* m_pData;
  uint32_t m_dwSize;
  bool m_bNewBuf;
  CFX_ByteString m_ImageDecoder;
  CPDF_Dictionary* m_pImageParam;
  const CPDF_Stream* m_pStream;
  uint8_t* m_pSrcData;
};

#endif  // CORE_FPDFAPI_PARSER_CPDF_STREAM_ACC_H_

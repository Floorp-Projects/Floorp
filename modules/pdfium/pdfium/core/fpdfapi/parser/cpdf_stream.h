// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FPDFAPI_PARSER_CPDF_STREAM_H_
#define CORE_FPDFAPI_PARSER_CPDF_STREAM_H_

#include <memory>
#include <set>

#include "core/fpdfapi/parser/cpdf_dictionary.h"
#include "core/fpdfapi/parser/cpdf_object.h"
#include "core/fxcrt/fx_basic.h"

class CPDF_Stream : public CPDF_Object {
 public:
  CPDF_Stream();

  // Takes ownership of |pData|.
  CPDF_Stream(std::unique_ptr<uint8_t, FxFreeDeleter> pData,
              uint32_t size,
              std::unique_ptr<CPDF_Dictionary> pDict);

  ~CPDF_Stream() override;

  // CPDF_Object:
  Type GetType() const override;
  std::unique_ptr<CPDF_Object> Clone() const override;
  CPDF_Dictionary* GetDict() const override;
  CFX_WideString GetUnicodeText() const override;
  bool IsStream() const override;
  CPDF_Stream* AsStream() override;
  const CPDF_Stream* AsStream() const override;

  uint32_t GetRawSize() const { return m_dwSize; }
  uint8_t* GetRawData() const { return m_pDataBuf.get(); }

  // Does not takes onwership of |pData|, copies into internally-owned buffer.
  void SetData(const uint8_t* pData, uint32_t size);

  void InitStream(const uint8_t* pData,
                  uint32_t size,
                  std::unique_ptr<CPDF_Dictionary> pDict);
  void InitStreamFromFile(const CFX_RetainPtr<IFX_SeekableReadStream>& pFile,
                          std::unique_ptr<CPDF_Dictionary> pDict);

  bool ReadRawData(FX_FILESIZE start_pos,
                   uint8_t* pBuf,
                   uint32_t buf_size) const;

  bool IsMemoryBased() const { return m_bMemoryBased; }
  bool HasFilter() const;

 protected:
  std::unique_ptr<CPDF_Object> CloneNonCyclic(
      bool bDirect,
      std::set<const CPDF_Object*>* pVisited) const override;

  bool m_bMemoryBased = true;
  uint32_t m_dwSize = 0;
  std::unique_ptr<CPDF_Dictionary> m_pDict;
  std::unique_ptr<uint8_t, FxFreeDeleter> m_pDataBuf;
  CFX_RetainPtr<IFX_SeekableReadStream> m_pFile;
};

inline CPDF_Stream* ToStream(CPDF_Object* obj) {
  return obj ? obj->AsStream() : nullptr;
}

inline const CPDF_Stream* ToStream(const CPDF_Object* obj) {
  return obj ? obj->AsStream() : nullptr;
}

inline std::unique_ptr<CPDF_Stream> ToStream(std::unique_ptr<CPDF_Object> obj) {
  CPDF_Stream* pStream = ToStream(obj.get());
  if (!pStream)
    return nullptr;
  obj.release();
  return std::unique_ptr<CPDF_Stream>(pStream);
}

#endif  // CORE_FPDFAPI_PARSER_CPDF_STREAM_H_

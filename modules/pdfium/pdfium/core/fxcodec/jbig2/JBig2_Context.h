// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FXCODEC_JBIG2_JBIG2_CONTEXT_H_
#define CORE_FXCODEC_JBIG2_JBIG2_CONTEXT_H_

#include <list>
#include <memory>
#include <utility>
#include <vector>

#include "core/fpdfapi/parser/cpdf_object.h"
#include "core/fxcodec/fx_codec_def.h"
#include "core/fxcodec/jbig2/JBig2_Page.h"
#include "core/fxcodec/jbig2/JBig2_Segment.h"

class CJBig2_ArithDecoder;
class CJBig2_GRDProc;
class CPDF_StreamAcc;
class IFX_Pause;

// Cache is keyed by the ObjNum of a stream and an index within the stream.
using CJBig2_CacheKey = std::pair<uint32_t, uint32_t>;
using CJBig2_CachePair =
    std::pair<CJBig2_CacheKey, std::unique_ptr<CJBig2_SymbolDict>>;

#define JBIG2_SUCCESS 0
#define JBIG2_FAILED -1
#define JBIG2_ERROR_TOO_SHORT -2
#define JBIG2_ERROR_FATAL -3
#define JBIG2_END_OF_PAGE 2
#define JBIG2_END_OF_FILE 3
#define JBIG2_ERROR_FILE_FORMAT -4
#define JBIG2_ERROR_STREAM_TYPE -5
#define JBIG2_ERROR_LIMIT -6
#define JBIG2_MIN_SEGMENT_SIZE 11

class CJBig2_Context {
 public:
  CJBig2_Context(CPDF_StreamAcc* pGlobalStream,
                 CPDF_StreamAcc* pSrcStream,
                 std::list<CJBig2_CachePair>* pSymbolDictCache,
                 IFX_Pause* pPause,
                 bool bIsGlobal);
  ~CJBig2_Context();

  int32_t getFirstPage(uint8_t* pBuf,
                       int32_t width,
                       int32_t height,
                       int32_t stride,
                       IFX_Pause* pPause);

  int32_t Continue(IFX_Pause* pPause);
  FXCODEC_STATUS GetProcessingStatus() { return m_ProcessingStatus; }

 private:
  int32_t decode_SquentialOrgnazation(IFX_Pause* pPause);
  int32_t decode_EmbedOrgnazation(IFX_Pause* pPause);
  int32_t decode_RandomOrgnazation_FirstPage(IFX_Pause* pPause);
  int32_t decode_RandomOrgnazation(IFX_Pause* pPause);

  CJBig2_Segment* findSegmentByNumber(uint32_t dwNumber);
  CJBig2_Segment* findReferredSegmentByTypeAndIndex(CJBig2_Segment* pSegment,
                                                    uint8_t cType,
                                                    int32_t nIndex);

  int32_t parseSegmentHeader(CJBig2_Segment* pSegment);
  int32_t parseSegmentData(CJBig2_Segment* pSegment, IFX_Pause* pPause);
  int32_t ProcessingParseSegmentData(CJBig2_Segment* pSegment,
                                     IFX_Pause* pPause);
  int32_t parseSymbolDict(CJBig2_Segment* pSegment, IFX_Pause* pPause);
  int32_t parseTextRegion(CJBig2_Segment* pSegment);
  int32_t parsePatternDict(CJBig2_Segment* pSegment, IFX_Pause* pPause);
  int32_t parseHalftoneRegion(CJBig2_Segment* pSegment, IFX_Pause* pPause);
  int32_t parseGenericRegion(CJBig2_Segment* pSegment, IFX_Pause* pPause);
  int32_t parseGenericRefinementRegion(CJBig2_Segment* pSegment);
  int32_t parseTable(CJBig2_Segment* pSegment);
  int32_t parseRegionInfo(JBig2RegionInfo* pRI);

  JBig2HuffmanCode* decodeSymbolIDHuffmanTable(CJBig2_BitStream* pStream,
                                               uint32_t SBNUMSYMS);

  void huffman_assign_code(int* CODES, int* PREFLEN, int NTEMP);
  void huffman_assign_code(JBig2HuffmanCode* SBSYMCODES, int NTEMP);

  std::unique_ptr<CJBig2_Context> m_pGlobalContext;
  std::unique_ptr<CJBig2_BitStream> m_pStream;
  std::vector<std::unique_ptr<CJBig2_Segment>> m_SegmentList;
  std::vector<std::unique_ptr<JBig2PageInfo>> m_PageInfoList;
  std::unique_ptr<CJBig2_Image> m_pPage;
  size_t m_nSegmentDecoded;
  bool m_bInPage;
  bool m_bBufSpecified;
  int32_t m_PauseStep;
  IFX_Pause* const m_pPause;
  FXCODEC_STATUS m_ProcessingStatus;
  std::vector<JBig2ArithCtx> m_gbContext;
  std::unique_ptr<CJBig2_ArithDecoder> m_pArithDecoder;
  std::unique_ptr<CJBig2_GRDProc> m_pGRD;
  std::unique_ptr<CJBig2_Segment> m_pSegment;
  uint32_t m_dwOffset;
  JBig2RegionInfo m_ri;
  std::list<CJBig2_CachePair>* const m_pSymbolDictCache;
  bool m_bIsGlobal;
};

#endif  // CORE_FXCODEC_JBIG2_JBIG2_CONTEXT_H_

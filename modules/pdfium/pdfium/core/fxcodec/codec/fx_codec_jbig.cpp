// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fxcodec/codec/ccodec_jbig2module.h"

#include <list>
#include <memory>

#include "core/fpdfapi/parser/cpdf_stream_acc.h"
#include "core/fxcodec/JBig2_DocumentContext.h"
#include "core/fxcodec/jbig2/JBig2_Context.h"
#include "core/fxcodec/jbig2/JBig2_Image.h"
#include "core/fxcrt/fx_memory.h"
#include "third_party/base/ptr_util.h"

JBig2_DocumentContext::JBig2_DocumentContext() {}

JBig2_DocumentContext::~JBig2_DocumentContext() {}

JBig2_DocumentContext* GetJBig2DocumentContext(
    std::unique_ptr<JBig2_DocumentContext>* pContextHolder) {
  if (!pContextHolder->get())
    *pContextHolder = pdfium::MakeUnique<JBig2_DocumentContext>();
  return pContextHolder->get();
}

CCodec_Jbig2Context::CCodec_Jbig2Context()
    : m_width(0),
      m_height(0),
      m_pGlobalStream(nullptr),
      m_pSrcStream(nullptr),
      m_dest_buf(0),
      m_dest_pitch(0),
      m_pPause(nullptr) {}

CCodec_Jbig2Context::~CCodec_Jbig2Context() {}

CCodec_Jbig2Module::~CCodec_Jbig2Module() {}

FXCODEC_STATUS CCodec_Jbig2Module::StartDecode(
    CCodec_Jbig2Context* pJbig2Context,
    std::unique_ptr<JBig2_DocumentContext>* pContextHolder,
    uint32_t width,
    uint32_t height,
    CPDF_StreamAcc* src_stream,
    CPDF_StreamAcc* global_stream,
    uint8_t* dest_buf,
    uint32_t dest_pitch,
    IFX_Pause* pPause) {
  if (!pJbig2Context)
    return FXCODEC_STATUS_ERR_PARAMS;

  JBig2_DocumentContext* pJBig2DocumentContext =
      GetJBig2DocumentContext(pContextHolder);
  pJbig2Context->m_width = width;
  pJbig2Context->m_height = height;
  pJbig2Context->m_pSrcStream = src_stream;
  pJbig2Context->m_pGlobalStream = global_stream;
  pJbig2Context->m_dest_buf = dest_buf;
  pJbig2Context->m_dest_pitch = dest_pitch;
  pJbig2Context->m_pPause = pPause;
  FXSYS_memset(dest_buf, 0, height * dest_pitch);
  pJbig2Context->m_pContext = pdfium::MakeUnique<CJBig2_Context>(
      global_stream, src_stream, pJBig2DocumentContext->GetSymbolDictCache(),
      pPause, false);
  if (!pJbig2Context->m_pContext)
    return FXCODEC_STATUS_ERROR;

  int ret = pJbig2Context->m_pContext->getFirstPage(dest_buf, width, height,
                                                    dest_pitch, pPause);
  if (pJbig2Context->m_pContext->GetProcessingStatus() ==
      FXCODEC_STATUS_DECODE_FINISH) {
    pJbig2Context->m_pContext.reset();
    if (ret != JBIG2_SUCCESS)
      return FXCODEC_STATUS_ERROR;

    int dword_size = height * dest_pitch / 4;
    uint32_t* dword_buf = (uint32_t*)dest_buf;
    for (int i = 0; i < dword_size; i++)
      dword_buf[i] = ~dword_buf[i];
    return FXCODEC_STATUS_DECODE_FINISH;
  }
  return pJbig2Context->m_pContext->GetProcessingStatus();
}

FXCODEC_STATUS CCodec_Jbig2Module::ContinueDecode(
    CCodec_Jbig2Context* pJbig2Context,
    IFX_Pause* pPause) {
  int ret = pJbig2Context->m_pContext->Continue(pPause);
  if (pJbig2Context->m_pContext->GetProcessingStatus() !=
      FXCODEC_STATUS_DECODE_FINISH) {
    return pJbig2Context->m_pContext->GetProcessingStatus();
  }
  pJbig2Context->m_pContext.reset();
  if (ret != JBIG2_SUCCESS)
    return FXCODEC_STATUS_ERROR;

  int dword_size = pJbig2Context->m_height * pJbig2Context->m_dest_pitch / 4;
  uint32_t* dword_buf = (uint32_t*)pJbig2Context->m_dest_buf;
  for (int i = 0; i < dword_size; i++)
    dword_buf[i] = ~dword_buf[i];
  return FXCODEC_STATUS_DECODE_FINISH;
}

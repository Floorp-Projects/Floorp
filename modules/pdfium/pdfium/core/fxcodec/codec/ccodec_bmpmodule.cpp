// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fxcodec/codec/ccodec_bmpmodule.h"

#include "core/fxcodec/codec/codec_int.h"
#include "core/fxcodec/fx_codec.h"
#include "core/fxcodec/lbmp/fx_bmp.h"
#include "core/fxge/fx_dib.h"

struct FXBMP_Context {
  bmp_decompress_struct_p bmp_ptr;
  void* parent_ptr;

  void* (*m_AllocFunc)(unsigned int);
  void (*m_FreeFunc)(void*);
};
extern "C" {
static void* bmp_alloc_func(unsigned int size) {
  return FX_Alloc(char, size);
}
static void bmp_free_func(void* p) {
  FX_Free(p);
}
};
static void bmp_error_data(bmp_decompress_struct_p bmp_ptr,
                           const FX_CHAR* err_msg) {
  FXSYS_strncpy((char*)bmp_ptr->err_ptr, err_msg, BMP_MAX_ERROR_SIZE - 1);
  longjmp(bmp_ptr->jmpbuf, 1);
}
static void bmp_read_scanline(bmp_decompress_struct_p bmp_ptr,
                              int32_t row_num,
                              uint8_t* row_buf) {
  FXBMP_Context* p = (FXBMP_Context*)bmp_ptr->context_ptr;
  CCodec_BmpModule* pModule = (CCodec_BmpModule*)p->parent_ptr;
  pModule->GetDelegate()->BmpReadScanline(row_num, row_buf);
}
static bool bmp_get_data_position(bmp_decompress_struct_p bmp_ptr,
                                  uint32_t rcd_pos) {
  FXBMP_Context* p = (FXBMP_Context*)bmp_ptr->context_ptr;
  CCodec_BmpModule* pModule = (CCodec_BmpModule*)p->parent_ptr;
  return pModule->GetDelegate()->BmpInputImagePositionBuf(rcd_pos);
}

CCodec_BmpModule::CCodec_BmpModule() {
  memset(m_szLastError, 0, sizeof(m_szLastError));
}

CCodec_BmpModule::~CCodec_BmpModule() {}

FXBMP_Context* CCodec_BmpModule::Start() {
  FXBMP_Context* p = FX_Alloc(FXBMP_Context, 1);
  if (!p)
    return nullptr;

  FXSYS_memset(p, 0, sizeof(FXBMP_Context));
  if (!p)
    return nullptr;

  p->m_AllocFunc = bmp_alloc_func;
  p->m_FreeFunc = bmp_free_func;
  p->bmp_ptr = nullptr;
  p->parent_ptr = (void*)this;
  p->bmp_ptr = bmp_create_decompress();
  if (!p->bmp_ptr) {
    FX_Free(p);
    return nullptr;
  }
  p->bmp_ptr->context_ptr = (void*)p;
  p->bmp_ptr->err_ptr = m_szLastError;
  p->bmp_ptr->bmp_error_fn = bmp_error_data;
  p->bmp_ptr->bmp_get_row_fn = bmp_read_scanline;
  p->bmp_ptr->bmp_get_data_position_fn = bmp_get_data_position;
  return p;
}

void CCodec_BmpModule::Finish(FXBMP_Context* ctx) {
  if (ctx) {
    bmp_destroy_decompress(&ctx->bmp_ptr);
    ctx->m_FreeFunc(ctx);
  }
}
int32_t CCodec_BmpModule::ReadHeader(FXBMP_Context* ctx,
                                     int32_t* width,
                                     int32_t* height,
                                     bool* tb_flag,
                                     int32_t* components,
                                     int32_t* pal_num,
                                     uint32_t** pal_pp,
                                     CFX_DIBAttribute* pAttribute) {
  if (setjmp(ctx->bmp_ptr->jmpbuf)) {
    return 0;
  }
  int32_t ret = bmp_read_header(ctx->bmp_ptr);
  if (ret != 1) {
    return ret;
  }
  *width = ctx->bmp_ptr->width;
  *height = ctx->bmp_ptr->height;
  *tb_flag = ctx->bmp_ptr->imgTB_flag;
  *components = ctx->bmp_ptr->components;
  *pal_num = ctx->bmp_ptr->pal_num;
  *pal_pp = ctx->bmp_ptr->pal_ptr;
  if (pAttribute) {
    pAttribute->m_wDPIUnit = FXCODEC_RESUNIT_METER;
    pAttribute->m_nXDPI = ctx->bmp_ptr->dpi_x;
    pAttribute->m_nYDPI = ctx->bmp_ptr->dpi_y;
    pAttribute->m_nBmpCompressType = ctx->bmp_ptr->compress_flag;
  }
  return 1;
}

int32_t CCodec_BmpModule::LoadImage(FXBMP_Context* ctx) {
  if (setjmp(ctx->bmp_ptr->jmpbuf))
    return 0;
  return bmp_decode_image(ctx->bmp_ptr);
}

uint32_t CCodec_BmpModule::GetAvailInput(FXBMP_Context* ctx,
                                         uint8_t** avail_buf_ptr) {
  return bmp_get_avail_input(ctx->bmp_ptr, avail_buf_ptr);
}

void CCodec_BmpModule::Input(FXBMP_Context* ctx,
                             const uint8_t* src_buf,
                             uint32_t src_size) {
  bmp_input_buffer(ctx->bmp_ptr, (uint8_t*)src_buf, src_size);
}

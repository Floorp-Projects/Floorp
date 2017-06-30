// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fxcodec/codec/ccodec_gifmodule.h"

#include "core/fxcodec/codec/codec_int.h"
#include "core/fxcodec/fx_codec.h"
#include "core/fxcodec/lgif/fx_gif.h"
#include "core/fxge/fx_dib.h"

struct FXGIF_Context {
  gif_decompress_struct_p gif_ptr;
  void* parent_ptr;

  void* (*m_AllocFunc)(unsigned int);
  void (*m_FreeFunc)(void*);
};

extern "C" {
static void* gif_alloc_func(unsigned int size) {
  return FX_Alloc(char, size);
}
static void gif_free_func(void* p) {
  FX_Free(p);
}
};

static void gif_error_data(gif_decompress_struct_p gif_ptr,
                           const FX_CHAR* err_msg) {
  FXSYS_strncpy((char*)gif_ptr->err_ptr, err_msg, GIF_MAX_ERROR_SIZE - 1);
  longjmp(gif_ptr->jmpbuf, 1);
}

static uint8_t* gif_ask_buf_for_pal(gif_decompress_struct_p gif_ptr,
                                    int32_t pal_size) {
  FXGIF_Context* p = (FXGIF_Context*)gif_ptr->context_ptr;
  CCodec_GifModule* pModule = (CCodec_GifModule*)p->parent_ptr;
  return pModule->GetDelegate()->GifAskLocalPaletteBuf(
      gif_get_frame_num(gif_ptr), pal_size);
}

static void gif_record_current_position(gif_decompress_struct_p gif_ptr,
                                        uint32_t* cur_pos_ptr) {
  FXGIF_Context* p = (FXGIF_Context*)gif_ptr->context_ptr;
  CCodec_GifModule* pModule = (CCodec_GifModule*)p->parent_ptr;
  pModule->GetDelegate()->GifRecordCurrentPosition(*cur_pos_ptr);
}

static void gif_read_scanline(gif_decompress_struct_p gif_ptr,
                              int32_t row_num,
                              uint8_t* row_buf) {
  FXGIF_Context* p = (FXGIF_Context*)gif_ptr->context_ptr;
  CCodec_GifModule* pModule = (CCodec_GifModule*)p->parent_ptr;
  pModule->GetDelegate()->GifReadScanline(row_num, row_buf);
}

static bool gif_get_record_position(gif_decompress_struct_p gif_ptr,
                                    uint32_t cur_pos,
                                    int32_t left,
                                    int32_t top,
                                    int32_t width,
                                    int32_t height,
                                    int32_t pal_num,
                                    void* pal_ptr,
                                    int32_t delay_time,
                                    bool user_input,
                                    int32_t trans_index,
                                    int32_t disposal_method,
                                    bool interlace) {
  FXGIF_Context* p = (FXGIF_Context*)gif_ptr->context_ptr;
  CCodec_GifModule* pModule = (CCodec_GifModule*)p->parent_ptr;
  return pModule->GetDelegate()->GifInputRecordPositionBuf(
      cur_pos, FX_RECT(left, top, left + width, top + height), pal_num, pal_ptr,
      delay_time, user_input, trans_index, disposal_method, interlace);
}

CCodec_GifModule::CCodec_GifModule() {
  memset(m_szLastError, 0, sizeof(m_szLastError));
}

CCodec_GifModule::~CCodec_GifModule() {}

FXGIF_Context* CCodec_GifModule::Start() {
  FXGIF_Context* p = FX_Alloc(FXGIF_Context, 1);
  if (!p)
    return nullptr;

  FXSYS_memset(p, 0, sizeof(FXGIF_Context));
  p->m_AllocFunc = gif_alloc_func;
  p->m_FreeFunc = gif_free_func;
  p->gif_ptr = nullptr;
  p->parent_ptr = (void*)this;
  p->gif_ptr = gif_create_decompress();
  if (!p->gif_ptr) {
    FX_Free(p);
    return nullptr;
  }
  p->gif_ptr->context_ptr = (void*)p;
  p->gif_ptr->err_ptr = m_szLastError;
  p->gif_ptr->gif_error_fn = gif_error_data;
  p->gif_ptr->gif_ask_buf_for_pal_fn = gif_ask_buf_for_pal;
  p->gif_ptr->gif_record_current_position_fn = gif_record_current_position;
  p->gif_ptr->gif_get_row_fn = gif_read_scanline;
  p->gif_ptr->gif_get_record_position_fn = gif_get_record_position;
  return p;
}

void CCodec_GifModule::Finish(FXGIF_Context* ctx) {
  if (ctx) {
    gif_destroy_decompress(&ctx->gif_ptr);
    ctx->m_FreeFunc(ctx);
  }
}

int32_t CCodec_GifModule::ReadHeader(FXGIF_Context* ctx,
                                     int* width,
                                     int* height,
                                     int* pal_num,
                                     void** pal_pp,
                                     int* bg_index,
                                     CFX_DIBAttribute* pAttribute) {
  if (setjmp(ctx->gif_ptr->jmpbuf))
    return 0;

  int32_t ret = gif_read_header(ctx->gif_ptr);
  if (ret != 1)
    return ret;

  *width = ctx->gif_ptr->width;
  *height = ctx->gif_ptr->height;
  *pal_num = ctx->gif_ptr->global_pal_num;
  *pal_pp = ctx->gif_ptr->global_pal_ptr;
  *bg_index = ctx->gif_ptr->bc_index;
  return 1;
}

int32_t CCodec_GifModule::LoadFrameInfo(FXGIF_Context* ctx, int* frame_num) {
  if (setjmp(ctx->gif_ptr->jmpbuf))
    return 0;

  int32_t ret = gif_get_frame(ctx->gif_ptr);
  if (ret != 1)
    return ret;

  *frame_num = gif_get_frame_num(ctx->gif_ptr);
  return 1;
}

int32_t CCodec_GifModule::LoadFrame(FXGIF_Context* ctx,
                                    int frame_num,
                                    CFX_DIBAttribute* pAttribute) {
  if (setjmp(ctx->gif_ptr->jmpbuf))
    return 0;

  int32_t ret = gif_load_frame(ctx->gif_ptr, frame_num);
  if (ret == 1) {
    if (pAttribute) {
      pAttribute->m_nGifLeft =
          (*ctx->gif_ptr->img_ptr_arr_ptr)[frame_num]->image_info_ptr->left;
      pAttribute->m_nGifTop =
          (*ctx->gif_ptr->img_ptr_arr_ptr)[frame_num]->image_info_ptr->top;
      pAttribute->m_fAspectRatio = ctx->gif_ptr->pixel_aspect;
      if (ctx->gif_ptr->cmt_data_ptr) {
        const uint8_t* buf =
            (const uint8_t*)ctx->gif_ptr->cmt_data_ptr->GetBuffer(0);
        uint32_t len = ctx->gif_ptr->cmt_data_ptr->GetLength();
        if (len > 21) {
          uint8_t size = *buf++;
          if (size) {
            pAttribute->m_strAuthor = CFX_ByteString(buf, size);
          } else {
            pAttribute->m_strAuthor.clear();
          }
          buf += size;
          size = *buf++;
          if (size == 20) {
            FXSYS_memcpy(pAttribute->m_strTime, buf, size);
          }
        }
      }
    }
  }
  return ret;
}

uint32_t CCodec_GifModule::GetAvailInput(FXGIF_Context* ctx,
                                         uint8_t** avail_buf_ptr) {
  return gif_get_avail_input(ctx->gif_ptr, avail_buf_ptr);
}

void CCodec_GifModule::Input(FXGIF_Context* ctx,
                             const uint8_t* src_buf,
                             uint32_t src_size) {
  gif_input_buffer(ctx->gif_ptr, (uint8_t*)src_buf, src_size);
}

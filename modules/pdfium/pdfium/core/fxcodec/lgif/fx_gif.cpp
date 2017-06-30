// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fxcodec/lgif/fx_gif.h"

#include "core/fxcodec/lbmp/fx_bmp.h"
#include "third_party/base/stl_util.h"

void CGifLZWDecoder::Input(uint8_t* src_buf, uint32_t src_size) {
  next_in = src_buf;
  avail_in = src_size;
}

uint32_t CGifLZWDecoder::GetAvailInput() {
  return avail_in;
}

CGifLZWDecoder::CGifLZWDecoder(FX_CHAR* error_ptr)
    : code_size(0),
      code_size_cur(0),
      code_clear(0),
      code_end(0),
      code_next(0),
      code_first(0),
      stack_size(0),
      code_old(0),
      next_in(nullptr),
      avail_in(0),
      bits_left(0),
      code_store(0),
      err_msg_ptr(error_ptr) {}

CGifLZWDecoder::~CGifLZWDecoder() {}

void CGifLZWDecoder::InitTable(uint8_t code_len) {
  code_size = code_len;
  ASSERT(code_size < 32);
  code_clear = 1 << code_size;
  code_end = code_clear + 1;
  bits_left = 0;
  code_store = 0;
  next_in = nullptr;
  avail_in = 0;
  stack_size = 0;
  code_first = 0;
  ClearTable();
}
void CGifLZWDecoder::ClearTable() {
  code_size_cur = code_size + 1;
  code_next = code_end + 1;
  code_old = (uint16_t)-1;
  FXSYS_memset(code_table, 0, sizeof(tag_Table) * GIF_MAX_LZW_CODE);
  FXSYS_memset(stack, 0, GIF_MAX_LZW_CODE);
  for (uint16_t i = 0; i < code_clear; i++) {
    code_table[i].suffix = (uint8_t)i;
  }
}
void CGifLZWDecoder::DecodeString(uint16_t code) {
  stack_size = 0;
  while (true) {
    ASSERT(code <= code_next);
    if (code < code_clear || code > code_next) {
      break;
    }
    stack[GIF_MAX_LZW_CODE - 1 - stack_size++] = code_table[code].suffix;
    code = code_table[code].prefix;
  }
  stack[GIF_MAX_LZW_CODE - 1 - stack_size++] = (uint8_t)code;
  code_first = (uint8_t)code;
}
void CGifLZWDecoder::AddCode(uint16_t prefix_code, uint8_t append_char) {
  if (code_next == GIF_MAX_LZW_CODE) {
    return;
  }
  code_table[code_next].prefix = prefix_code;
  code_table[code_next].suffix = append_char;
  if (++code_next < GIF_MAX_LZW_CODE) {
    if (code_next >> code_size_cur) {
      code_size_cur++;
    }
  }
}
int32_t CGifLZWDecoder::Decode(uint8_t* des_buf, uint32_t& des_size) {
  if (des_size == 0) {
    return 3;
  }
  uint32_t i = 0;
  if (stack_size != 0) {
    if (des_size < stack_size) {
      FXSYS_memcpy(des_buf, &stack[GIF_MAX_LZW_CODE - stack_size], des_size);
      stack_size -= (uint16_t)des_size;
      return 3;
    }
    FXSYS_memcpy(des_buf, &stack[GIF_MAX_LZW_CODE - stack_size], stack_size);
    des_buf += stack_size;
    i += stack_size;
    stack_size = 0;
  }
  uint16_t code = 0;
  while (i <= des_size && (avail_in > 0 || bits_left >= code_size_cur)) {
    if (code_size_cur > 12) {
      if (err_msg_ptr) {
        FXSYS_strncpy(err_msg_ptr, "Code Length Out Of Range",
                      GIF_MAX_ERROR_SIZE - 1);
      }
      return 0;
    }
    if (avail_in > 0) {
      if (bits_left > 31) {
        if (err_msg_ptr)
          FXSYS_strncpy(err_msg_ptr, "Decode Error", GIF_MAX_ERROR_SIZE - 1);
        return 0;
      }
      pdfium::base::CheckedNumeric<uint32_t> safe_code = *next_in++;
      safe_code <<= bits_left;
      safe_code |= code_store;
      if (!safe_code.IsValid()) {
        if (err_msg_ptr) {
          FXSYS_strncpy(err_msg_ptr, "Code Store Out Of Range",
                        GIF_MAX_ERROR_SIZE - 1);
        }
        return 0;
      }
      code_store = safe_code.ValueOrDie();
      avail_in--;
      bits_left += 8;
    }
    while (bits_left >= code_size_cur) {
      code = (uint16_t)code_store & ((1 << code_size_cur) - 1);
      code_store >>= code_size_cur;
      bits_left -= code_size_cur;
      if (code == code_clear) {
        ClearTable();
        continue;
      } else if (code >= code_end) {
        des_size = i;
        return 1;
      } else {
        if (code_old != (uint16_t)-1) {
          if (code_next < GIF_MAX_LZW_CODE) {
            if (code == code_next) {
              AddCode(code_old, code_first);
              DecodeString(code);
            } else if (code > code_next) {
              if (err_msg_ptr) {
                FXSYS_strncpy(err_msg_ptr, "Decode Error, Out Of Range",
                              GIF_MAX_ERROR_SIZE - 1);
              }
              return 0;
            } else {
              DecodeString(code);
              uint8_t append_char = stack[GIF_MAX_LZW_CODE - stack_size];
              AddCode(code_old, append_char);
            }
          }
        } else {
          DecodeString(code);
        }
        code_old = code;
        if (i + stack_size > des_size) {
          FXSYS_memcpy(des_buf, &stack[GIF_MAX_LZW_CODE - stack_size],
                       des_size - i);
          stack_size -= (uint16_t)(des_size - i);
          return 3;
        }
        FXSYS_memcpy(des_buf, &stack[GIF_MAX_LZW_CODE - stack_size],
                     stack_size);
        des_buf += stack_size;
        i += stack_size;
        stack_size = 0;
      }
    }
  }
  if (avail_in == 0) {
    des_size = i;
    return 2;
  }
  return 0;
}
static bool gif_grow_buf(uint8_t*& dst_buf, uint32_t& dst_len, uint32_t size) {
  if (dst_len < size) {
    uint32_t len_org = dst_len;
    while (dst_buf && dst_len < size) {
      dst_len <<= 1;
      // TODO(thestig): Probably should be a try-realloc here.
      dst_buf = FX_Realloc(uint8_t, dst_buf, dst_len);
    }
    if (!dst_buf) {
      dst_len = size;
      dst_buf = FX_Realloc(uint8_t, dst_buf, dst_len);
    }
    FXSYS_memset(dst_buf + len_org, 0, dst_len - len_org);
    return !!dst_buf;
  }
  return true;
}
static inline void gif_cut_index(uint8_t& val,
                                 uint32_t index,
                                 uint8_t index_bit,
                                 uint8_t index_bit_use,
                                 uint8_t bit_use) {
  uint32_t cut = ((1 << (index_bit - index_bit_use)) - 1) << index_bit_use;
  val |= ((index & cut) >> index_bit_use) << bit_use;
}
static inline uint8_t gif_cut_buf(const uint8_t* buf,
                                  uint32_t& offset,
                                  uint8_t bit_cut,
                                  uint8_t& bit_offset,
                                  uint32_t& bit_num) {
  if (bit_cut != 8) {
    uint16_t index = 0;
    index |= ((1 << bit_cut) - 1) << (7 - bit_offset);
    uint8_t ret = ((index & buf[offset]) >> (7 - bit_offset));
    bit_offset += bit_cut;
    if (bit_offset >= 8) {
      if (bit_offset > 8) {
        ret |= ((index & (buf[offset + 1] << 8)) >> 8);
      }
      bit_offset -= 8;
      offset++;
    }
    bit_num += bit_cut;
    return ret;
  }
  bit_num += bit_cut;
  return buf[offset++];
}
CGifLZWEncoder::CGifLZWEncoder() {
  FXSYS_memset(this, 0, sizeof(CGifLZWEncoder));
}
CGifLZWEncoder::~CGifLZWEncoder() {}
void CGifLZWEncoder::ClearTable() {
  index_bit_cur = code_size + 1;
  index_num = code_end + 1;
  table_cur = code_end + 1;
  for (uint16_t i = 0; i < GIF_MAX_LZW_CODE; i++) {
    code_table[i].prefix = 0;
    code_table[i].suffix = 0;
  }
}
void CGifLZWEncoder::Start(uint8_t code_len,
                           const uint8_t* src_buf,
                           uint8_t*& dst_buf,
                           uint32_t& offset) {
  code_size = code_len + 1;
  ASSERT(code_size < 32);
  src_bit_cut = code_size;
  if (code_len == 0) {
    src_bit_cut = 1;
    code_size = 2;
  }
  code_clear = 1 << code_size;
  code_end = code_clear + 1;
  dst_buf[offset++] = code_size;
  bit_offset = 0;
  ClearTable();
  src_offset = 0;
  src_bit_offset = 0;
  src_bit_num = 0;
  code_table[index_num].prefix = gif_cut_buf(src_buf, src_offset, src_bit_cut,
                                             src_bit_offset, src_bit_num);
  code_table[index_num].suffix = gif_cut_buf(src_buf, src_offset, src_bit_cut,
                                             src_bit_offset, src_bit_num);
}
void CGifLZWEncoder::WriteBlock(uint8_t*& dst_buf,
                                uint32_t& dst_len,
                                uint32_t& offset) {
  if (!gif_grow_buf(dst_buf, dst_len, offset + GIF_DATA_BLOCK + 1)) {
    longjmp(jmp, 1);
  }
  dst_buf[offset++] = index_buf_len;
  FXSYS_memcpy(&dst_buf[offset], index_buf, index_buf_len);
  offset += index_buf_len;
  FXSYS_memset(index_buf, 0, GIF_DATA_BLOCK);
  index_buf_len = 0;
}
void CGifLZWEncoder::EncodeString(uint32_t index,
                                  uint8_t*& dst_buf,
                                  uint32_t& dst_len,
                                  uint32_t& offset) {
  uint8_t index_bit_use;
  index_bit_use = 0;
  if (index_buf_len == GIF_DATA_BLOCK) {
    WriteBlock(dst_buf, dst_len, offset);
  }
  gif_cut_index(index_buf[index_buf_len], index, index_bit_cur, index_bit_use,
                bit_offset);
  if (index_bit_cur <= (8 - bit_offset)) {
    bit_offset += index_bit_cur;
  } else if (index_bit_cur <= (16 - bit_offset)) {
    index_bit_use += (8 - bit_offset);
    bit_offset = 0;
    index_buf_len++;
    if (index_buf_len == GIF_DATA_BLOCK) {
      WriteBlock(dst_buf, dst_len, offset);
    }
    gif_cut_index(index_buf[index_buf_len], index, index_bit_cur, index_bit_use,
                  bit_offset);
    bit_offset = index_bit_cur - index_bit_use;
  } else {
    index_bit_use += (8 - bit_offset);
    bit_offset = 0;
    index_buf_len++;
    if (index_buf_len == GIF_DATA_BLOCK) {
      WriteBlock(dst_buf, dst_len, offset);
    }
    gif_cut_index(index_buf[index_buf_len], index, index_bit_cur, index_bit_use,
                  bit_offset);
    index_bit_use += 8;
    bit_offset = 0;
    index_buf_len++;
    if (index_buf_len == GIF_DATA_BLOCK) {
      WriteBlock(dst_buf, dst_len, offset);
    }
    gif_cut_index(index_buf[index_buf_len], index, index_bit_cur, index_bit_use,
                  bit_offset);
    bit_offset = index_bit_cur - index_bit_use;
  }
  if (bit_offset == 8) {
    bit_offset = 0;
    index_buf_len++;
    if (index_buf_len == GIF_DATA_BLOCK) {
      WriteBlock(dst_buf, dst_len, offset);
    }
  }
  if (index == code_end) {
    index_buf_len++;
    WriteBlock(dst_buf, dst_len, offset);
  }
  if (index_num++ >> index_bit_cur) {
    index_bit_cur++;
  }
}
bool CGifLZWEncoder::Encode(const uint8_t* src_buf,
                            uint32_t src_len,
                            uint8_t*& dst_buf,
                            uint32_t& dst_len,
                            uint32_t& offset) {
  uint8_t suffix;
  if (setjmp(jmp)) {
    return false;
  }
  while (src_bit_num < src_len) {
    if (!LookUpInTable(src_buf, src_offset, src_bit_offset)) {
      EncodeString(code_table[index_num].prefix, dst_buf, dst_len, offset);
      if (index_num == GIF_MAX_LZW_CODE) {
        suffix = code_table[index_num - 1].suffix;
        EncodeString(code_clear, dst_buf, dst_len, offset);
        ClearTable();
        code_table[index_num].prefix = suffix;
        code_table[index_num].suffix = gif_cut_buf(
            src_buf, src_offset, src_bit_cut, src_bit_offset, src_bit_num);
      } else {
        code_table[index_num].prefix = code_table[index_num - 1].suffix;
        code_table[index_num].suffix = gif_cut_buf(
            src_buf, src_offset, src_bit_cut, src_bit_offset, src_bit_num);
      }
    }
  }
  src_offset = 0;
  src_bit_offset = 0;
  src_bit_num = 0;
  return true;
}
bool CGifLZWEncoder::LookUpInTable(const uint8_t* buf,
                                   uint32_t& offset,
                                   uint8_t& out_bit_offset) {
  for (uint16_t i = table_cur; i < index_num; i++) {
    if (code_table[i].prefix == code_table[index_num].prefix &&
        code_table[i].suffix == code_table[index_num].suffix) {
      code_table[index_num].prefix = i;
      code_table[index_num].suffix =
          gif_cut_buf(buf, offset, src_bit_cut, out_bit_offset, src_bit_num);
      table_cur = i;
      return true;
    }
  }
  table_cur = code_end + 1;
  return false;
}
void CGifLZWEncoder::Finish(uint8_t*& dst_buf,
                            uint32_t& dst_len,
                            uint32_t& offset) {
  EncodeString(code_table[index_num].prefix, dst_buf, dst_len, offset);
  EncodeString(code_end, dst_buf, dst_len, offset);
  bit_offset = 0;
  ClearTable();
}
gif_decompress_struct_p gif_create_decompress() {
  gif_decompress_struct_p gif_ptr = FX_Alloc(gif_decompress_struct, 1);
  FXSYS_memset(gif_ptr, 0, sizeof(gif_decompress_struct));
  gif_ptr->decode_status = GIF_D_STATUS_SIG;
  gif_ptr->img_ptr_arr_ptr = new std::vector<GifImage*>;
  gif_ptr->cmt_data_ptr = new CFX_ByteString;
  gif_ptr->pt_ptr_arr_ptr = new std::vector<GifPlainText*>;
  return gif_ptr;
}
void gif_destroy_decompress(gif_decompress_struct_pp gif_ptr_ptr) {
  if (!gif_ptr_ptr || !*gif_ptr_ptr)
    return;

  gif_decompress_struct_p gif_ptr = *gif_ptr_ptr;
  *gif_ptr_ptr = nullptr;
  FX_Free(gif_ptr->global_pal_ptr);
  delete gif_ptr->img_decoder_ptr;
  if (gif_ptr->img_ptr_arr_ptr) {
    size_t size_img_arr = gif_ptr->img_ptr_arr_ptr->size();
    for (size_t i = 0; i < size_img_arr; i++) {
      GifImage* p = (*gif_ptr->img_ptr_arr_ptr)[i];
      FX_Free(p->image_info_ptr);
      FX_Free(p->image_gce_ptr);
      FX_Free(p->image_row_buf);
      if (p->local_pal_ptr && p->local_pal_ptr != gif_ptr->global_pal_ptr) {
        FX_Free(p->local_pal_ptr);
      }
      FX_Free(p);
    }
    gif_ptr->img_ptr_arr_ptr->clear();
    delete gif_ptr->img_ptr_arr_ptr;
  }
  delete gif_ptr->cmt_data_ptr;
  FX_Free(gif_ptr->gce_ptr);
  if (gif_ptr->pt_ptr_arr_ptr) {
    size_t size_pt_arr = gif_ptr->pt_ptr_arr_ptr->size();
    for (size_t i = 0; i < size_pt_arr; i++) {
      GifPlainText* p = (*gif_ptr->pt_ptr_arr_ptr)[i];
      FX_Free(p->gce_ptr);
      FX_Free(p->pte_ptr);
      delete p->string_ptr;
      FX_Free(p);
    }
    gif_ptr->pt_ptr_arr_ptr->clear();
    delete gif_ptr->pt_ptr_arr_ptr;
  }
  FX_Free(gif_ptr);
}
gif_compress_struct_p gif_create_compress() {
  gif_compress_struct_p gif_ptr = FX_Alloc(gif_compress_struct, 1);
  FXSYS_memset(gif_ptr, 0, sizeof(gif_compress_struct));
  gif_ptr->img_encoder_ptr = new CGifLZWEncoder;
  gif_ptr->header_ptr = FX_Alloc(GifHeader, 1);
  if (!gif_ptr->header_ptr) {
    delete (gif_ptr->img_encoder_ptr);
    FX_Free(gif_ptr);
    return nullptr;
  }
  FXSYS_memcpy(gif_ptr->header_ptr->signature, GIF_SIGNATURE, 3);
  FXSYS_memcpy(gif_ptr->header_ptr->version, "89a", 3);
  gif_ptr->lsd_ptr = FX_Alloc(GifLSD, 1);
  if (!gif_ptr->lsd_ptr) {
    FX_Free(gif_ptr->header_ptr);
    delete (gif_ptr->img_encoder_ptr);
    FX_Free(gif_ptr);
    return nullptr;
  }
  FXSYS_memset(gif_ptr->lsd_ptr, 0, sizeof(GifLSD));
  gif_ptr->image_info_ptr = FX_Alloc(GifImageInfo, 1);
  if (!gif_ptr->image_info_ptr) {
    FX_Free(gif_ptr->lsd_ptr);
    FX_Free(gif_ptr->header_ptr);
    delete (gif_ptr->img_encoder_ptr);
    FX_Free(gif_ptr);
    return nullptr;
  }
  FXSYS_memset(gif_ptr->image_info_ptr, 0, sizeof(GifImageInfo));
  gif_ptr->gce_ptr = FX_Alloc(GifGCE, 1);
  if (!gif_ptr->gce_ptr) {
    FX_Free(gif_ptr->image_info_ptr);
    FX_Free(gif_ptr->lsd_ptr);
    FX_Free(gif_ptr->header_ptr);
    delete (gif_ptr->img_encoder_ptr);
    FX_Free(gif_ptr);
    return nullptr;
  }
  gif_ptr->pte_ptr = FX_Alloc(GifPTE, 1);
  if (!gif_ptr->pte_ptr) {
    FX_Free(gif_ptr->gce_ptr);
    FX_Free(gif_ptr->image_info_ptr);
    FX_Free(gif_ptr->lsd_ptr);
    FX_Free(gif_ptr->header_ptr);
    delete (gif_ptr->img_encoder_ptr);
    FX_Free(gif_ptr);
    return nullptr;
  }
  FXSYS_memset(gif_ptr->pte_ptr, 0, sizeof(GifPTE));
  gif_ptr->pte_ptr->block_size = 12;
  return gif_ptr;
}
void gif_destroy_compress(gif_compress_struct_pp gif_ptr_ptr) {
  if (!gif_ptr_ptr || !*gif_ptr_ptr)
    return;

  gif_compress_struct_p gif_ptr = *gif_ptr_ptr;
  *gif_ptr_ptr = nullptr;
  FX_Free(gif_ptr->header_ptr);
  FX_Free(gif_ptr->lsd_ptr);
  FX_Free(gif_ptr->global_pal);
  FX_Free(gif_ptr->image_info_ptr);
  FX_Free(gif_ptr->local_pal);
  delete gif_ptr->img_encoder_ptr;
  FX_Free(gif_ptr->gce_ptr);
  FX_Free(gif_ptr->cmt_data_ptr);
  FX_Free(gif_ptr->pte_ptr);
  FX_Free(gif_ptr);
}
void gif_error(gif_decompress_struct_p gif_ptr, const FX_CHAR* err_msg) {
  if (gif_ptr && gif_ptr->gif_error_fn) {
    gif_ptr->gif_error_fn(gif_ptr, err_msg);
  }
}
void gif_warn(gif_decompress_struct_p gif_ptr, const FX_CHAR* err_msg) {}
int32_t gif_read_header(gif_decompress_struct_p gif_ptr) {
  if (!gif_ptr)
    return 0;

  uint32_t skip_size_org = gif_ptr->skip_size;
  ASSERT(sizeof(GifHeader) == 6);
  GifHeader* gif_header_ptr = nullptr;
  if (!gif_read_data(gif_ptr, (uint8_t**)&gif_header_ptr, 6))
    return 2;

  if (FXSYS_strncmp(gif_header_ptr->signature, GIF_SIGNATURE, 3) != 0 ||
      gif_header_ptr->version[0] != '8' || gif_header_ptr->version[2] != 'a') {
    gif_error(gif_ptr, "Not A Gif Image");
    return 0;
  }
  ASSERT(sizeof(GifLSD) == 7);
  GifLSD* gif_lsd_ptr = nullptr;
  if (!gif_read_data(gif_ptr, (uint8_t**)&gif_lsd_ptr, 7)) {
    gif_ptr->skip_size = skip_size_org;
    return 2;
  }
  if (((GifGF*)&gif_lsd_ptr->global_flag)->global_pal) {
    gif_ptr->global_pal_num = 2
                              << ((GifGF*)&gif_lsd_ptr->global_flag)->pal_bits;
    ASSERT(sizeof(GifPalette) == 3);
    int32_t global_pal_size = gif_ptr->global_pal_num * 3;
    uint8_t* global_pal_ptr = nullptr;
    if (!gif_read_data(gif_ptr, &global_pal_ptr, global_pal_size)) {
      gif_ptr->skip_size = skip_size_org;
      return 2;
    }
    gif_ptr->global_sort_flag = ((GifGF*)&gif_lsd_ptr->global_flag)->sort_flag;
    gif_ptr->global_color_resolution =
        ((GifGF*)&gif_lsd_ptr->global_flag)->color_resolution;
    FX_Free(gif_ptr->global_pal_ptr);
    gif_ptr->global_pal_ptr = (GifPalette*)FX_Alloc(uint8_t, global_pal_size);
    FXSYS_memcpy(gif_ptr->global_pal_ptr, global_pal_ptr, global_pal_size);
  }
  gif_ptr->width = (int)GetWord_LSBFirst((uint8_t*)&gif_lsd_ptr->width);
  gif_ptr->height = (int)GetWord_LSBFirst((uint8_t*)&gif_lsd_ptr->height);
  gif_ptr->bc_index = gif_lsd_ptr->bc_index;
  gif_ptr->pixel_aspect = gif_lsd_ptr->pixel_aspect;
  return 1;
}
int32_t gif_get_frame(gif_decompress_struct_p gif_ptr) {
  if (!gif_ptr)
    return 0;

  int32_t ret = 1;
  while (true) {
    switch (gif_ptr->decode_status) {
      case GIF_D_STATUS_TAIL:
        return 1;
      case GIF_D_STATUS_SIG: {
        uint8_t* sig_ptr = nullptr;
        if (!gif_read_data(gif_ptr, &sig_ptr, 1))
          return 2;

        switch (*sig_ptr) {
          case GIF_SIG_EXTENSION:
            gif_save_decoding_status(gif_ptr, GIF_D_STATUS_EXT);
            continue;
          case GIF_SIG_IMAGE:
            gif_save_decoding_status(gif_ptr, GIF_D_STATUS_IMG_INFO);
            continue;
          case GIF_SIG_TRAILER:
            gif_save_decoding_status(gif_ptr, GIF_D_STATUS_TAIL);
            return 1;
          default:
            if (gif_ptr->avail_in) {
              gif_warn(gif_ptr, "The Gif File has non_standard Tag!");
              gif_save_decoding_status(gif_ptr, GIF_D_STATUS_SIG);
              continue;
            }
            gif_warn(gif_ptr, "The Gif File Doesn't have Trailer Tag!");
            return 1;
        }
      }
      case GIF_D_STATUS_EXT: {
        uint8_t* ext_ptr = nullptr;
        if (!gif_read_data(gif_ptr, &ext_ptr, 1))
          return 2;

        switch (*ext_ptr) {
          case GIF_BLOCK_CE:
            gif_save_decoding_status(gif_ptr, GIF_D_STATUS_EXT_CE);
            continue;
          case GIF_BLOCK_GCE:
            gif_save_decoding_status(gif_ptr, GIF_D_STATUS_EXT_GCE);
            continue;
          case GIF_BLOCK_PTE:
            gif_save_decoding_status(gif_ptr, GIF_D_STATUS_EXT_PTE);
            continue;
          default: {
            int32_t status = GIF_D_STATUS_EXT_UNE;
            if (*ext_ptr == GIF_BLOCK_PTE) {
              status = GIF_D_STATUS_EXT_PTE;
            }
            gif_save_decoding_status(gif_ptr, status);
            continue;
          }
        }
      }
      case GIF_D_STATUS_IMG_INFO: {
        ret = gif_decode_image_info(gif_ptr);
        if (ret != 1) {
          return ret;
        }
        continue;
      }
      case GIF_D_STATUS_IMG_DATA: {
        uint8_t* data_size_ptr = nullptr;
        uint8_t* data_ptr = nullptr;
        uint32_t skip_size_org = gif_ptr->skip_size;
        if (!gif_read_data(gif_ptr, &data_size_ptr, 1))
          return 2;

        while (*data_size_ptr != GIF_BLOCK_TERMINAL) {
          if (!gif_read_data(gif_ptr, &data_ptr, *data_size_ptr)) {
            gif_ptr->skip_size = skip_size_org;
            return 2;
          }
          gif_save_decoding_status(gif_ptr, GIF_D_STATUS_IMG_DATA);
          skip_size_org = gif_ptr->skip_size;
          if (!gif_read_data(gif_ptr, &data_size_ptr, 1))
            return 2;
        }
        gif_save_decoding_status(gif_ptr, GIF_D_STATUS_SIG);
        continue;
      }
      default: {
        ret = gif_decode_extension(gif_ptr);
        if (ret != 1) {
          return ret;
        }
        continue;
      }
    }
  }
  return 1;
}
void gif_takeover_gce_ptr(gif_decompress_struct_p gif_ptr,
                          GifGCE** gce_ptr_ptr) {
  *gce_ptr_ptr = nullptr;
  if (gif_ptr->gce_ptr && gce_ptr_ptr) {
    *gce_ptr_ptr = gif_ptr->gce_ptr;
    gif_ptr->gce_ptr = nullptr;
  }
}
int32_t gif_decode_extension(gif_decompress_struct_p gif_ptr) {
  uint8_t* data_size_ptr = nullptr;
  uint8_t* data_ptr = nullptr;
  uint32_t skip_size_org = gif_ptr->skip_size;
  switch (gif_ptr->decode_status) {
    case GIF_D_STATUS_EXT_CE: {
      if (!gif_read_data(gif_ptr, &data_size_ptr, 1)) {
        gif_ptr->skip_size = skip_size_org;
        return 2;
      }
      gif_ptr->cmt_data_ptr->clear();
      while (*data_size_ptr != GIF_BLOCK_TERMINAL) {
        uint8_t data_size = *data_size_ptr;
        if (!gif_read_data(gif_ptr, &data_ptr, *data_size_ptr) ||
            !gif_read_data(gif_ptr, &data_size_ptr, 1)) {
          gif_ptr->skip_size = skip_size_org;
          return 2;
        }
        *(gif_ptr->cmt_data_ptr) +=
            CFX_ByteString((const FX_CHAR*)data_ptr, data_size);
      }
    } break;
    case GIF_D_STATUS_EXT_PTE: {
      ASSERT(sizeof(GifPTE) == 13);
      GifPTE* gif_pte_ptr = nullptr;
      if (!gif_read_data(gif_ptr, (uint8_t**)&gif_pte_ptr, 13)) {
        return 2;
      }
      GifPlainText* gif_pt_ptr = FX_Alloc(GifPlainText, 1);
      FXSYS_memset(gif_pt_ptr, 0, sizeof(GifPlainText));
      gif_takeover_gce_ptr(gif_ptr, &gif_pt_ptr->gce_ptr);
      gif_pt_ptr->pte_ptr = FX_Alloc(GifPTE, 1);
      gif_pt_ptr->string_ptr = new CFX_ByteString;
      gif_pt_ptr->pte_ptr->block_size = gif_pte_ptr->block_size;
      gif_pt_ptr->pte_ptr->grid_left =
          GetWord_LSBFirst((uint8_t*)&gif_pte_ptr->grid_left);
      gif_pt_ptr->pte_ptr->grid_top =
          GetWord_LSBFirst((uint8_t*)&gif_pte_ptr->grid_top);
      gif_pt_ptr->pte_ptr->grid_width =
          GetWord_LSBFirst((uint8_t*)&gif_pte_ptr->grid_width);
      gif_pt_ptr->pte_ptr->grid_height =
          GetWord_LSBFirst((uint8_t*)&gif_pte_ptr->grid_height);
      gif_pt_ptr->pte_ptr->char_width = gif_pte_ptr->char_width;
      gif_pt_ptr->pte_ptr->char_height = gif_pte_ptr->char_height;
      gif_pt_ptr->pte_ptr->fc_index = gif_pte_ptr->fc_index;
      gif_pt_ptr->pte_ptr->bc_index = gif_pte_ptr->bc_index;
      if (!gif_read_data(gif_ptr, &data_size_ptr, 1)) {
        gif_ptr->skip_size = skip_size_org;
        if (gif_pt_ptr) {
          FX_Free(gif_pt_ptr->gce_ptr);
          FX_Free(gif_pt_ptr->pte_ptr);
          delete gif_pt_ptr->string_ptr;
          FX_Free(gif_pt_ptr);
        }
        return 2;
      }
      while (*data_size_ptr != GIF_BLOCK_TERMINAL) {
        uint8_t data_size = *data_size_ptr;
        if (!gif_read_data(gif_ptr, &data_ptr, *data_size_ptr) ||
            !gif_read_data(gif_ptr, &data_size_ptr, 1)) {
          gif_ptr->skip_size = skip_size_org;
          if (gif_pt_ptr) {
            FX_Free(gif_pt_ptr->gce_ptr);
            FX_Free(gif_pt_ptr->pte_ptr);
            delete gif_pt_ptr->string_ptr;
            FX_Free(gif_pt_ptr);
          }
          return 2;
        }
        *(gif_pt_ptr->string_ptr) +=
            CFX_ByteString((const FX_CHAR*)data_ptr, data_size);
      }
      gif_ptr->pt_ptr_arr_ptr->push_back(gif_pt_ptr);
    } break;
    case GIF_D_STATUS_EXT_GCE: {
      ASSERT(sizeof(GifGCE) == 5);
      GifGCE* gif_gce_ptr = nullptr;
      if (!gif_read_data(gif_ptr, (uint8_t**)&gif_gce_ptr, 6))
        return 2;

      if (!gif_ptr->gce_ptr)
        gif_ptr->gce_ptr = FX_Alloc(GifGCE, 1);
      gif_ptr->gce_ptr->block_size = gif_gce_ptr->block_size;
      gif_ptr->gce_ptr->gce_flag = gif_gce_ptr->gce_flag;
      gif_ptr->gce_ptr->delay_time =
          GetWord_LSBFirst((uint8_t*)&gif_gce_ptr->delay_time);
      gif_ptr->gce_ptr->trans_index = gif_gce_ptr->trans_index;
    } break;
    default: {
      if (gif_ptr->decode_status == GIF_D_STATUS_EXT_PTE) {
        FX_Free(gif_ptr->gce_ptr);
        gif_ptr->gce_ptr = nullptr;
      }
      if (!gif_read_data(gif_ptr, &data_size_ptr, 1))
        return 2;

      while (*data_size_ptr != GIF_BLOCK_TERMINAL) {
        if (!gif_read_data(gif_ptr, &data_ptr, *data_size_ptr) ||
            !gif_read_data(gif_ptr, &data_size_ptr, 1)) {
          gif_ptr->skip_size = skip_size_org;
          return 2;
        }
      }
    }
  }
  gif_save_decoding_status(gif_ptr, GIF_D_STATUS_SIG);
  return 1;
}
int32_t gif_decode_image_info(gif_decompress_struct_p gif_ptr) {
  if (gif_ptr->width == 0 || gif_ptr->height == 0) {
    gif_error(gif_ptr, "No Image Header Info");
    return 0;
  }
  uint32_t skip_size_org = gif_ptr->skip_size;
  ASSERT(sizeof(GifImageInfo) == 9);
  GifImageInfo* gif_img_info_ptr = nullptr;
  if (!gif_read_data(gif_ptr, (uint8_t**)&gif_img_info_ptr, 9))
    return 2;

  GifImage* gif_image_ptr = FX_Alloc(GifImage, 1);
  FXSYS_memset(gif_image_ptr, 0, sizeof(GifImage));
  gif_image_ptr->image_info_ptr = FX_Alloc(GifImageInfo, 1);
  gif_image_ptr->image_info_ptr->left =
      GetWord_LSBFirst((uint8_t*)&gif_img_info_ptr->left);
  gif_image_ptr->image_info_ptr->top =
      GetWord_LSBFirst((uint8_t*)&gif_img_info_ptr->top);
  gif_image_ptr->image_info_ptr->width =
      GetWord_LSBFirst((uint8_t*)&gif_img_info_ptr->width);
  gif_image_ptr->image_info_ptr->height =
      GetWord_LSBFirst((uint8_t*)&gif_img_info_ptr->height);
  gif_image_ptr->image_info_ptr->local_flag = gif_img_info_ptr->local_flag;
  if (gif_image_ptr->image_info_ptr->left +
              gif_image_ptr->image_info_ptr->width >
          gif_ptr->width ||
      gif_image_ptr->image_info_ptr->top +
              gif_image_ptr->image_info_ptr->height >
          gif_ptr->height) {
    FX_Free(gif_image_ptr->image_info_ptr);
    FX_Free(gif_image_ptr->image_row_buf);
    FX_Free(gif_image_ptr);
    gif_error(gif_ptr, "Image Data Out Of LSD, The File May Be Corrupt");
    return 0;
  }
  GifLF* gif_img_info_lf_ptr = (GifLF*)&gif_img_info_ptr->local_flag;
  if (gif_img_info_lf_ptr->local_pal) {
    ASSERT(sizeof(GifPalette) == 3);
    int32_t loc_pal_size = (2 << gif_img_info_lf_ptr->pal_bits) * 3;
    uint8_t* loc_pal_ptr = nullptr;
    if (!gif_read_data(gif_ptr, &loc_pal_ptr, loc_pal_size)) {
      gif_ptr->skip_size = skip_size_org;
      FX_Free(gif_image_ptr->image_info_ptr);
      FX_Free(gif_image_ptr->image_row_buf);
      FX_Free(gif_image_ptr);
      return 2;
    }
    gif_image_ptr->local_pal_ptr =
        (GifPalette*)gif_ptr->gif_ask_buf_for_pal_fn(gif_ptr, loc_pal_size);
    if (gif_image_ptr->local_pal_ptr) {
      FXSYS_memcpy((uint8_t*)gif_image_ptr->local_pal_ptr, loc_pal_ptr,
                   loc_pal_size);
    }
  }
  uint8_t* code_size_ptr = nullptr;
  if (!gif_read_data(gif_ptr, &code_size_ptr, 1)) {
    gif_ptr->skip_size = skip_size_org;
    FX_Free(gif_image_ptr->image_info_ptr);
    FX_Free(gif_image_ptr->local_pal_ptr);
    FX_Free(gif_image_ptr->image_row_buf);
    FX_Free(gif_image_ptr);
    return 2;
  }
  gif_image_ptr->image_code_size = *code_size_ptr;
  gif_ptr->gif_record_current_position_fn(gif_ptr,
                                          &gif_image_ptr->image_data_pos);
  gif_image_ptr->image_data_pos += gif_ptr->skip_size;
  gif_takeover_gce_ptr(gif_ptr, &gif_image_ptr->image_gce_ptr);
  gif_ptr->img_ptr_arr_ptr->push_back(gif_image_ptr);
  gif_save_decoding_status(gif_ptr, GIF_D_STATUS_IMG_DATA);
  return 1;
}
int32_t gif_load_frame(gif_decompress_struct_p gif_ptr, int32_t frame_num) {
  if (!gif_ptr || frame_num < 0 ||
      frame_num >= pdfium::CollectionSize<int>(*gif_ptr->img_ptr_arr_ptr)) {
    return 0;
  }
  uint8_t* data_size_ptr = nullptr;
  uint8_t* data_ptr = nullptr;
  uint32_t skip_size_org = gif_ptr->skip_size;
  GifImage* gif_image_ptr = (*gif_ptr->img_ptr_arr_ptr)[frame_num];
  uint32_t gif_img_row_bytes = gif_image_ptr->image_info_ptr->width;
  if (gif_img_row_bytes == 0) {
    gif_error(gif_ptr, "Error Invalid Number of Row Bytes");
    return 0;
  }
  if (gif_ptr->decode_status == GIF_D_STATUS_TAIL) {
    if (gif_image_ptr->image_row_buf) {
      FX_Free(gif_image_ptr->image_row_buf);
      gif_image_ptr->image_row_buf = nullptr;
    }
    gif_image_ptr->image_row_buf = FX_Alloc(uint8_t, gif_img_row_bytes);
    GifGCE* gif_img_gce_ptr = gif_image_ptr->image_gce_ptr;
    int32_t loc_pal_num =
        ((GifLF*)&gif_image_ptr->image_info_ptr->local_flag)->local_pal
            ? (2 << ((GifLF*)&gif_image_ptr->image_info_ptr->local_flag)
                        ->pal_bits)
            : 0;
    gif_ptr->avail_in = 0;
    if (!gif_img_gce_ptr) {
      bool bRes = gif_ptr->gif_get_record_position_fn(
          gif_ptr, gif_image_ptr->image_data_pos,
          gif_image_ptr->image_info_ptr->left,
          gif_image_ptr->image_info_ptr->top,
          gif_image_ptr->image_info_ptr->width,
          gif_image_ptr->image_info_ptr->height, loc_pal_num,
          gif_image_ptr->local_pal_ptr, 0, 0, -1, 0,
          (bool)((GifLF*)&gif_image_ptr->image_info_ptr->local_flag)
              ->interlace);
      if (!bRes) {
        FX_Free(gif_image_ptr->image_row_buf);
        gif_image_ptr->image_row_buf = nullptr;
        gif_error(gif_ptr, "Error Read Record Position Data");
        return 0;
      }
    } else {
      bool bRes = gif_ptr->gif_get_record_position_fn(
          gif_ptr, gif_image_ptr->image_data_pos,
          gif_image_ptr->image_info_ptr->left,
          gif_image_ptr->image_info_ptr->top,
          gif_image_ptr->image_info_ptr->width,
          gif_image_ptr->image_info_ptr->height, loc_pal_num,
          gif_image_ptr->local_pal_ptr,
          (int32_t)gif_image_ptr->image_gce_ptr->delay_time,
          (bool)((GifCEF*)&gif_image_ptr->image_gce_ptr->gce_flag)->user_input,
          ((GifCEF*)&gif_image_ptr->image_gce_ptr->gce_flag)->transparency
              ? (int32_t)gif_image_ptr->image_gce_ptr->trans_index
              : -1,
          (int32_t)((GifCEF*)&gif_image_ptr->image_gce_ptr->gce_flag)
              ->disposal_method,
          (bool)((GifLF*)&gif_image_ptr->image_info_ptr->local_flag)
              ->interlace);
      if (!bRes) {
        FX_Free(gif_image_ptr->image_row_buf);
        gif_image_ptr->image_row_buf = nullptr;
        gif_error(gif_ptr, "Error Read Record Position Data");
        return 0;
      }
    }
    if (gif_image_ptr->image_code_size >= 32) {
      FX_Free(gif_image_ptr->image_row_buf);
      gif_image_ptr->image_row_buf = nullptr;
      gif_error(gif_ptr, "Error Invalid Code Size");
      return 0;
    }
    if (!gif_ptr->img_decoder_ptr)
      gif_ptr->img_decoder_ptr = new CGifLZWDecoder(gif_ptr->err_ptr);
    gif_ptr->img_decoder_ptr->InitTable(gif_image_ptr->image_code_size);
    gif_ptr->img_row_offset = 0;
    gif_ptr->img_row_avail_size = 0;
    gif_ptr->img_pass_num = 0;
    gif_image_ptr->image_row_num = 0;
    gif_save_decoding_status(gif_ptr, GIF_D_STATUS_IMG_DATA);
  }
  CGifLZWDecoder* img_decoder_ptr = gif_ptr->img_decoder_ptr;
  if (gif_ptr->decode_status == GIF_D_STATUS_IMG_DATA) {
    if (!gif_read_data(gif_ptr, &data_size_ptr, 1))
      return 2;

    if (*data_size_ptr != GIF_BLOCK_TERMINAL) {
      if (!gif_read_data(gif_ptr, &data_ptr, *data_size_ptr)) {
        gif_ptr->skip_size = skip_size_org;
        return 2;
      }
      img_decoder_ptr->Input(data_ptr, *data_size_ptr);
      gif_save_decoding_status(gif_ptr, GIF_D_STATUS_IMG_DATA);
      gif_ptr->img_row_offset += gif_ptr->img_row_avail_size;
      gif_ptr->img_row_avail_size = gif_img_row_bytes - gif_ptr->img_row_offset;
      int32_t ret = img_decoder_ptr->Decode(
          gif_image_ptr->image_row_buf + gif_ptr->img_row_offset,
          gif_ptr->img_row_avail_size);
      if (ret == 0) {
        gif_decoding_failure_at_tail_cleanup(gif_ptr, gif_image_ptr);
        return 0;
      }
      while (ret != 0) {
        if (ret == 1) {
          gif_ptr->gif_get_row_fn(gif_ptr, gif_image_ptr->image_row_num,
                                  gif_image_ptr->image_row_buf);
          FX_Free(gif_image_ptr->image_row_buf);
          gif_image_ptr->image_row_buf = nullptr;
          gif_save_decoding_status(gif_ptr, GIF_D_STATUS_TAIL);
          return 1;
        }
        if (ret == 2) {
          ASSERT(img_decoder_ptr->GetAvailInput() == 0);
          skip_size_org = gif_ptr->skip_size;
          if (!gif_read_data(gif_ptr, &data_size_ptr, 1))
            return 2;

          if (*data_size_ptr != GIF_BLOCK_TERMINAL) {
            if (!gif_read_data(gif_ptr, &data_ptr, *data_size_ptr)) {
              gif_ptr->skip_size = skip_size_org;
              return 2;
            }
            img_decoder_ptr->Input(data_ptr, *data_size_ptr);
            gif_save_decoding_status(gif_ptr, GIF_D_STATUS_IMG_DATA);
            gif_ptr->img_row_offset += gif_ptr->img_row_avail_size;
            gif_ptr->img_row_avail_size =
                gif_img_row_bytes - gif_ptr->img_row_offset;
            ret = img_decoder_ptr->Decode(
                gif_image_ptr->image_row_buf + gif_ptr->img_row_offset,
                gif_ptr->img_row_avail_size);
          }
        }
        if (ret == 3) {
          if (((GifLF*)&gif_image_ptr->image_info_ptr->local_flag)->interlace) {
            gif_ptr->gif_get_row_fn(gif_ptr, gif_image_ptr->image_row_num,
                                    gif_image_ptr->image_row_buf);
            gif_image_ptr->image_row_num +=
                s_gif_interlace_step[gif_ptr->img_pass_num];
            if (gif_image_ptr->image_row_num >=
                (int32_t)gif_image_ptr->image_info_ptr->height) {
              gif_ptr->img_pass_num++;
              if (gif_ptr->img_pass_num == FX_ArraySize(s_gif_interlace_step)) {
                gif_decoding_failure_at_tail_cleanup(gif_ptr, gif_image_ptr);
                return 0;
              }
              gif_image_ptr->image_row_num =
                  s_gif_interlace_step[gif_ptr->img_pass_num] / 2;
            }
          } else {
            gif_ptr->gif_get_row_fn(gif_ptr, gif_image_ptr->image_row_num++,
                                    gif_image_ptr->image_row_buf);
          }
          gif_ptr->img_row_offset = 0;
          gif_ptr->img_row_avail_size = gif_img_row_bytes;
          ret = img_decoder_ptr->Decode(
              gif_image_ptr->image_row_buf + gif_ptr->img_row_offset,
              gif_ptr->img_row_avail_size);
        }
        if (ret == 0) {
          gif_decoding_failure_at_tail_cleanup(gif_ptr, gif_image_ptr);
          return 0;
        }
      }
    }
    gif_save_decoding_status(gif_ptr, GIF_D_STATUS_TAIL);
  }
  gif_error(gif_ptr, "Decode Image Data Error");
  return 0;
}
void gif_decoding_failure_at_tail_cleanup(gif_decompress_struct_p gif_ptr,
                                          GifImage* gif_image_ptr) {
  FX_Free(gif_image_ptr->image_row_buf);
  gif_image_ptr->image_row_buf = nullptr;
  gif_save_decoding_status(gif_ptr, GIF_D_STATUS_TAIL);
  gif_error(gif_ptr, "Decode Image Data Error");
}
void gif_save_decoding_status(gif_decompress_struct_p gif_ptr, int32_t status) {
  gif_ptr->decode_status = status;
  gif_ptr->next_in += gif_ptr->skip_size;
  gif_ptr->avail_in -= gif_ptr->skip_size;
  gif_ptr->skip_size = 0;
}
uint8_t* gif_read_data(gif_decompress_struct_p gif_ptr,
                       uint8_t** des_buf_pp,
                       uint32_t data_size) {
  if (!gif_ptr || gif_ptr->avail_in < gif_ptr->skip_size + data_size)
    return nullptr;

  *des_buf_pp = gif_ptr->next_in + gif_ptr->skip_size;
  gif_ptr->skip_size += data_size;
  return *des_buf_pp;
}
void gif_input_buffer(gif_decompress_struct_p gif_ptr,
                      uint8_t* src_buf,
                      uint32_t src_size) {
  gif_ptr->next_in = src_buf;
  gif_ptr->avail_in = src_size;
  gif_ptr->skip_size = 0;
}
uint32_t gif_get_avail_input(gif_decompress_struct_p gif_ptr,
                             uint8_t** avail_buf_ptr) {
  if (avail_buf_ptr) {
    *avail_buf_ptr = nullptr;
    if (gif_ptr->avail_in > 0) {
      *avail_buf_ptr = gif_ptr->next_in;
    }
  }
  return gif_ptr->avail_in;
}
int32_t gif_get_frame_num(gif_decompress_struct_p gif_ptr) {
  return pdfium::CollectionSize<int32_t>(*gif_ptr->img_ptr_arr_ptr);
}
static bool gif_write_header(gif_compress_struct_p gif_ptr,
                             uint8_t*& dst_buf,
                             uint32_t& dst_len) {
  if (gif_ptr->cur_offset) {
    return true;
  }
  dst_len = sizeof(GifHeader) + sizeof(GifLSD) + sizeof(GifGF);
  dst_buf = FX_TryAlloc(uint8_t, dst_len);
  if (!dst_buf)
    return false;

  FXSYS_memset(dst_buf, 0, dst_len);
  FXSYS_memcpy(dst_buf, gif_ptr->header_ptr, sizeof(GifHeader));
  gif_ptr->cur_offset += sizeof(GifHeader);
  SetWord_LSBFirst(dst_buf + gif_ptr->cur_offset, gif_ptr->lsd_ptr->width);
  gif_ptr->cur_offset += 2;
  SetWord_LSBFirst(dst_buf + gif_ptr->cur_offset, gif_ptr->lsd_ptr->height);
  gif_ptr->cur_offset += 2;
  dst_buf[gif_ptr->cur_offset++] = gif_ptr->lsd_ptr->global_flag;
  dst_buf[gif_ptr->cur_offset++] = gif_ptr->lsd_ptr->bc_index;
  dst_buf[gif_ptr->cur_offset++] = gif_ptr->lsd_ptr->pixel_aspect;
  if (gif_ptr->global_pal) {
    uint16_t size = sizeof(GifPalette) * gif_ptr->gpal_num;
    if (!gif_grow_buf(dst_buf, dst_len, gif_ptr->cur_offset + size)) {
      return false;
    }
    FXSYS_memcpy(&dst_buf[gif_ptr->cur_offset], gif_ptr->global_pal, size);
    gif_ptr->cur_offset += size;
  }
  return true;
}
void interlace_buf(const uint8_t* buf, uint32_t pitch, uint32_t height) {
  std::vector<uint8_t*> pass[4];
  uint32_t row = 0;
  uint8_t* temp;
  while (row < height) {
    size_t j;
    if (row % 8 == 0) {
      j = 0;
    } else if (row % 4 == 0) {
      j = 1;
    } else if (row % 2 == 0) {
      j = 2;
    } else {
      j = 3;
    }
    temp = FX_Alloc(uint8_t, pitch);
    FXSYS_memcpy(temp, &buf[pitch * row], pitch);
    pass[j].push_back(temp);
    row++;
  }
  for (size_t i = 0, row = 0; i < 4; i++) {
    for (size_t j = 0; j < pass[i].size(); j++, row++) {
      FXSYS_memcpy((uint8_t*)&buf[pitch * row], pass[i][j], pitch);
      FX_Free(pass[i][j]);
    }
  }
}
static void gif_write_block_data(const uint8_t* src_buf,
                                 uint32_t src_len,
                                 uint8_t*& dst_buf,
                                 uint32_t& dst_len,
                                 uint32_t& dst_offset) {
  uint32_t src_offset = 0;
  while (src_len > GIF_DATA_BLOCK) {
    dst_buf[dst_offset++] = GIF_DATA_BLOCK;
    FXSYS_memcpy(&dst_buf[dst_offset], &src_buf[src_offset], GIF_DATA_BLOCK);
    dst_offset += GIF_DATA_BLOCK;
    src_offset += GIF_DATA_BLOCK;
    src_len -= GIF_DATA_BLOCK;
  }
  dst_buf[dst_offset++] = (uint8_t)src_len;
  FXSYS_memcpy(&dst_buf[dst_offset], &src_buf[src_offset], src_len);
  dst_offset += src_len;
}
static bool gif_write_data(gif_compress_struct_p gif_ptr,
                           uint8_t*& dst_buf,
                           uint32_t& dst_len) {
  if (!gif_grow_buf(dst_buf, dst_len, gif_ptr->cur_offset + GIF_DATA_BLOCK)) {
    return false;
  }
  if (FXSYS_memcmp(gif_ptr->header_ptr->version, "89a", 3) == 0) {
    dst_buf[gif_ptr->cur_offset++] = GIF_SIG_EXTENSION;
    dst_buf[gif_ptr->cur_offset++] = GIF_BLOCK_GCE;
    gif_ptr->gce_ptr->block_size = 4;
    dst_buf[gif_ptr->cur_offset++] = gif_ptr->gce_ptr->block_size;
    gif_ptr->gce_ptr->gce_flag = 0;
    dst_buf[gif_ptr->cur_offset++] = gif_ptr->gce_ptr->gce_flag;
    gif_ptr->gce_ptr->delay_time = 10;
    SetWord_LSBFirst(dst_buf + gif_ptr->cur_offset,
                     gif_ptr->gce_ptr->delay_time);
    gif_ptr->cur_offset += 2;
    gif_ptr->gce_ptr->trans_index = 0;
    dst_buf[gif_ptr->cur_offset++] = gif_ptr->gce_ptr->trans_index;
    dst_buf[gif_ptr->cur_offset++] = 0;
  }
  dst_buf[gif_ptr->cur_offset++] = GIF_SIG_IMAGE;
  SetWord_LSBFirst(dst_buf + gif_ptr->cur_offset,
                   gif_ptr->image_info_ptr->left);
  gif_ptr->cur_offset += 2;
  SetWord_LSBFirst(dst_buf + gif_ptr->cur_offset, gif_ptr->image_info_ptr->top);
  gif_ptr->cur_offset += 2;
  SetWord_LSBFirst(dst_buf + gif_ptr->cur_offset,
                   gif_ptr->image_info_ptr->width);
  gif_ptr->cur_offset += 2;
  SetWord_LSBFirst(dst_buf + gif_ptr->cur_offset,
                   gif_ptr->image_info_ptr->height);
  gif_ptr->cur_offset += 2;
  GifLF& lf = (GifLF&)gif_ptr->image_info_ptr->local_flag;
  dst_buf[gif_ptr->cur_offset++] = gif_ptr->image_info_ptr->local_flag;
  if (gif_ptr->local_pal) {
    uint32_t pal_size = sizeof(GifPalette) * gif_ptr->lpal_num;
    if (!gif_grow_buf(dst_buf, dst_len, pal_size + gif_ptr->cur_offset)) {
      return false;
    }
    FXSYS_memcpy(&dst_buf[gif_ptr->cur_offset], gif_ptr->local_pal, pal_size);
    gif_ptr->cur_offset += pal_size;
  }
  if (lf.interlace) {
    interlace_buf(gif_ptr->src_buf, gif_ptr->src_pitch,
                  gif_ptr->image_info_ptr->height);
  }
  uint8_t code_bit = lf.pal_bits;
  if (lf.local_pal == 0) {
    GifGF& gf = (GifGF&)gif_ptr->lsd_ptr->global_flag;
    code_bit = gf.pal_bits;
  }
  if (code_bit >= 31)
    return false;
  gif_ptr->img_encoder_ptr->Start(code_bit, gif_ptr->src_buf, dst_buf,
                                  gif_ptr->cur_offset);
  uint32_t i;
  for (i = 0; i < gif_ptr->src_row; i++) {
    if (!gif_ptr->img_encoder_ptr->Encode(
            &gif_ptr->src_buf[i * gif_ptr->src_pitch],
            gif_ptr->src_width * (code_bit + 1), dst_buf, dst_len,
            gif_ptr->cur_offset)) {
      return false;
    }
  }
  gif_ptr->img_encoder_ptr->Finish(dst_buf, dst_len, gif_ptr->cur_offset);
  dst_buf[gif_ptr->cur_offset++] = 0;
  if (FXSYS_memcmp(gif_ptr->header_ptr->version, "89a", 3) == 0 &&
      gif_ptr->cmt_data_ptr) {
    dst_buf[gif_ptr->cur_offset++] = GIF_SIG_EXTENSION;
    dst_buf[gif_ptr->cur_offset++] = GIF_BLOCK_CE;
    gif_write_block_data(gif_ptr->cmt_data_ptr, gif_ptr->cmt_data_len, dst_buf,
                         dst_len, gif_ptr->cur_offset);
    dst_buf[gif_ptr->cur_offset++] = 0;
  }
  if (FXSYS_memcmp(gif_ptr->header_ptr->version, "89a", 3) == 0 &&
      gif_ptr->pte_data_ptr) {
    dst_buf[gif_ptr->cur_offset++] = GIF_SIG_EXTENSION;
    dst_buf[gif_ptr->cur_offset++] = GIF_BLOCK_PTE;
    dst_buf[gif_ptr->cur_offset++] = gif_ptr->pte_ptr->block_size;
    SetWord_LSBFirst(dst_buf + gif_ptr->cur_offset,
                     gif_ptr->pte_ptr->grid_left);
    gif_ptr->cur_offset += 2;
    SetWord_LSBFirst(dst_buf + gif_ptr->cur_offset, gif_ptr->pte_ptr->grid_top);
    gif_ptr->cur_offset += 2;
    SetWord_LSBFirst(dst_buf + gif_ptr->cur_offset,
                     gif_ptr->pte_ptr->grid_width);
    gif_ptr->cur_offset += 2;
    SetWord_LSBFirst(dst_buf + gif_ptr->cur_offset,
                     gif_ptr->pte_ptr->grid_height);
    gif_ptr->cur_offset += 2;
    SetWord_LSBFirst(dst_buf + gif_ptr->cur_offset,
                     gif_ptr->pte_ptr->char_width);
    gif_ptr->cur_offset += 2;
    SetWord_LSBFirst(dst_buf + gif_ptr->cur_offset,
                     gif_ptr->pte_ptr->char_height);
    gif_ptr->cur_offset += 2;
    SetWord_LSBFirst(dst_buf + gif_ptr->cur_offset, gif_ptr->pte_ptr->fc_index);
    gif_ptr->cur_offset += 2;
    SetWord_LSBFirst(dst_buf + gif_ptr->cur_offset, gif_ptr->pte_ptr->bc_index);
    gif_ptr->cur_offset += 2;
    gif_write_block_data(gif_ptr->pte_data_ptr, gif_ptr->pte_data_len, dst_buf,
                         dst_len, gif_ptr->cur_offset);
    gif_ptr->cur_offset += gif_ptr->pte_data_len;
    dst_buf[gif_ptr->cur_offset++] = 0;
  }
  dst_buf[gif_ptr->cur_offset++] = GIF_SIG_TRAILER;
  return true;
}
bool gif_encode(gif_compress_struct_p gif_ptr,
                uint8_t*& dst_buf,
                uint32_t& dst_len) {
  if (!gif_write_header(gif_ptr, dst_buf, dst_len)) {
    return false;
  }
  uint32_t cur_offset = gif_ptr->cur_offset;
  bool res = true;
  if (gif_ptr->frames) {
    gif_ptr->cur_offset--;
  }
  if (!gif_write_data(gif_ptr, dst_buf, dst_len)) {
    gif_ptr->cur_offset = cur_offset;
    res = false;
  }
  dst_len = gif_ptr->cur_offset;
  dst_buf[dst_len - 1] = GIF_SIG_TRAILER;
  if (res) {
    gif_ptr->frames++;
  }
  return res;
}

// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fxcodec/lbmp/fx_bmp.h"

#include <algorithm>

namespace {

const size_t kBmpCoreHeaderSize = 12;
const size_t kBmpInfoHeaderSize = 40;

// TODO(thestig): Replace with FXDWORD_GET_LSBFIRST?
uint32_t GetDWord_LSBFirst(uint8_t* p) {
  return p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24);
}

void SetDWord_LSBFirst(uint8_t* p, uint32_t v) {
  p[0] = (uint8_t)v;
  p[1] = (uint8_t)(v >> 8);
  p[2] = (uint8_t)(v >> 16);
  p[3] = (uint8_t)(v >> 24);
}
}  // namespace

uint16_t GetWord_LSBFirst(uint8_t* p) {
  return p[0] | (p[1] << 8);
}
void SetWord_LSBFirst(uint8_t* p, uint16_t v) {
  p[0] = (uint8_t)v;
  p[1] = (uint8_t)(v >> 8);
}
void bmp_error(bmp_decompress_struct_p bmp_ptr, const FX_CHAR* err_msg) {
  if (bmp_ptr && bmp_ptr->bmp_error_fn) {
    bmp_ptr->bmp_error_fn(bmp_ptr, err_msg);
  }
}
bmp_decompress_struct_p bmp_create_decompress() {
  bmp_decompress_struct_p bmp_ptr = FX_Alloc(bmp_decompress_struct, 1);
  FXSYS_memset(bmp_ptr, 0, sizeof(bmp_decompress_struct));
  bmp_ptr->decode_status = BMP_D_STATUS_HEADER;
  bmp_ptr->bmp_header_ptr = FX_Alloc(BmpFileHeader, 1);
  return bmp_ptr;
}
void bmp_destroy_decompress(bmp_decompress_struct_pp bmp_ptr_ptr) {
  if (!bmp_ptr_ptr || !*bmp_ptr_ptr)
    return;

  bmp_decompress_struct_p bmp_ptr = *bmp_ptr_ptr;
  *bmp_ptr_ptr = nullptr;
  if (bmp_ptr->out_row_buffer) {
    FX_Free(bmp_ptr->out_row_buffer);
  }
  FX_Free(bmp_ptr->pal_ptr);
  FX_Free(bmp_ptr->bmp_header_ptr);
  FX_Free(bmp_ptr);
}
int32_t bmp_read_header(bmp_decompress_struct_p bmp_ptr) {
  if (!bmp_ptr)
    return 0;

  uint32_t skip_size_org = bmp_ptr->skip_size;
  if (bmp_ptr->decode_status == BMP_D_STATUS_HEADER) {
    ASSERT(sizeof(BmpFileHeader) == 14);
    BmpFileHeader* bmp_header_ptr = nullptr;
    if (!bmp_read_data(bmp_ptr, (uint8_t**)&bmp_header_ptr, 14))
      return 2;

    bmp_ptr->bmp_header_ptr->bfType =
        GetWord_LSBFirst((uint8_t*)&bmp_header_ptr->bfType);
    bmp_ptr->bmp_header_ptr->bfOffBits =
        GetDWord_LSBFirst((uint8_t*)&bmp_header_ptr->bfOffBits);
    bmp_ptr->data_size = GetDWord_LSBFirst((uint8_t*)&bmp_header_ptr->bfSize);
    if (bmp_ptr->bmp_header_ptr->bfType != BMP_SIGNATURE) {
      bmp_error(bmp_ptr, "Not A Bmp Image");
      return 0;
    }
    if (bmp_ptr->avail_in < sizeof(uint32_t)) {
      bmp_ptr->skip_size = skip_size_org;
      return 2;
    }
    bmp_ptr->img_ifh_size =
        GetDWord_LSBFirst(bmp_ptr->next_in + bmp_ptr->skip_size);
    bmp_ptr->pal_type = 0;
    static_assert(sizeof(BmpCoreHeader) == kBmpCoreHeaderSize,
                  "BmpCoreHeader has wrong size");
    static_assert(sizeof(BmpInfoHeader) == kBmpInfoHeaderSize,
                  "BmpInfoHeader has wrong size");
    switch (bmp_ptr->img_ifh_size) {
      case kBmpCoreHeaderSize: {
        bmp_ptr->pal_type = 1;
        BmpCoreHeaderPtr bmp_core_header_ptr = nullptr;
        if (!bmp_read_data(bmp_ptr, (uint8_t**)&bmp_core_header_ptr,
                           bmp_ptr->img_ifh_size)) {
          bmp_ptr->skip_size = skip_size_org;
          return 2;
        }
        bmp_ptr->width =
            GetWord_LSBFirst((uint8_t*)&bmp_core_header_ptr->bcWidth);
        bmp_ptr->height =
            GetWord_LSBFirst((uint8_t*)&bmp_core_header_ptr->bcHeight);
        bmp_ptr->bitCounts =
            GetWord_LSBFirst((uint8_t*)&bmp_core_header_ptr->bcBitCount);
        bmp_ptr->compress_flag = BMP_RGB;
        bmp_ptr->imgTB_flag = false;
      } break;
      case kBmpInfoHeaderSize: {
        BmpInfoHeaderPtr bmp_info_header_ptr = nullptr;
        if (!bmp_read_data(bmp_ptr, (uint8_t**)&bmp_info_header_ptr,
                           bmp_ptr->img_ifh_size)) {
          bmp_ptr->skip_size = skip_size_org;
          return 2;
        }
        bmp_ptr->width =
            GetDWord_LSBFirst((uint8_t*)&bmp_info_header_ptr->biWidth);
        bmp_ptr->height =
            GetDWord_LSBFirst((uint8_t*)&bmp_info_header_ptr->biHeight);
        bmp_ptr->bitCounts =
            GetWord_LSBFirst((uint8_t*)&bmp_info_header_ptr->biBitCount);
        bmp_ptr->compress_flag =
            GetDWord_LSBFirst((uint8_t*)&bmp_info_header_ptr->biCompression);
        bmp_ptr->color_used =
            GetDWord_LSBFirst((uint8_t*)&bmp_info_header_ptr->biClrUsed);
        bmp_ptr->dpi_x = (int32_t)GetDWord_LSBFirst(
            (uint8_t*)&bmp_info_header_ptr->biXPelsPerMeter);
        bmp_ptr->dpi_y = (int32_t)GetDWord_LSBFirst(
            (uint8_t*)&bmp_info_header_ptr->biYPelsPerMeter);
        if (bmp_ptr->height < 0) {
          bmp_ptr->height = -bmp_ptr->height;
          bmp_ptr->imgTB_flag = true;
        }
      } break;
      default: {
        if (bmp_ptr->img_ifh_size >
            std::min(kBmpInfoHeaderSize, sizeof(BmpInfoHeader))) {
          BmpInfoHeaderPtr bmp_info_header_ptr = nullptr;
          if (!bmp_read_data(bmp_ptr, (uint8_t**)&bmp_info_header_ptr,
                             bmp_ptr->img_ifh_size)) {
            bmp_ptr->skip_size = skip_size_org;
            return 2;
          }
          uint16_t biPlanes;
          bmp_ptr->width =
              GetDWord_LSBFirst((uint8_t*)&bmp_info_header_ptr->biWidth);
          bmp_ptr->height =
              GetDWord_LSBFirst((uint8_t*)&bmp_info_header_ptr->biHeight);
          bmp_ptr->bitCounts =
              GetWord_LSBFirst((uint8_t*)&bmp_info_header_ptr->biBitCount);
          bmp_ptr->compress_flag =
              GetDWord_LSBFirst((uint8_t*)&bmp_info_header_ptr->biCompression);
          bmp_ptr->color_used =
              GetDWord_LSBFirst((uint8_t*)&bmp_info_header_ptr->biClrUsed);
          biPlanes = GetWord_LSBFirst((uint8_t*)&bmp_info_header_ptr->biPlanes);
          bmp_ptr->dpi_x = GetDWord_LSBFirst(
              (uint8_t*)&bmp_info_header_ptr->biXPelsPerMeter);
          bmp_ptr->dpi_y = GetDWord_LSBFirst(
              (uint8_t*)&bmp_info_header_ptr->biYPelsPerMeter);
          if (bmp_ptr->height < 0) {
            bmp_ptr->height = -bmp_ptr->height;
            bmp_ptr->imgTB_flag = true;
          }
          if (bmp_ptr->compress_flag == BMP_RGB && biPlanes == 1 &&
              bmp_ptr->color_used == 0) {
            break;
          }
        }
        bmp_error(bmp_ptr, "Unsupported Bmp File");
        return 0;
      }
    }
    if (bmp_ptr->width <= 0 || bmp_ptr->compress_flag > BMP_BITFIELDS) {
      bmp_error(bmp_ptr, "The Bmp File Is Corrupt");
      return 0;
    }
    switch (bmp_ptr->bitCounts) {
      case 1:
      case 4:
      case 8:
      case 16:
      case 24: {
        if (bmp_ptr->color_used > ((uint32_t)1) << bmp_ptr->bitCounts) {
          bmp_error(bmp_ptr, "The Bmp File Is Corrupt");
          return 0;
        }
      }
      case 32:
        break;
      default:
        bmp_error(bmp_ptr, "The Bmp File Is Corrupt");
        return 0;
    }
    bmp_ptr->src_row_bytes = BMP_WIDTHBYTES(bmp_ptr->width, bmp_ptr->bitCounts);
    switch (bmp_ptr->bitCounts) {
      case 1:
      case 4:
      case 8:
        bmp_ptr->out_row_bytes = BMP_WIDTHBYTES(bmp_ptr->width, 8);
        bmp_ptr->components = 1;
        break;
      case 16:
      case 24:
        bmp_ptr->out_row_bytes = BMP_WIDTHBYTES(bmp_ptr->width, 24);
        bmp_ptr->components = 3;
        break;
      case 32:
        bmp_ptr->out_row_bytes = bmp_ptr->src_row_bytes;
        bmp_ptr->components = 4;
        break;
    }
    FX_Free(bmp_ptr->out_row_buffer);

    if (bmp_ptr->out_row_bytes <= 0) {
      bmp_error(bmp_ptr, "The Bmp File Is Corrupt");
      return 0;
    }

    bmp_ptr->out_row_buffer = FX_Alloc(uint8_t, bmp_ptr->out_row_bytes);
    FXSYS_memset(bmp_ptr->out_row_buffer, 0, bmp_ptr->out_row_bytes);
    bmp_save_decoding_status(bmp_ptr, BMP_D_STATUS_PAL);
  }
  if (bmp_ptr->decode_status == BMP_D_STATUS_PAL) {
    skip_size_org = bmp_ptr->skip_size;
    if (bmp_ptr->compress_flag == BMP_BITFIELDS) {
      if (bmp_ptr->bitCounts != 16 && bmp_ptr->bitCounts != 32) {
        bmp_error(bmp_ptr, "The Bmp File Is Corrupt");
        return 0;
      }
      uint32_t* mask;
      if (bmp_read_data(bmp_ptr, (uint8_t**)&mask, 3 * sizeof(uint32_t)) ==
          nullptr) {
        bmp_ptr->skip_size = skip_size_org;
        return 2;
      }
      bmp_ptr->mask_red = GetDWord_LSBFirst((uint8_t*)&mask[0]);
      bmp_ptr->mask_green = GetDWord_LSBFirst((uint8_t*)&mask[1]);
      bmp_ptr->mask_blue = GetDWord_LSBFirst((uint8_t*)&mask[2]);
      if (bmp_ptr->mask_red & bmp_ptr->mask_green ||
          bmp_ptr->mask_red & bmp_ptr->mask_blue ||
          bmp_ptr->mask_green & bmp_ptr->mask_blue) {
        bmp_error(bmp_ptr, "The Bitfield Bmp File Is Corrupt");
        return 0;
      }
      if (bmp_ptr->bmp_header_ptr->bfOffBits < 26 + bmp_ptr->img_ifh_size) {
        bmp_ptr->bmp_header_ptr->bfOffBits = 26 + bmp_ptr->img_ifh_size;
      }
      bmp_save_decoding_status(bmp_ptr, BMP_D_STATUS_DATA_PRE);
      return 1;
    } else if (bmp_ptr->bitCounts == 16) {
      bmp_ptr->mask_red = 0x7C00;
      bmp_ptr->mask_green = 0x03E0;
      bmp_ptr->mask_blue = 0x001F;
    }
    bmp_ptr->pal_num = 0;
    if (bmp_ptr->bitCounts < 16) {
      bmp_ptr->pal_num = 1 << bmp_ptr->bitCounts;
      if (bmp_ptr->color_used != 0) {
        bmp_ptr->pal_num = bmp_ptr->color_used;
      }
      uint8_t* src_pal_ptr = nullptr;
      uint32_t src_pal_size = bmp_ptr->pal_num * (bmp_ptr->pal_type ? 3 : 4);
      if (bmp_read_data(bmp_ptr, (uint8_t**)&src_pal_ptr, src_pal_size) ==
          nullptr) {
        bmp_ptr->skip_size = skip_size_org;
        return 2;
      }
      FX_Free(bmp_ptr->pal_ptr);
      bmp_ptr->pal_ptr = FX_Alloc(uint32_t, bmp_ptr->pal_num);
      int32_t src_pal_index = 0;
      if (bmp_ptr->pal_type == BMP_PAL_OLD) {
        while (src_pal_index < bmp_ptr->pal_num) {
          bmp_ptr->pal_ptr[src_pal_index++] = BMP_PAL_ENCODE(
              0x00, src_pal_ptr[2], src_pal_ptr[1], src_pal_ptr[0]);
          src_pal_ptr += 3;
        }
      } else {
        while (src_pal_index < bmp_ptr->pal_num) {
          bmp_ptr->pal_ptr[src_pal_index++] = BMP_PAL_ENCODE(
              src_pal_ptr[3], src_pal_ptr[2], src_pal_ptr[1], src_pal_ptr[0]);
          src_pal_ptr += 4;
        }
      }
    }
    if (bmp_ptr->bmp_header_ptr->bfOffBits <
        14 + bmp_ptr->img_ifh_size +
            bmp_ptr->pal_num * (bmp_ptr->pal_type ? 3 : 4)) {
      bmp_ptr->bmp_header_ptr->bfOffBits =
          14 + bmp_ptr->img_ifh_size +
          bmp_ptr->pal_num * (bmp_ptr->pal_type ? 3 : 4);
    }
    bmp_save_decoding_status(bmp_ptr, BMP_D_STATUS_DATA_PRE);
  }
  return 1;
}
int32_t bmp_decode_image(bmp_decompress_struct_p bmp_ptr) {
  if (bmp_ptr->decode_status == BMP_D_STATUS_DATA_PRE) {
    bmp_ptr->avail_in = 0;
    if (!bmp_ptr->bmp_get_data_position_fn(
            bmp_ptr, bmp_ptr->bmp_header_ptr->bfOffBits)) {
      bmp_ptr->decode_status = BMP_D_STATUS_TAIL;
      bmp_error(bmp_ptr, "The Bmp File Is Corrupt, Unexpected Stream Offset");
      return 0;
    }
    bmp_ptr->row_num = 0;
    bmp_save_decoding_status(bmp_ptr, BMP_D_STATUS_DATA);
  }
  if (bmp_ptr->decode_status == BMP_D_STATUS_DATA) {
    switch (bmp_ptr->compress_flag) {
      case BMP_RGB:
      case BMP_BITFIELDS:
        return bmp_decode_rgb(bmp_ptr);
      case BMP_RLE8:
        return bmp_decode_rle8(bmp_ptr);
      case BMP_RLE4:
        return bmp_decode_rle4(bmp_ptr);
    }
  }
  bmp_error(bmp_ptr, "Any Uncontrol Error");
  return 0;
}
int32_t bmp_decode_rgb(bmp_decompress_struct_p bmp_ptr) {
  uint8_t* row_buf = bmp_ptr->out_row_buffer;
  uint8_t* des_buf = nullptr;
  while (bmp_ptr->row_num < bmp_ptr->height) {
    if (!bmp_read_data(bmp_ptr, &des_buf, bmp_ptr->src_row_bytes))
      return 2;

    bmp_save_decoding_status(bmp_ptr, BMP_D_STATUS_DATA);
    switch (bmp_ptr->bitCounts) {
      case 1: {
        for (int32_t col = 0; col < bmp_ptr->width; col++) {
          *row_buf++ = des_buf[col >> 3] & (0x80 >> (col % 8)) ? 0x01 : 0x00;
        }
      } break;
      case 4: {
        for (int32_t col = 0; col < bmp_ptr->width; col++) {
          *row_buf++ = (col & 0x01) ? (des_buf[col >> 1] & 0x0F)
                                    : ((des_buf[col >> 1] & 0xF0) >> 4);
        }
      } break;
      case 16: {
        uint16_t* buf = (uint16_t*)des_buf;
        uint8_t blue_bits = 0;
        uint8_t green_bits = 0;
        uint8_t red_bits = 0;
        for (int32_t i = 0; i < 16; i++) {
          if ((bmp_ptr->mask_blue >> i) & 0x01) {
            blue_bits++;
          }
          if ((bmp_ptr->mask_green >> i) & 0x01) {
            green_bits++;
          }
          if ((bmp_ptr->mask_red >> i) & 0x01) {
            red_bits++;
          }
        }
        green_bits += blue_bits;
        red_bits += green_bits;
        if (blue_bits > 8 || green_bits < 8 || red_bits < 8)
          return 2;
        blue_bits = 8 - blue_bits;
        green_bits -= 8;
        red_bits -= 8;
        for (int32_t col = 0; col < bmp_ptr->width; col++) {
          *buf = GetWord_LSBFirst((uint8_t*)buf);
          *row_buf++ = (uint8_t)((*buf & bmp_ptr->mask_blue) << blue_bits);
          *row_buf++ = (uint8_t)((*buf & bmp_ptr->mask_green) >> green_bits);
          *row_buf++ = (uint8_t)((*buf++ & bmp_ptr->mask_red) >> red_bits);
        }
      } break;
      case 8:
      case 24:
      case 32:
        FXSYS_memcpy(bmp_ptr->out_row_buffer, des_buf, bmp_ptr->src_row_bytes);
        break;
    }
    row_buf = bmp_ptr->out_row_buffer;
    bmp_ptr->bmp_get_row_fn(bmp_ptr,
                            bmp_ptr->imgTB_flag
                                ? bmp_ptr->row_num++
                                : (bmp_ptr->height - 1 - bmp_ptr->row_num++),
                            bmp_ptr->out_row_buffer);
  }
  bmp_save_decoding_status(bmp_ptr, BMP_D_STATUS_TAIL);
  return 1;
}
int32_t bmp_decode_rle8(bmp_decompress_struct_p bmp_ptr) {
  uint8_t* first_byte_ptr = nullptr;
  uint8_t* second_byte_ptr = nullptr;
  bmp_ptr->col_num = 0;
  while (true) {
    uint32_t skip_size_org = bmp_ptr->skip_size;
    if (!bmp_read_data(bmp_ptr, &first_byte_ptr, 1))
      return 2;

    switch (*first_byte_ptr) {
      case RLE_MARKER: {
        if (!bmp_read_data(bmp_ptr, &first_byte_ptr, 1)) {
          bmp_ptr->skip_size = skip_size_org;
          return 2;
        }
        switch (*first_byte_ptr) {
          case RLE_EOL: {
            if (bmp_ptr->row_num >= bmp_ptr->height) {
              bmp_save_decoding_status(bmp_ptr, BMP_D_STATUS_TAIL);
              bmp_error(bmp_ptr, "The Bmp File Is Corrupt");
              return 0;
            }
            bmp_ptr->bmp_get_row_fn(
                bmp_ptr, bmp_ptr->imgTB_flag
                             ? bmp_ptr->row_num++
                             : (bmp_ptr->height - 1 - bmp_ptr->row_num++),
                bmp_ptr->out_row_buffer);
            bmp_ptr->col_num = 0;
            FXSYS_memset(bmp_ptr->out_row_buffer, 0, bmp_ptr->out_row_bytes);
            bmp_save_decoding_status(bmp_ptr, BMP_D_STATUS_DATA);
            continue;
          }
          case RLE_EOI: {
            if (bmp_ptr->row_num < bmp_ptr->height) {
              bmp_ptr->bmp_get_row_fn(
                  bmp_ptr, bmp_ptr->imgTB_flag
                               ? bmp_ptr->row_num++
                               : (bmp_ptr->height - 1 - bmp_ptr->row_num++),
                  bmp_ptr->out_row_buffer);
            }
            bmp_save_decoding_status(bmp_ptr, BMP_D_STATUS_TAIL);
            return 1;
          }
          case RLE_DELTA: {
            uint8_t* delta_ptr;
            if (!bmp_read_data(bmp_ptr, &delta_ptr, 2)) {
              bmp_ptr->skip_size = skip_size_org;
              return 2;
            }
            bmp_ptr->col_num += (int32_t)delta_ptr[0];
            int32_t bmp_row_num_next = bmp_ptr->row_num + (int32_t)delta_ptr[1];
            if (bmp_ptr->col_num >= bmp_ptr->out_row_bytes ||
                bmp_row_num_next >= bmp_ptr->height) {
              bmp_error(bmp_ptr, "The Bmp File Is Corrupt Or Not Supported");
              return 0;
            }
            while (bmp_ptr->row_num < bmp_row_num_next) {
              FXSYS_memset(bmp_ptr->out_row_buffer, 0, bmp_ptr->out_row_bytes);
              bmp_ptr->bmp_get_row_fn(
                  bmp_ptr, bmp_ptr->imgTB_flag
                               ? bmp_ptr->row_num++
                               : (bmp_ptr->height - 1 - bmp_ptr->row_num++),
                  bmp_ptr->out_row_buffer);
            }
          } break;
          default: {
            if ((int32_t)(*first_byte_ptr) >
                bmp_ptr->src_row_bytes - bmp_ptr->col_num) {
              bmp_error(bmp_ptr, "The Bmp File Is Corrupt");
              return 0;
            }
            if (!bmp_read_data(bmp_ptr, &second_byte_ptr,
                               *first_byte_ptr & 1 ? *first_byte_ptr + 1
                                                   : *first_byte_ptr)) {
              bmp_ptr->skip_size = skip_size_org;
              return 2;
            }
            FXSYS_memcpy(bmp_ptr->out_row_buffer + bmp_ptr->col_num,
                         second_byte_ptr, *first_byte_ptr);
            bmp_ptr->col_num += (int32_t)(*first_byte_ptr);
          }
        }
      } break;
      default: {
        if (!bmp_read_data(bmp_ptr, &second_byte_ptr, 1)) {
          bmp_ptr->skip_size = skip_size_org;
          return 2;
        }
        if ((int32_t)(*first_byte_ptr) >
            bmp_ptr->src_row_bytes - bmp_ptr->col_num) {
          bmp_error(bmp_ptr, "The Bmp File Is Corrupt");
          return 0;
        }
        FXSYS_memset(bmp_ptr->out_row_buffer + bmp_ptr->col_num,
                     *second_byte_ptr, *first_byte_ptr);
        bmp_ptr->col_num += (int32_t)(*first_byte_ptr);
      }
    }
  }
  bmp_error(bmp_ptr, "Any Uncontrol Error");
  return 0;
}
int32_t bmp_decode_rle4(bmp_decompress_struct_p bmp_ptr) {
  uint8_t* first_byte_ptr = nullptr;
  uint8_t* second_byte_ptr = nullptr;
  bmp_ptr->col_num = 0;
  while (true) {
    uint32_t skip_size_org = bmp_ptr->skip_size;
    if (!bmp_read_data(bmp_ptr, &first_byte_ptr, 1))
      return 2;

    switch (*first_byte_ptr) {
      case RLE_MARKER: {
        if (!bmp_read_data(bmp_ptr, &first_byte_ptr, 1)) {
          bmp_ptr->skip_size = skip_size_org;
          return 2;
        }
        switch (*first_byte_ptr) {
          case RLE_EOL: {
            if (bmp_ptr->row_num >= bmp_ptr->height) {
              bmp_save_decoding_status(bmp_ptr, BMP_D_STATUS_TAIL);
              bmp_error(bmp_ptr, "The Bmp File Is Corrupt");
              return 0;
            }
            bmp_ptr->bmp_get_row_fn(
                bmp_ptr, bmp_ptr->imgTB_flag
                             ? bmp_ptr->row_num++
                             : (bmp_ptr->height - 1 - bmp_ptr->row_num++),
                bmp_ptr->out_row_buffer);
            bmp_ptr->col_num = 0;
            FXSYS_memset(bmp_ptr->out_row_buffer, 0, bmp_ptr->out_row_bytes);
            bmp_save_decoding_status(bmp_ptr, BMP_D_STATUS_DATA);
            continue;
          }
          case RLE_EOI: {
            if (bmp_ptr->row_num < bmp_ptr->height) {
              bmp_ptr->bmp_get_row_fn(
                  bmp_ptr, bmp_ptr->imgTB_flag
                               ? bmp_ptr->row_num++
                               : (bmp_ptr->height - 1 - bmp_ptr->row_num++),
                  bmp_ptr->out_row_buffer);
            }
            bmp_save_decoding_status(bmp_ptr, BMP_D_STATUS_TAIL);
            return 1;
          }
          case RLE_DELTA: {
            uint8_t* delta_ptr;
            if (!bmp_read_data(bmp_ptr, &delta_ptr, 2)) {
              bmp_ptr->skip_size = skip_size_org;
              return 2;
            }
            bmp_ptr->col_num += (int32_t)delta_ptr[0];
            int32_t bmp_row_num_next = bmp_ptr->row_num + (int32_t)delta_ptr[1];
            if (bmp_ptr->col_num >= bmp_ptr->out_row_bytes ||
                bmp_row_num_next >= bmp_ptr->height) {
              bmp_error(bmp_ptr, "The Bmp File Is Corrupt Or Not Supported");
              return 0;
            }
            while (bmp_ptr->row_num < bmp_row_num_next) {
              FXSYS_memset(bmp_ptr->out_row_buffer, 0, bmp_ptr->out_row_bytes);
              bmp_ptr->bmp_get_row_fn(
                  bmp_ptr, bmp_ptr->imgTB_flag
                               ? bmp_ptr->row_num++
                               : (bmp_ptr->height - 1 - bmp_ptr->row_num++),
                  bmp_ptr->out_row_buffer);
            }
          } break;
          default: {
            uint8_t size = (uint8_t)(((uint16_t)(*first_byte_ptr) + 1) >> 1);
            if ((int32_t)*first_byte_ptr >=
                bmp_ptr->out_row_bytes - bmp_ptr->col_num) {
              if (size + (bmp_ptr->col_num >> 1) > bmp_ptr->src_row_bytes) {
                bmp_error(bmp_ptr, "The Bmp File Is Corrupt");
                return 0;
              }
              *first_byte_ptr = bmp_ptr->out_row_bytes - bmp_ptr->col_num - 1;
            }
            if (!bmp_read_data(bmp_ptr, &second_byte_ptr,
                               size & 1 ? size + 1 : size)) {
              bmp_ptr->skip_size = skip_size_org;
              return 2;
            }
            for (uint8_t i = 0; i < *first_byte_ptr; i++) {
              if (i & 0x01) {
                *(bmp_ptr->out_row_buffer + bmp_ptr->col_num++) =
                    (*second_byte_ptr++ & 0x0F);
              } else {
                *(bmp_ptr->out_row_buffer + bmp_ptr->col_num++) =
                    ((*second_byte_ptr & 0xF0) >> 4);
              }
            }
          }
        }
      } break;
      default: {
        if (!bmp_read_data(bmp_ptr, &second_byte_ptr, 1)) {
          bmp_ptr->skip_size = skip_size_org;
          return 2;
        }
        if ((int32_t)*first_byte_ptr >
            bmp_ptr->out_row_bytes - bmp_ptr->col_num) {
          uint8_t size = (uint8_t)(((uint16_t)(*first_byte_ptr) + 1) >> 1);
          if (size + (bmp_ptr->col_num >> 1) > bmp_ptr->src_row_bytes) {
            bmp_error(bmp_ptr, "The Bmp File Is Corrupt");
            return 0;
          }
          *first_byte_ptr = bmp_ptr->out_row_bytes - bmp_ptr->col_num - 1;
        }
        for (uint8_t i = 0; i < *first_byte_ptr; i++) {
          if (i & 0x01) {
            *(bmp_ptr->out_row_buffer + bmp_ptr->col_num++) =
                (*second_byte_ptr & 0x0F);
          } else {
            *(bmp_ptr->out_row_buffer + bmp_ptr->col_num++) =
                ((*second_byte_ptr & 0xF0) >> 4);
          }
        }
      }
    }
  }
  bmp_error(bmp_ptr, "Any Uncontrol Error");
  return 0;
}
uint8_t* bmp_read_data(bmp_decompress_struct_p bmp_ptr,
                       uint8_t** des_buf_pp,
                       uint32_t data_size) {
  if (!bmp_ptr || bmp_ptr->avail_in < bmp_ptr->skip_size + data_size)
    return nullptr;

  *des_buf_pp = bmp_ptr->next_in + bmp_ptr->skip_size;
  bmp_ptr->skip_size += data_size;
  return *des_buf_pp;
}
void bmp_save_decoding_status(bmp_decompress_struct_p bmp_ptr, int32_t status) {
  bmp_ptr->decode_status = status;
  bmp_ptr->next_in += bmp_ptr->skip_size;
  bmp_ptr->avail_in -= bmp_ptr->skip_size;
  bmp_ptr->skip_size = 0;
}
void bmp_input_buffer(bmp_decompress_struct_p bmp_ptr,
                      uint8_t* src_buf,
                      uint32_t src_size) {
  bmp_ptr->next_in = src_buf;
  bmp_ptr->avail_in = src_size;
  bmp_ptr->skip_size = 0;
}
uint32_t bmp_get_avail_input(bmp_decompress_struct_p bmp_ptr,
                             uint8_t** avail_buf_ptr) {
  if (avail_buf_ptr) {
    *avail_buf_ptr = nullptr;
    if (bmp_ptr->avail_in > 0) {
      *avail_buf_ptr = bmp_ptr->next_in;
    }
  }
  return bmp_ptr->avail_in;
}
bmp_compress_struct_p bmp_create_compress() {
  bmp_compress_struct_p bmp_ptr;
  bmp_ptr = FX_Alloc(bmp_compress_struct, 1);
  if (bmp_ptr) {
    FXSYS_memset(bmp_ptr, 0, sizeof(bmp_compress_struct));
  }
  return bmp_ptr;
}
void bmp_destroy_compress(bmp_compress_struct_p bmp_ptr) {
  if (bmp_ptr) {
    if (bmp_ptr->src_free && bmp_ptr->src_buf) {
      FX_Free(bmp_ptr->src_buf);
    }
    FX_Free(bmp_ptr);
  }
}
static void WriteFileHeader(BmpFileHeaderPtr head_ptr, uint8_t* dst_buf) {
  uint32_t offset;
  offset = 0;
  SetWord_LSBFirst(&dst_buf[offset], head_ptr->bfType);
  offset += 2;
  SetDWord_LSBFirst(&dst_buf[offset], head_ptr->bfSize);
  offset += 4;
  SetWord_LSBFirst(&dst_buf[offset], head_ptr->bfReserved1);
  offset += 2;
  SetWord_LSBFirst(&dst_buf[offset], head_ptr->bfReserved2);
  offset += 2;
  SetDWord_LSBFirst(&dst_buf[offset], head_ptr->bfOffBits);
  offset += 4;
}
static void WriteInfoHeader(BmpInfoHeaderPtr info_head_ptr, uint8_t* dst_buf) {
  uint32_t offset;
  offset = sizeof(BmpFileHeader);
  SetDWord_LSBFirst(&dst_buf[offset], info_head_ptr->biSize);
  offset += 4;
  SetDWord_LSBFirst(&dst_buf[offset], info_head_ptr->biWidth);
  offset += 4;
  SetDWord_LSBFirst(&dst_buf[offset], info_head_ptr->biHeight);
  offset += 4;
  SetWord_LSBFirst(&dst_buf[offset], info_head_ptr->biPlanes);
  offset += 2;
  SetWord_LSBFirst(&dst_buf[offset], info_head_ptr->biBitCount);
  offset += 2;
  SetDWord_LSBFirst(&dst_buf[offset], info_head_ptr->biCompression);
  offset += 4;
  SetDWord_LSBFirst(&dst_buf[offset], info_head_ptr->biSizeImage);
  offset += 4;
  SetDWord_LSBFirst(&dst_buf[offset], info_head_ptr->biXPelsPerMeter);
  offset += 4;
  SetDWord_LSBFirst(&dst_buf[offset], info_head_ptr->biYPelsPerMeter);
  offset += 4;
  SetDWord_LSBFirst(&dst_buf[offset], info_head_ptr->biClrUsed);
  offset += 4;
  SetDWord_LSBFirst(&dst_buf[offset], info_head_ptr->biClrImportant);
  offset += 4;
}
static void bmp_encode_bitfields(bmp_compress_struct_p bmp_ptr,
                                 uint8_t*& dst_buf,
                                 uint32_t& dst_size) {
  if (bmp_ptr->info_header.biBitCount != 16 &&
      bmp_ptr->info_header.biBitCount != 32) {
    return;
  }
  uint32_t size, dst_pos, i;
  size = bmp_ptr->src_pitch * bmp_ptr->src_row *
         bmp_ptr->info_header.biBitCount / 16;
  dst_pos = bmp_ptr->file_header.bfOffBits;
  dst_size += size;
  dst_buf = FX_Realloc(uint8_t, dst_buf, dst_size);
  FXSYS_memset(&dst_buf[dst_pos], 0, size);
  uint32_t mask_red;
  uint32_t mask_green;
  uint32_t mask_blue;
  mask_red = 0x7C00;
  mask_green = 0x03E0;
  mask_blue = 0x001F;
  if (bmp_ptr->info_header.biCompression == BMP_BITFIELDS) {
    if (bmp_ptr->bit_type == BMP_BIT_565) {
      mask_red = 0xF800;
      mask_green = 0x07E0;
      mask_blue = 0x001F;
    }
    if (bmp_ptr->info_header.biBitCount == 32) {
      mask_red = 0xFF0000;
      mask_green = 0x00FF00;
      mask_blue = 0x0000FF;
    }
    SetDWord_LSBFirst(&dst_buf[dst_pos], mask_red);
    dst_pos += 4;
    SetDWord_LSBFirst(&dst_buf[dst_pos], mask_green);
    dst_pos += 4;
    SetDWord_LSBFirst(&dst_buf[dst_pos], mask_blue);
    dst_pos += 4;
    bmp_ptr->file_header.bfOffBits = dst_pos;
  }
  uint8_t blue_bits = 0;
  uint8_t green_bits = 0;
  uint8_t red_bits = 0;
  for (i = 0; i < bmp_ptr->info_header.biBitCount; i++) {
    if ((mask_blue >> i) & 0x01) {
      blue_bits++;
    }
    if ((mask_green >> i) & 0x01) {
      green_bits++;
    }
    if ((mask_red >> i) & 0x01) {
      red_bits++;
    }
  }
  green_bits += blue_bits;
  red_bits += green_bits;
  blue_bits = 8 - blue_bits;
  green_bits -= 8;
  red_bits -= 8;
  i = 0;
  for (int32_t row_num = bmp_ptr->src_row - 1; row_num > -1; row_num--, i = 0) {
    while (i < bmp_ptr->src_width * bmp_ptr->src_bpp / 8) {
      uint8_t b = bmp_ptr->src_buf[row_num * bmp_ptr->src_pitch + i++];
      uint8_t g = bmp_ptr->src_buf[row_num * bmp_ptr->src_pitch + i++];
      uint8_t r = bmp_ptr->src_buf[row_num * bmp_ptr->src_pitch + i++];
      if (bmp_ptr->src_bpp == 32) {
        i++;
      }
      uint32_t pix_val = 0;
      pix_val |= (b >> blue_bits) & mask_blue;
      pix_val |= (g << green_bits) & mask_green;
      pix_val |= (r << red_bits) & mask_red;
      if (bmp_ptr->info_header.biBitCount == 16) {
        SetWord_LSBFirst(&dst_buf[dst_pos], pix_val);
        dst_pos += 2;
      } else {
        SetDWord_LSBFirst(&dst_buf[dst_pos], pix_val);
        dst_pos += 4;
      }
    }
  }
  dst_size = dst_pos;
}

static void bmp_encode_rgb(bmp_compress_struct_p bmp_ptr,
                           uint8_t*& dst_buf,
                           uint32_t& dst_size) {
  if (bmp_ptr->info_header.biBitCount == 16) {
    bmp_encode_bitfields(bmp_ptr, dst_buf, dst_size);
    return;
  }
  uint32_t size, dst_pos;
  uint32_t dst_pitch =
      (bmp_ptr->src_width * bmp_ptr->info_header.biBitCount + 31) / 32 * 4;
  size = dst_pitch * bmp_ptr->src_row;
  dst_pos = bmp_ptr->file_header.bfOffBits;
  dst_size += size;
  dst_buf = FX_Realloc(uint8_t, dst_buf, dst_size);
  FXSYS_memset(&dst_buf[dst_pos], 0, size);
  for (int32_t row_num = bmp_ptr->src_row - 1; row_num > -1; row_num--) {
    FXSYS_memcpy(&dst_buf[dst_pos],
                 &bmp_ptr->src_buf[row_num * bmp_ptr->src_pitch],
                 bmp_ptr->src_pitch);
    dst_pos += dst_pitch;
  }
  dst_size = dst_pos;
}
static uint8_t bmp_rle8_search(const uint8_t* buf, int32_t len) {
  uint8_t num;
  num = 1;
  while (num < len) {
    if (buf[num - 1] != buf[num] || num == 0xFF) {
      break;
    }
    num++;
  }
  return num;
}
static void bmp_encode_rle8(bmp_compress_struct_p bmp_ptr,
                            uint8_t*& dst_buf,
                            uint32_t& dst_size) {
  uint32_t size, dst_pos, index;
  uint8_t rle[2] = {0};
  size = bmp_ptr->src_pitch * bmp_ptr->src_row * 2;
  dst_pos = bmp_ptr->file_header.bfOffBits;
  dst_size += size;
  dst_buf = FX_Realloc(uint8_t, dst_buf, dst_size);
  FXSYS_memset(&dst_buf[dst_pos], 0, size);
  for (int32_t row_num = bmp_ptr->src_row - 1, i = 0; row_num > -1;) {
    index = row_num * bmp_ptr->src_pitch;
    rle[0] = bmp_rle8_search(&bmp_ptr->src_buf[index + i], size - index - i);
    rle[1] = bmp_ptr->src_buf[index + i];
    if (i + rle[0] >= (int32_t)bmp_ptr->src_pitch) {
      rle[0] = uint8_t(bmp_ptr->src_pitch - i);
      if (rle[0]) {
        dst_buf[dst_pos++] = rle[0];
        dst_buf[dst_pos++] = rle[1];
      }
      dst_buf[dst_pos++] = RLE_MARKER;
      dst_buf[dst_pos++] = RLE_EOL;
      i = 0;
      row_num--;
    } else {
      i += rle[0];
      dst_buf[dst_pos++] = rle[0];
      dst_buf[dst_pos++] = rle[1];
    }
  }
  dst_buf[dst_pos++] = RLE_MARKER;
  dst_buf[dst_pos++] = RLE_EOI;
  dst_size = dst_pos;
}
static uint8_t bmp_rle4_search(const uint8_t* buf, int32_t len) {
  uint8_t num;
  num = 2;
  while (num < len) {
    if (buf[num - 2] != buf[num] || num == 0xFF) {
      break;
    }
    num++;
  }
  return num;
}
static void bmp_encode_rle4(bmp_compress_struct_p bmp_ptr,
                            uint8_t*& dst_buf,
                            uint32_t& dst_size) {
  uint32_t size, dst_pos, index;
  uint8_t rle[2] = {0};
  size = bmp_ptr->src_pitch * bmp_ptr->src_row;
  dst_pos = bmp_ptr->file_header.bfOffBits;
  dst_size += size;
  dst_buf = FX_Realloc(uint8_t, dst_buf, dst_size);
  FXSYS_memset(&dst_buf[dst_pos], 0, size);
  for (int32_t row_num = bmp_ptr->src_row - 1, i = 0; row_num > -1;
       rle[1] = 0) {
    index = row_num * bmp_ptr->src_pitch;
    rle[0] = bmp_rle4_search(&bmp_ptr->src_buf[index + i], size - index - i);
    rle[1] |= (bmp_ptr->src_buf[index + i] & 0x0f) << 4;
    rle[1] |= bmp_ptr->src_buf[index + i + 1] & 0x0f;
    if (i + rle[0] >= (int32_t)bmp_ptr->src_pitch) {
      rle[0] = uint8_t(bmp_ptr->src_pitch - i);
      if (rle[0]) {
        dst_buf[dst_pos++] = rle[0];
        dst_buf[dst_pos++] = rle[1];
      }
      dst_buf[dst_pos++] = RLE_MARKER;
      dst_buf[dst_pos++] = RLE_EOL;
      i = 0;
      row_num--;
    } else {
      i += rle[0];
      dst_buf[dst_pos++] = rle[0];
      dst_buf[dst_pos++] = rle[1];
    }
  }
  dst_buf[dst_pos++] = RLE_MARKER;
  dst_buf[dst_pos++] = RLE_EOI;
  dst_size = dst_pos;
}
bool bmp_encode_image(bmp_compress_struct_p bmp_ptr,
                      uint8_t*& dst_buf,
                      uint32_t& dst_size) {
  uint32_t head_size = sizeof(BmpFileHeader) + sizeof(BmpInfoHeader);
  uint32_t pal_size = sizeof(uint32_t) * bmp_ptr->pal_num;
  if (bmp_ptr->info_header.biClrUsed > 0 &&
      bmp_ptr->info_header.biClrUsed < bmp_ptr->pal_num) {
    pal_size = sizeof(uint32_t) * bmp_ptr->info_header.biClrUsed;
  }
  dst_size = head_size + sizeof(uint32_t) * bmp_ptr->pal_num;
  dst_buf = FX_TryAlloc(uint8_t, dst_size);
  if (!dst_buf)
    return false;

  FXSYS_memset(dst_buf, 0, dst_size);
  bmp_ptr->file_header.bfOffBits = head_size;
  if (bmp_ptr->pal_ptr && pal_size) {
    FXSYS_memcpy(&dst_buf[head_size], bmp_ptr->pal_ptr, pal_size);
    bmp_ptr->file_header.bfOffBits += pal_size;
  }
  WriteInfoHeader(&bmp_ptr->info_header, dst_buf);
  switch (bmp_ptr->info_header.biCompression) {
    case BMP_RGB:
      bmp_encode_rgb(bmp_ptr, dst_buf, dst_size);
      break;
    case BMP_BITFIELDS:
      bmp_encode_bitfields(bmp_ptr, dst_buf, dst_size);
      break;
    case BMP_RLE8:
      bmp_encode_rle8(bmp_ptr, dst_buf, dst_size);
      break;
    case BMP_RLE4:
      bmp_encode_rle4(bmp_ptr, dst_buf, dst_size);
      break;
    default:
      break;
  }
  bmp_ptr->file_header.bfSize = dst_size;
  WriteFileHeader(&bmp_ptr->file_header, dst_buf);
  return true;
}

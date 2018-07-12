/*
 * Copyright (c) 2016, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

/*!\defgroup aom_decoder AOMedia AOM/AV1 Decoder
 * \ingroup aom
 *
 * @{
 */
/*!\file
 * \brief Provides definitions for using AOM or AV1 within the aom Decoder
 *        interface.
 */
#ifndef AOM_AOMDX_H_
#define AOM_AOMDX_H_

#ifdef __cplusplus
extern "C" {
#endif

/* Include controls common to both the encoder and decoder */
#include "./aom.h"

/*!\name Algorithm interface for AV1
 *
 * This interface provides the capability to decode AV1 streams.
 * @{
 */
extern aom_codec_iface_t aom_codec_av1_dx_algo;
extern aom_codec_iface_t *aom_codec_av1_dx(void);
/*!@} - end algorithm interface member group*/

#ifndef AOM_ACCOUNTING_H_
/** Data structure that stores bit accounting for debug
 */
typedef struct Accounting Accounting;
#endif

#ifndef AOM_INSPECTION_H_
/** Callback that inspects decoder frame data.
 */
typedef void (*aom_inspect_cb)(void *decoder, void *ctx);
#endif

/*!\brief Structure to hold inspection callback and context.
 *
 * Defines a structure to hold the inspection callback function and calling
 * context.
 */
typedef struct aom_inspect_init {
  /*! Inspection callback. */
  aom_inspect_cb inspect_cb;

  /*! Inspection context. */
  void *inspect_ctx;
} aom_inspect_init;

/*!\enum aom_dec_control_id
 * \brief AOM decoder control functions
 *
 * This set of macros define the control functions available for the AOM
 * decoder interface.
 *
 * \sa #aom_codec_control
 */
enum aom_dec_control_id {
  /** control function to get info on which reference frames were updated
   *  by the last decode
   */
  AOMD_GET_LAST_REF_UPDATES = AOM_DECODER_CTRL_ID_START,

  /** check if the indicated frame is corrupted */
  AOMD_GET_FRAME_CORRUPTED,

  /** control function to get info on which reference frames were used
   *  by the last decode
   */
  AOMD_GET_LAST_REF_USED,

  /** decryption function to decrypt encoded buffer data immediately
   * before decoding. Takes a aom_decrypt_init, which contains
   * a callback function and opaque context pointer.
   */
  AOMD_SET_DECRYPTOR,
  // AOMD_SET_DECRYPTOR = AOMD_SET_DECRYPTOR,

  /** control function to get the dimensions that the current frame is decoded
   * at. This may be different to the intended display size for the frame as
   * specified in the wrapper or frame header (see AV1D_GET_DISPLAY_SIZE). */
  AV1D_GET_FRAME_SIZE,

  /** control function to get the current frame's intended display dimensions
   * (as specified in the wrapper or frame header). This may be different to
   * the decoded dimensions of this frame (see AV1D_GET_FRAME_SIZE). */
  AV1D_GET_DISPLAY_SIZE,

  /** control function to get the bit depth of the stream. */
  AV1D_GET_BIT_DEPTH,

  /** control function to set the byte alignment of the planes in the reference
   * buffers. Valid values are power of 2, from 32 to 1024. A value of 0 sets
   * legacy alignment. I.e. Y plane is aligned to 32 bytes, U plane directly
   * follows Y plane, and V plane directly follows U plane. Default value is 0.
   */
  AV1_SET_BYTE_ALIGNMENT,

  /** control function to invert the decoding order to from right to left. The
   * function is used in a test to confirm the decoding independence of tile
   * columns. The function may be used in application where this order
   * of decoding is desired.
   *
   * TODO(yaowu): Rework the unit test that uses this control, and in a future
   *              release, this test-only control shall be removed.
   */
  AV1_INVERT_TILE_DECODE_ORDER,

  /** control function to set the skip loop filter flag. Valid values are
   * integers. The decoder will skip the loop filter when its value is set to
   * nonzero. If the loop filter is skipped the decoder may accumulate decode
   * artifacts. The default value is 0.
   */
  AV1_SET_SKIP_LOOP_FILTER,

  /** control function to retrieve a pointer to the Accounting struct.  When
   * compiled without --enable-accounting, this returns AOM_CODEC_INCAPABLE.
   * If called before a frame has been decoded, this returns AOM_CODEC_ERROR.
   * The caller should ensure that AOM_CODEC_OK is returned before attempting
   * to dereference the Accounting pointer.
   */
  AV1_GET_ACCOUNTING,

  /** control function to get last decoded frame quantizer. Returned value uses
   * internal quantizer scale defined by the codec.
   */
  AOMD_GET_LAST_QUANTIZER,

  /** control function to set the range of tile decoding. A value that is
   * greater and equal to zero indicates only the specific row/column is
   * decoded. A value that is -1 indicates the whole row/column is decoded.
   * A special case is both values are -1 that means the whole frame is
   * decoded.
   */
  AV1_SET_DECODE_TILE_ROW,
  AV1_SET_DECODE_TILE_COL,

  /** control function to set an aom_inspect_cb callback that is invoked each
   * time a frame is decoded.  When compiled without --enable-inspection, this
   * returns AOM_CODEC_INCAPABLE.
   */
  AV1_SET_INSPECTION_CALLBACK,

  AOM_DECODER_CTRL_ID_MAX,
};

/** Decrypt n bytes of data from input -> output, using the decrypt_state
 *  passed in AOMD_SET_DECRYPTOR.
 */
typedef void (*aom_decrypt_cb)(void *decrypt_state, const unsigned char *input,
                               unsigned char *output, int count);

/*!\brief Structure to hold decryption state
 *
 * Defines a structure to hold the decryption state and access function.
 */
typedef struct aom_decrypt_init {
  /*! Decrypt callback. */
  aom_decrypt_cb decrypt_cb;

  /*! Decryption state. */
  void *decrypt_state;
} aom_decrypt_init;

/*!\cond */
/*!\brief AOM decoder control function parameter type
 *
 * Defines the data types that AOMD control functions take. Note that
 * additional common controls are defined in aom.h
 *
 */

AOM_CTRL_USE_TYPE(AOMD_GET_LAST_REF_UPDATES, int *)
#define AOM_CTRL_AOMD_GET_LAST_REF_UPDATES
AOM_CTRL_USE_TYPE(AOMD_GET_FRAME_CORRUPTED, int *)
#define AOM_CTRL_AOMD_GET_FRAME_CORRUPTED
AOM_CTRL_USE_TYPE(AOMD_GET_LAST_REF_USED, int *)
#define AOM_CTRL_AOMD_GET_LAST_REF_USED
AOM_CTRL_USE_TYPE(AOMD_GET_LAST_QUANTIZER, int *)
#define AOM_CTRL_AOMD_GET_LAST_QUANTIZER
AOM_CTRL_USE_TYPE(AOMD_SET_DECRYPTOR, aom_decrypt_init *)
#define AOM_CTRL_AOMD_SET_DECRYPTOR
// AOM_CTRL_USE_TYPE(AOMD_SET_DECRYPTOR, aom_decrypt_init *)
//#define AOM_CTRL_AOMD_SET_DECRYPTOR
AOM_CTRL_USE_TYPE(AV1D_GET_DISPLAY_SIZE, int *)
#define AOM_CTRL_AV1D_GET_DISPLAY_SIZE
AOM_CTRL_USE_TYPE(AV1D_GET_BIT_DEPTH, unsigned int *)
#define AOM_CTRL_AV1D_GET_BIT_DEPTH
AOM_CTRL_USE_TYPE(AV1D_GET_FRAME_SIZE, int *)
#define AOM_CTRL_AV1D_GET_FRAME_SIZE
AOM_CTRL_USE_TYPE(AV1_INVERT_TILE_DECODE_ORDER, int)
#define AOM_CTRL_AV1_INVERT_TILE_DECODE_ORDER
AOM_CTRL_USE_TYPE(AV1_GET_ACCOUNTING, Accounting **)
#define AOM_CTRL_AV1_GET_ACCOUNTING
AOM_CTRL_USE_TYPE(AV1_SET_DECODE_TILE_ROW, int)
#define AOM_CTRL_AV1_SET_DECODE_TILE_ROW
AOM_CTRL_USE_TYPE(AV1_SET_DECODE_TILE_COL, int)
#define AOM_CTRL_AV1_SET_DECODE_TILE_COL
AOM_CTRL_USE_TYPE(AV1_SET_INSPECTION_CALLBACK, aom_inspect_init *)
#define AOM_CTRL_AV1_SET_INSPECTION_CALLBACK
/*!\endcond */
/*! @} - end defgroup aom_decoder */

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AOM_AOMDX_H_

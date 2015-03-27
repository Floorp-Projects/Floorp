/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


/*!\defgroup vp8_decoder WebM VP8 Decoder
 * \ingroup vp8
 *
 * @{
 */
/*!\file
 * \brief Provides definitions for using the VP8 algorithm within the vpx Decoder
 *        interface.
 */
#ifndef VPX_VP8DX_H_
#define VPX_VP8DX_H_

#ifdef __cplusplus
extern "C" {
#endif

/* Include controls common to both the encoder and decoder */
#include "./vp8.h"

/*!\name Algorithm interface for VP8
 *
 * This interface provides the capability to decode raw VP8 streams, as would
 * be found in AVI files and other non-Flash uses.
 * @{
 */
extern vpx_codec_iface_t  vpx_codec_vp8_dx_algo;
extern vpx_codec_iface_t *vpx_codec_vp8_dx(void);

/* TODO(jkoleszar): These move to VP9 in a later patch set. */
extern vpx_codec_iface_t  vpx_codec_vp9_dx_algo;
extern vpx_codec_iface_t *vpx_codec_vp9_dx(void);
/*!@} - end algorithm interface member group*/


/*!\enum vp8_dec_control_id
 * \brief VP8 decoder control functions
 *
 * This set of macros define the control functions available for the VP8
 * decoder interface.
 *
 * \sa #vpx_codec_control
 */
enum vp8_dec_control_id {
  /** control function to get info on which reference frames were updated
   *  by the last decode
   */
  VP8D_GET_LAST_REF_UPDATES = VP8_DECODER_CTRL_ID_START,

  /** check if the indicated frame is corrupted */
  VP8D_GET_FRAME_CORRUPTED,

  /** control function to get info on which reference frames were used
   *  by the last decode
   */
  VP8D_GET_LAST_REF_USED,

  /** decryption function to decrypt encoded buffer data immediately
   * before decoding. Takes a vp8_decrypt_init, which contains
   * a callback function and opaque context pointer.
   */
  VP8D_SET_DECRYPTOR,

  /** control function to get the display dimensions for the current frame. */
  VP9D_GET_DISPLAY_SIZE,

  /** For testing. */
  VP9_INVERT_TILE_DECODE_ORDER,

  VP8_DECODER_CTRL_ID_MAX
};

/*!\brief Structure to hold decryption state
 *
 * Defines a structure to hold the decryption state and access function.
 */
typedef struct vp8_decrypt_init {
    /** Decrypt n bytes of data from input -> output, using the decrypt_state
     *  passed in VP8D_SET_DECRYPTOR.
     */
    void (*decrypt_cb)(void *decrypt_state, const unsigned char *input,
                       unsigned char *output, int count);
    /*! Decryption state. */
    void *decrypt_state;
} vp8_decrypt_init;

/*!\brief VP8 decoder control function parameter type
 *
 * Defines the data types that VP8D control functions take. Note that
 * additional common controls are defined in vp8.h
 *
 */


VPX_CTRL_USE_TYPE(VP8D_GET_LAST_REF_UPDATES,   int *)
VPX_CTRL_USE_TYPE(VP8D_GET_FRAME_CORRUPTED,    int *)
VPX_CTRL_USE_TYPE(VP8D_GET_LAST_REF_USED,      int *)
VPX_CTRL_USE_TYPE(VP8D_SET_DECRYPTOR,          vp8_decrypt_init *)
VPX_CTRL_USE_TYPE(VP9D_GET_DISPLAY_SIZE,       int *)
VPX_CTRL_USE_TYPE(VP9_INVERT_TILE_DECODE_ORDER, int)

/*! @} - end defgroup vp8_decoder */

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // VPX_VP8DX_H_

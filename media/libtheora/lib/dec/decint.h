/********************************************************************
 *                                                                  *
 * THIS FILE IS PART OF THE OggTheora SOFTWARE CODEC SOURCE CODE.   *
 * USE, DISTRIBUTION AND REPRODUCTION OF THIS LIBRARY SOURCE IS     *
 * GOVERNED BY A BSD-STYLE SOURCE LICENSE INCLUDED WITH THIS SOURCE *
 * IN 'COPYING'. PLEASE READ THESE TERMS BEFORE DISTRIBUTING.       *
 *                                                                  *
 * THE Theora SOURCE CODE IS COPYRIGHT (C) 2002-2007                *
 * by the Xiph.Org Foundation and contributors http://www.xiph.org/ *
 *                                                                  *
 ********************************************************************

  function:
    last mod: $Id: decint.h 15400 2008-10-15 12:10:58Z tterribe $

 ********************************************************************/

#include <limits.h>
#if !defined(_decint_H)
# define _decint_H (1)
# include "theora/theoradec.h"
# include "../internal.h"
# include "bitpack.h"

typedef struct th_setup_info oc_setup_info;
typedef struct th_dec_ctx    oc_dec_ctx;

# include "idct.h"
# include "huffdec.h"
# include "dequant.h"

/*Constants for the packet-in state machine specific to the decoder.*/

/*Next packet to read: Data packet.*/
#define OC_PACKET_DATA (0)



struct th_setup_info{
  /*The Huffman codes.*/
  oc_huff_node      *huff_tables[TH_NHUFFMAN_TABLES];
  /*The quantization parameters.*/
  th_quant_info  qinfo;
};



struct th_dec_ctx{
  /*Shared encoder/decoder state.*/
  oc_theora_state      state;
  /*Whether or not packets are ready to be emitted.
    This takes on negative values while there are remaining header packets to
     be emitted, reaches 0 when the codec is ready for input, and goes to 1
     when a frame has been processed and a data packet is ready.*/
  int                  packet_state;
  /*Buffer in which to assemble packets.*/
  oggpack_buffer       opb;
  /*Huffman decode trees.*/
  oc_huff_node        *huff_tables[TH_NHUFFMAN_TABLES];
  /*The index of one past the last token in each plane for each coefficient.
    The final entries are the total number of tokens for each coefficient.*/
  int                  ti0[3][64];
  /*The index of one past the last extra bits entry in each plane for each
     coefficient.
    The final entries are the total number of extra bits entries for each
     coefficient.*/
  int                  ebi0[3][64];
  /*The number of outstanding EOB runs at the start of each coefficient in each
     plane.*/
  int                  eob_runs[3][64];
  /*The DCT token lists.*/
  unsigned char      **dct_tokens;
  /*The extra bits associated with DCT tokens.*/
  ogg_uint16_t       **extra_bits;
  /*The out-of-loop post-processing level.*/
  int                  pp_level;
  /*The DC scale used for out-of-loop deblocking.*/
  int                  pp_dc_scale[64];
  /*The sharpen modifier used for out-of-loop deringing.*/
  int                  pp_sharp_mod[64];
  /*The DC quantization index of each block.*/
  unsigned char       *dc_qis;
  /*The variance of each block.*/
  int                 *variances;
  /*The storage for the post-processed frame buffer.*/
  unsigned char       *pp_frame_data;
  /*Whether or not the post-processsed frame buffer has space for chroma.*/
  int                  pp_frame_has_chroma;
  /*The buffer used for the post-processed frame.*/
  th_ycbcr_buffer      pp_frame_buf;
  /*The striped decode callback function.*/
  th_stripe_callback   stripe_cb;
};

#endif

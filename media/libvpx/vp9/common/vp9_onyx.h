/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VP9_COMMON_VP9_ONYX_H_
#define VP9_COMMON_VP9_ONYX_H_

#include "./vpx_config.h"
#include "vpx/internal/vpx_codec_internal.h"
#include "vpx/vp8cx.h"
#include "vpx_scale/yv12config.h"
#include "vp9/common/vp9_ppflags.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_SEGMENTS 8

  typedef int *VP9_PTR;

  /* Create/destroy static data structures. */

  typedef enum {
    NORMAL      = 0,
    FOURFIVE    = 1,
    THREEFIVE   = 2,
    ONETWO      = 3
  } VPX_SCALING;

  typedef enum {
    VP9_LAST_FLAG = 1,
    VP9_GOLD_FLAG = 2,
    VP9_ALT_FLAG = 4
  } VP9_REFFRAME;


  typedef enum {
    USAGE_LOCAL_FILE_PLAYBACK   = 0x0,
    USAGE_STREAM_FROM_SERVER    = 0x1,
    USAGE_CONSTRAINED_QUALITY   = 0x2,
    USAGE_CONSTANT_QUALITY      = 0x3,
  } END_USAGE;


  typedef enum {
    MODE_GOODQUALITY    = 0x1,
    MODE_BESTQUALITY    = 0x2,
    MODE_FIRSTPASS      = 0x3,
    MODE_SECONDPASS     = 0x4,
    MODE_SECONDPASS_BEST = 0x5,
    MODE_REALTIME       = 0x6,
  } MODE;

  typedef enum {
    FRAMEFLAGS_KEY    = 1,
    FRAMEFLAGS_GOLDEN = 2,
    FRAMEFLAGS_ALTREF = 4,
  } FRAMETYPE_FLAGS;

  typedef enum {
    NO_AQ = 0,
    VARIANCE_AQ = 1,
    COMPLEXITY_AQ = 2,
    AQ_MODES_COUNT  // This should always be the last member of the enum
  } AQ_MODES;

  typedef struct {
    int version;  // 4 versions of bitstream defined:
                  //   0 - best quality/slowest decode,
                  //   3 - lowest quality/fastest decode
    int width;  // width of data passed to the compressor
    int height;  // height of data passed to the compressor
    double framerate;  // set to passed in framerate
    int64_t target_bandwidth;  // bandwidth to be used in kilobits per second

    int noise_sensitivity;  // pre processing blur: recommendation 0
    int sharpness;  // sharpening output: recommendation 0:
    int cpu_used;
    unsigned int rc_max_intra_bitrate_pct;

    // mode ->
    // (0)=Realtime/Live Encoding. This mode is optimized for realtime
    //     encoding (for example, capturing a television signal or feed from
    //     a live camera). ( speed setting controls how fast )
    // (1)=Good Quality Fast Encoding. The encoder balances quality with the
    //     amount of time it takes to encode the output. ( speed setting
    //     controls how fast )
    // (2)=One Pass - Best Quality. The encoder places priority on the
    //     quality of the output over encoding speed. The output is compressed
    //     at the highest possible quality. This option takes the longest
    //     amount of time to encode. ( speed setting ignored )
    // (3)=Two Pass - First Pass. The encoder generates a file of statistics
    //     for use in the second encoding pass. ( speed setting controls how
    //     fast )
    // (4)=Two Pass - Second Pass. The encoder uses the statistics that were
    //     generated in the first encoding pass to create the compressed
    //     output. ( speed setting controls how fast )
    // (5)=Two Pass - Second Pass Best.  The encoder uses the statistics that
    //     were generated in the first encoding pass to create the compressed
    //     output using the highest possible quality, and taking a
    //    longer amount of time to encode.. ( speed setting ignored )
    int mode;

    // Key Framing Operations
    int auto_key;  // autodetect cut scenes and set the keyframes
    int key_freq;  // maximum distance to key frame.

    int allow_lag;  // allow lagged compression (if 0 lagin frames is ignored)
    int lag_in_frames;  // how many frames lag before we start encoding

    // ----------------------------------------------------------------
    // DATARATE CONTROL OPTIONS

    int end_usage;  // vbr or cbr

    // buffer targeting aggressiveness
    int under_shoot_pct;
    int over_shoot_pct;

    // buffering parameters
    int64_t starting_buffer_level;  // in seconds
    int64_t optimal_buffer_level;
    int64_t maximum_buffer_size;

    // Frame drop threshold.
    int drop_frames_water_mark;

    // controlling quality
    int fixed_q;
    int worst_allowed_q;
    int best_allowed_q;
    int cq_level;
    int lossless;
    int aq_mode;  // Adaptive Quantization mode

    // two pass datarate control
    int two_pass_vbrbias;        // two pass datarate control tweaks
    int two_pass_vbrmin_section;
    int two_pass_vbrmax_section;
    // END DATARATE CONTROL OPTIONS
    // ----------------------------------------------------------------

    // Spatial and temporal scalability.
    int ss_number_layers;  // Number of spatial layers.
    int ts_number_layers;  // Number of temporal layers.
    // Bitrate allocation (CBR mode) and framerate factor, for temporal layers.
    int ts_target_bitrate[VPX_TS_MAX_LAYERS];
    int ts_rate_decimator[VPX_TS_MAX_LAYERS];

    // these parameters aren't to be used in final build don't use!!!
    int play_alternate;
    int alt_freq;

    int encode_breakout;  // early breakout : for video conf recommend 800

    /* Bitfield defining the error resiliency features to enable.
     * Can provide decodable frames after losses in previous
     * frames and decodable partitions after losses in the same frame.
     */
    unsigned int error_resilient_mode;

    /* Bitfield defining the parallel decoding mode where the
     * decoding in successive frames may be conducted in parallel
     * just by decoding the frame headers.
     */
    unsigned int frame_parallel_decoding_mode;

    int arnr_max_frames;
    int arnr_strength;
    int arnr_type;

    int tile_columns;
    int tile_rows;

    struct vpx_fixed_buf         two_pass_stats_in;
    struct vpx_codec_pkt_list  *output_pkt_list;

    vp8e_tuning tuning;
  } VP9_CONFIG;


  void vp9_initialize_enc();

  VP9_PTR vp9_create_compressor(VP9_CONFIG *oxcf);
  void vp9_remove_compressor(VP9_PTR *comp);

  void vp9_change_config(VP9_PTR onyx, VP9_CONFIG *oxcf);

  // receive a frames worth of data. caller can assume that a copy of this
  // frame is made and not just a copy of the pointer..
  int vp9_receive_raw_frame(VP9_PTR comp, unsigned int frame_flags,
                            YV12_BUFFER_CONFIG *sd, int64_t time_stamp,
                            int64_t end_time_stamp);

  int vp9_get_compressed_data(VP9_PTR comp, unsigned int *frame_flags,
                              size_t *size, uint8_t *dest,
                              int64_t *time_stamp, int64_t *time_end,
                              int flush);

  int vp9_get_preview_raw_frame(VP9_PTR comp, YV12_BUFFER_CONFIG *dest,
                                vp9_ppflags_t *flags);

  int vp9_use_as_reference(VP9_PTR comp, int ref_frame_flags);

  int vp9_update_reference(VP9_PTR comp, int ref_frame_flags);

  int vp9_copy_reference_enc(VP9_PTR comp, VP9_REFFRAME ref_frame_flag,
                             YV12_BUFFER_CONFIG *sd);

  int vp9_get_reference_enc(VP9_PTR ptr, int index, YV12_BUFFER_CONFIG **fb);

  int vp9_set_reference_enc(VP9_PTR comp, VP9_REFFRAME ref_frame_flag,
                            YV12_BUFFER_CONFIG *sd);

  int vp9_update_entropy(VP9_PTR comp, int update);

  int vp9_set_roimap(VP9_PTR comp, unsigned char *map,
                     unsigned int rows, unsigned int cols,
                     int delta_q[MAX_SEGMENTS],
                     int delta_lf[MAX_SEGMENTS],
                     unsigned int threshold[MAX_SEGMENTS]);

  int vp9_set_active_map(VP9_PTR comp, unsigned char *map,
                         unsigned int rows, unsigned int cols);

  int vp9_set_internal_size(VP9_PTR comp,
                            VPX_SCALING horiz_mode, VPX_SCALING vert_mode);

  int vp9_set_size_literal(VP9_PTR comp, unsigned int width,
                           unsigned int height);

  void vp9_set_svc(VP9_PTR comp, int use_svc);

  int vp9_get_quantizer(VP9_PTR c);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // VP9_COMMON_VP9_ONYX_H_

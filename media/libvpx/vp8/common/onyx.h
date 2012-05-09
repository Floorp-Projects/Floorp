/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#ifndef __INC_VP8_H
#define __INC_VP8_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "vpx_config.h"
#include "vpx/internal/vpx_codec_internal.h"
#include "vpx/vp8cx.h"
#include "vpx/vpx_encoder.h"
#include "vpx_scale/yv12config.h"
#include "ppflags.h"

    struct VP8_COMP;

    /* Create/destroy static data structures. */

    typedef enum
    {
        NORMAL      = 0,
        FOURFIVE    = 1,
        THREEFIVE   = 2,
        ONETWO      = 3

    } VPX_SCALING;

    typedef enum
    {
        VP8_LAST_FLAG = 1,
        VP8_GOLD_FLAG = 2,
        VP8_ALT_FLAG = 4
    } VP8_REFFRAME;


    typedef enum
    {
        USAGE_STREAM_FROM_SERVER    = 0x0,
        USAGE_LOCAL_FILE_PLAYBACK   = 0x1,
        USAGE_CONSTRAINED_QUALITY   = 0x2
    } END_USAGE;


    typedef enum
    {
        MODE_REALTIME       = 0x0,
        MODE_GOODQUALITY    = 0x1,
        MODE_BESTQUALITY    = 0x2,
        MODE_FIRSTPASS      = 0x3,
        MODE_SECONDPASS     = 0x4,
        MODE_SECONDPASS_BEST = 0x5
    } MODE;

    typedef enum
    {
        FRAMEFLAGS_KEY    = 1,
        FRAMEFLAGS_GOLDEN = 2,
        FRAMEFLAGS_ALTREF = 4
    } FRAMETYPE_FLAGS;


#include <assert.h>
    static void Scale2Ratio(int mode, int *hr, int *hs)
    {
        switch (mode)
        {
        case    NORMAL:
            *hr = 1;
            *hs = 1;
            break;
        case    FOURFIVE:
            *hr = 4;
            *hs = 5;
            break;
        case    THREEFIVE:
            *hr = 3;
            *hs = 5;
            break;
        case    ONETWO:
            *hr = 1;
            *hs = 2;
            break;
        default:
            *hr = 1;
            *hs = 1;
            assert(0);
            break;
        }
    }

    typedef struct
    {
        int Version;            // 4 versions of bitstream defined 0 best quality/slowest decode, 3 lowest quality/fastest decode
        int Width;              // width of data passed to the compressor
        int Height;             // height of data passed to the compressor
        struct vpx_rational  timebase;
        int target_bandwidth;    // bandwidth to be used in kilobits per second

        int noise_sensitivity;   // parameter used for applying pre processing blur: recommendation 0
        int Sharpness;          // parameter used for sharpening output: recommendation 0:
        int cpu_used;
        unsigned int rc_max_intra_bitrate_pct;

        // mode ->
        //(0)=Realtime/Live Encoding. This mode is optimized for realtim encoding (for example, capturing
        //    a television signal or feed from a live camera). ( speed setting controls how fast )
        //(1)=Good Quality Fast Encoding. The encoder balances quality with the amount of time it takes to
        //    encode the output. ( speed setting controls how fast )
        //(2)=One Pass - Best Quality. The encoder places priority on the quality of the output over encoding
        //    speed. The output is compressed at the highest possible quality. This option takes the longest
        //    amount of time to encode. ( speed setting ignored )
        //(3)=Two Pass - First Pass. The encoder generates a file of statistics for use in the second encoding
        //    pass. ( speed setting controls how fast )
        //(4)=Two Pass - Second Pass. The encoder uses the statistics that were generated in the first encoding
        //    pass to create the compressed output. ( speed setting controls how fast )
        //(5)=Two Pass - Second Pass Best.  The encoder uses the statistics that were generated in the first
        //    encoding pass to create the compressed output using the highest possible quality, and taking a
        //    longer amount of time to encode.. ( speed setting ignored )
        int Mode;               //

        // Key Framing Operations
        int auto_key;            // automatically detect cut scenes and set the keyframes
        int key_freq;            // maximum distance to key frame.

        int allow_lag;           // allow lagged compression (if 0 lagin frames is ignored)
        int lag_in_frames;        // how many frames lag before we start encoding

        //----------------------------------------------------------------
        // DATARATE CONTROL OPTIONS

        int end_usage; // vbr or cbr

        // buffer targeting aggressiveness
        int under_shoot_pct;
        int over_shoot_pct;

        // buffering parameters
        int64_t starting_buffer_level;  // in bytes
        int64_t optimal_buffer_level;
        int64_t maximum_buffer_size;

        int64_t starting_buffer_level_in_ms;  // in milli-seconds
        int64_t optimal_buffer_level_in_ms;
        int64_t maximum_buffer_size_in_ms;

        // controlling quality
        int fixed_q;
        int worst_allowed_q;
        int best_allowed_q;
        int cq_level;

        // allow internal resizing ( currently disabled in the build !!!!!)
        int allow_spatial_resampling;
        int resample_down_water_mark;
        int resample_up_water_mark;

        // allow internal frame rate alterations
        int allow_df;
        int drop_frames_water_mark;

        // two pass datarate control
        int two_pass_vbrbias;        // two pass datarate control tweaks
        int two_pass_vbrmin_section;
        int two_pass_vbrmax_section;
        // END DATARATE CONTROL OPTIONS
        //----------------------------------------------------------------


        // these parameters aren't to be used in final build don't use!!!
        int play_alternate;
        int alt_freq;
        int alt_q;
        int key_q;
        int gold_q;


        int multi_threaded;   // how many threads to run the encoder on
        int token_partitions; // how many token partitions to create for multi core decoding
        int encode_breakout;  // early breakout encode threshold : for video conf recommend 800

        unsigned int error_resilient_mode; // Bitfield defining the error
                                   // resiliency features to enable. Can provide
                                   // decodable frames after losses in previous
                                   // frames and decodable partitions after
                                   // losses in the same frame.

        int arnr_max_frames;
        int arnr_strength ;
        int arnr_type     ;

        struct vpx_fixed_buf         two_pass_stats_in;
        struct vpx_codec_pkt_list  *output_pkt_list;

        vp8e_tuning tuning;

        // Temporal scaling parameters
        unsigned int number_of_layers;
        unsigned int target_bitrate[MAX_PERIODICITY];
        unsigned int rate_decimator[MAX_PERIODICITY];
        unsigned int periodicity;
        unsigned int layer_id[MAX_PERIODICITY];

#if CONFIG_MULTI_RES_ENCODING
        /* Number of total resolutions encoded */
        unsigned int mr_total_resolutions;

        /* Current encoder ID */
        unsigned int mr_encoder_id;

        /* Down-sampling factor */
        vpx_rational_t mr_down_sampling_factor;

        /* Memory location to store low-resolution encoder's mode info */
        void* mr_low_res_mode_info;
#endif
    } VP8_CONFIG;


    void vp8_initialize();

    struct VP8_COMP* vp8_create_compressor(VP8_CONFIG *oxcf);
    void vp8_remove_compressor(struct VP8_COMP* *comp);

    void vp8_init_config(struct VP8_COMP* onyx, VP8_CONFIG *oxcf);
    void vp8_change_config(struct VP8_COMP* onyx, VP8_CONFIG *oxcf);

// receive a frames worth of data caller can assume that a copy of this frame is made
// and not just a copy of the pointer..
    int vp8_receive_raw_frame(struct VP8_COMP* comp, unsigned int frame_flags, YV12_BUFFER_CONFIG *sd, int64_t time_stamp, int64_t end_time_stamp);
    int vp8_get_compressed_data(struct VP8_COMP* comp, unsigned int *frame_flags, unsigned long *size, unsigned char *dest, unsigned char *dest_end, int64_t *time_stamp, int64_t *time_end, int flush);
    int vp8_get_preview_raw_frame(struct VP8_COMP* comp, YV12_BUFFER_CONFIG *dest, vp8_ppflags_t *flags);

    int vp8_use_as_reference(struct VP8_COMP* comp, int ref_frame_flags);
    int vp8_update_reference(struct VP8_COMP* comp, int ref_frame_flags);
    int vp8_get_reference(struct VP8_COMP* comp, VP8_REFFRAME ref_frame_flag, YV12_BUFFER_CONFIG *sd);
    int vp8_set_reference(struct VP8_COMP* comp, VP8_REFFRAME ref_frame_flag, YV12_BUFFER_CONFIG *sd);
    int vp8_update_entropy(struct VP8_COMP* comp, int update);
    int vp8_set_roimap(struct VP8_COMP* comp, unsigned char *map, unsigned int rows, unsigned int cols, int delta_q[4], int delta_lf[4], unsigned int threshold[4]);
    int vp8_set_active_map(struct VP8_COMP* comp, unsigned char *map, unsigned int rows, unsigned int cols);
    int vp8_set_internal_size(struct VP8_COMP* comp, VPX_SCALING horiz_mode, VPX_SCALING vert_mode);
    int vp8_get_quantizer(struct VP8_COMP* c);

#ifdef __cplusplus
}
#endif

#endif

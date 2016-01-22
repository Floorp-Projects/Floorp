/*
 * Copyright © 2014 Mozilla Foundation
 *
 * This program is made available under an ISC-style license.  See the
 * accompanying file LICENSE for details.
 */
#ifndef CUBEB_RESAMPLER_H
#define CUBEB_RESAMPLER_H

#include "cubeb/cubeb.h"

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct cubeb_resampler cubeb_resampler;

typedef enum {
  CUBEB_RESAMPLER_QUALITY_VOIP,
  CUBEB_RESAMPLER_QUALITY_DEFAULT,
  CUBEB_RESAMPLER_QUALITY_DESKTOP
} cubeb_resampler_quality;

/**
 * Create a resampler to adapt the requested sample rate into something that
 * is accepted by the audio backend.
 * @param stream A cubeb_stream instance supplied to the data callback.
 * @param params Used to calculate bytes per frame and buffer size for resampling.
 * @param out_rate The sampling rate after resampling.
 * @param callback A callback to request data for resampling.
 * @param buffer_frame_count Maximum number of frames passed to cubeb_resampler_fill
 *                           as |frames_needed|. This is also used to calculate
 *                           the size of buffer allocated for resampling.
 * @param user_ptr User data supplied to the data callback.
 * @param quality Quality of the resampler.
 * @retval A non-null pointer if success.
 */
cubeb_resampler * cubeb_resampler_create(cubeb_stream * stream,
                                         cubeb_stream_params params,
                                         unsigned int out_rate,
                                         cubeb_data_callback callback,
                                         long buffer_frame_count,
                                         void * user_ptr,
                                         cubeb_resampler_quality quality);

/**
 * Fill the buffer with frames acquired using the data callback. Resampling will
 * happen if necessary.
 * @param resampler A cubeb_resampler instance.
 * @param buffer The buffer to be filled.
 * @param frames_needed Number of frames that should be produced.
 * @retval Number of frames that are actually produced.
 * @retval CUBEB_ERROR on error.
 */
long cubeb_resampler_fill(cubeb_resampler * resampler,
                          void * buffer, long frames_needed);

/**
 * Destroy a cubeb_resampler.
 * @param resampler A cubeb_resampler instance.
 */
void cubeb_resampler_destroy(cubeb_resampler * resampler);

#if defined(__cplusplus)
}
#endif

#endif /* CUBEB_RESAMPLER_H */

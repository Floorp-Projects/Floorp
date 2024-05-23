/*
 * Copyright © 2023 Mozilla Foundation
 *
 * This program is made available under an ISC-style license.  See the
 * accompanying file LICENSE for details.
 */

#ifndef CUBEB_AUDIO_DUMP
#define CUBEB_AUDIO_DUMP

#include "cubeb/cubeb.h"

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct cubeb_audio_dump_stream * cubeb_audio_dump_stream_t;
typedef struct cubeb_audio_dump_session * cubeb_audio_dump_session_t;

// Start audio dumping session
// This can only be called if the other API functions
// aren't currently being called: synchronized externally.
// This is not real-time safe.
//
// This is generally called when deciding to start logging some audio.
//
// Returns 0 in case of success.
int
cubeb_audio_dump_init(cubeb_audio_dump_session_t * session);

// End audio dumping session
// This can only be called if the other API functions
// aren't currently being called: synchronized externally.
//
// This is generally called when deciding to stop logging some audio.
//
// This is not real-time safe.
// Returns 0 in case of success.
int
cubeb_audio_dump_shutdown(cubeb_audio_dump_session_t session);

// Register a stream for dumping to a file
// This can only be called if cubeb_audio_dump_write
// isn't currently being called: synchronized externally.
//
// This is generally called when setting up a system-level stream side (either
// input or output).
//
// This is not real-time safe.
// Returns 0 in case of success.
int
cubeb_audio_dump_stream_init(cubeb_audio_dump_session_t session,
                             cubeb_audio_dump_stream_t * stream,
                             cubeb_stream_params stream_params,
                             const char * name);

// Unregister a stream for dumping to a file
// This can only be called if cubeb_audio_dump_write
// isn't currently being called: synchronized externally.
//
// This is generally called when a system-level audio stream side
// (input/output) has been stopped and drained, and the audio callback isn't
// going to be called.
//
// This is not real-time safe.
// Returns 0 in case of success.
int
cubeb_audio_dump_stream_shutdown(cubeb_audio_dump_session_t session,
                                 cubeb_audio_dump_stream_t stream);

// Start dumping.
// cubeb_audio_dump_write can now be called.
//
// This starts dumping the audio to disk. Generally this is called when
// cubeb_stream_start is caled is called, but can be called at the beginning of
// the application.
//
// This is not real-time safe.
// Returns 0 in case of success.
int
cubeb_audio_dump_start(cubeb_audio_dump_session_t session);

// Stop dumping.
// cubeb_audio_dump_write can't be called at this point.
//
// This stops dumping the audio to disk cubeb_stream_stop is caled is called,
// but can be called before exiting the application.
//
// This is not real-time safe.
// Returns 0 in case of success.
int
cubeb_audio_dump_stop(cubeb_audio_dump_session_t session);

// Dump some audio samples for audio stream id.
//
// This is generally called from the real-time audio callback.
//
// This is real-time safe.
// Returns 0 in case of success.
int
cubeb_audio_dump_write(cubeb_audio_dump_stream_t stream, void * audio_samples,
                       uint32_t count);

#ifdef __cplusplus
};
#endif

#endif

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef AUDIO_THREAD_PRIORITY_H
#define AUDIO_THREAD_PRIORITY_H

#include <stdint.h>
#include <stdlib.h>

/**
 * An opaque structure containing information about a thread that was promoted
 * to real-time priority.
 */
struct atp_handle;

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

/**
 * Promotes the current thread to real-time priority.
 *
 * audio_buffer_frames: number of frames per audio buffer. If unknown, passing 0
 * will choose an appropriate number, conservatively. If variable, either pass 0
 * or an upper bound.
 * audio_samplerate_hz: sample-rate for this audio stream, in Hz
 *
 * Returns an opaque handle in case of success, NULL otherwise.
 */
atp_handle *atp_promote_current_thread_to_real_time(uint32_t audio_buffer_frames,
                                                    uint32_t audio_samplerate_hz);

/**
 * Demotes the current thread promoted to real-time priority via
 * `atp_demote_current_thread_from_real_time` to its previous priority.
 *
 * Returns 0 in case of success, non-zero otherwise.
 */
int32_t atp_demote_current_thread_from_real_time(atp_handle *handle);

/**
 * Frees an atp_handle. This is useful when it impractical to call
 *`atp_demote_current_thread_from_real_time` on the right thread. Access to the
 * handle must be synchronized externaly (or the related thread must have
 * exited).
 *
 * Returns 0 in case of success, non-zero otherwise.
 */
int32_t atp_free_handle(atp_handle *handle);

#ifdef __cplusplus
} // extern "C"
#endif // __cplusplus

#endif // AUDIO_THREAD_PRIORITY_H

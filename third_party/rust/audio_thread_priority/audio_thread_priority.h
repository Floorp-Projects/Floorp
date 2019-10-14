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
struct atp_thread_info;
extern size_t ATP_THREAD_INFO_SIZE;

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

/*
 * Linux-only API.
 *
 * The Linux backend uses DBUS to promote a thread to real-time priority. In
 * environment where this is not possible (due to sandboxing), this set of
 * functions allow remoting the call to a process that can make DBUS calls.
 *
 * To do so:
 * - Set the real-time limit from within the process where a
 *   thread will be promoted. This is a `setrlimit` call, that can be done
 *   before the sandbox lockdown.
 * - Then, gather information on the thread that will be promoted.
 * - Serialize this info.
 * - Send over the serialized data via an IPC mechanism
 * - Deserialize the inf
 * - Call `atp_promote_thread_to_real_time`
 */

#ifdef __linux__
/**
 * Promotes a thread, possibly in another process, to real-time priority.
 *
 * thread_info: info on the thread to promote, gathered with
 * `atp_get_current_thread_info()`, called on the thread itself.
 * audio_buffer_frames: number of frames per audio buffer. If unknown, passing 0
 * will choose an appropriate number, conservatively. If variable, either pass 0
 * or an upper bound.
 * audio_samplerate_hz: sample-rate for this audio stream, in Hz
 *
 * Returns an opaque handle in case of success, NULL otherwise.
 *
 * This call is useful on Linux desktop only, when the process is sandboxed and
 * cannot promote itself directly.
 */
atp_handle *atp_promote_thread_to_real_time(atp_thread_info *thread_info);

/**
 * Demotes a thread promoted to real-time priority via
 * `atp_demote_thread_from_real_time` to its previous priority.
 *
 * Returns 0 in case of success, non-zero otherwise.
 *
 * This call is useful on Linux desktop only, when the process is sandboxed and
 * cannot promote itself directly.
 */
int32_t atp_demote_thread_from_real_time(atp_thread_info* thread_info);

/**
 * Gather informations from the calling thread, to be able to promote it from
 * another thread and/or process.
 *
 * Returns a non-null pointer to an `atp_thread_info` structure in case of
 * sucess, to be freed later with `atp_free_thread_info`, and NULL otherwise.
 *
 * This call is useful on Linux desktop only, when the process is sandboxed and
 * cannot promote itself directly.
 */
atp_thread_info *atp_get_current_thread_info();

/**
 * Free an `atp_thread_info` structure.
 *
 * Returns 0 in case of success, non-zero in case of error (because thread_info
 * was NULL).
 */
int32_t atp_free_thread_info(atp_thread_info *thread_info);

/**
 * Serialize an `atp_thread_info` to a byte buffer that is
 * sizeof(atp_thread_info) long.
 */
void atp_serialize_thread_info(atp_thread_info *thread_info, uint8_t *bytes);

/**
 * Deserialize a byte buffer of sizeof(atp_thread_info) to an `atp_thread_info`
 * pointer. It can be then freed using atp_free_thread_info.
 * */
atp_thread_info* atp_deserialize_thread_info(uint8_t *bytes);

/**
 * Set real-time limit for the calling process.
 *
 * This is useful only on Linux desktop, and allows remoting the rtkit DBUS call
 * to a process that has access to DBUS. This function has to be called before
 * attempting to promote threads from another process.
 *
 * This sets the real-time computation limit. For actually promoting the thread
 * to a real-time scheduling class, see `atp_promote_thread_to_real_time`.
 */
int32_t atp_set_real_time_limit(uint32_t audio_buffer_frames,
                                uint32_t audio_samplerate_hz);

#endif // __linux__

#ifdef __cplusplus
} // extern "C"
#endif // __cplusplus

#endif // AUDIO_THREAD_PRIORITY_H

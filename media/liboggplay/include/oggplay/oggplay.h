/*
   Copyright (C) 2003 Commonwealth Scientific and Industrial Research
   Organisation (CSIRO) Australia

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:

   - Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.

   - Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.

   - Neither the name of CSIRO Australia nor the names of its
   contributors may be used to endorse or promote products derived from
   this software without specific prior written permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
   PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE ORGANISATION OR
   CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
   EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
   PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
   PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
   LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
   NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

/** @file
 * 
 * The liboggplay C API.
 *
 * @authors
 * Shane Stephens <shane.stephens@annodex.net>
 * Michael Martin
 * Viktor Gal
 */
 
#ifndef __OGGPLAY_H__
#define __OGGPLAY_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <oggplay/oggplay_enums.h>
#include <oggplay/oggplay_reader.h>
 
/**
 * This is returned by oggplay_open_with_reader() or oggplay_new_with_reader().
 */
typedef struct _OggPlay OggPlay;

/**
 * A structure for storing the decoded frames for the various streams in the 
 * Ogg container.
 */
typedef struct _OggPlayCallbackInfo OggPlayCallbackInfo;

/**
 * This is the signature of a callback which you must provide for OggPlay
 * to call whenever there's any unpresented decoded frame available.
 * 
 * @see oggplay_step_decoding
 * @param player The OggPlay handle
 * @param num_records size of the OggPlayCallbackInfo array
 * @param records array of OggPlayCallbackInfo
 * @param user A generic pointer for the data the user provided earlier.
 * @returns 0 to continue, non-zero to instruct OggPlay to stop.
 *
 */
typedef int (OggPlayDataCallback) (OggPlay *player, int num_records, 
                                   OggPlayCallbackInfo **records, void *user);

#include <oggplay/oggplay_query.h>
#include <oggplay/oggplay_callback_info.h>
#include <oggplay/oggplay_tools.h>
#include <oggplay/oggplay_seek.h>

/**
 * Create an OggPlay handle associated with the given reader.
 * 
 * This functions creates a new OggPlay handle associated with 
 * the OggPlayReader and it calls oggplay_initialise to 
 * read the header packets of the Ogg container. 
 *
 * @param reader an OggPlayReader handle associated with the Ogg content
 * @return A new OggPlay handle
 * @retval NULL in case of error.
 */
OggPlay *
oggplay_open_with_reader(OggPlayReader *reader);

/**
 * Create a new OggPlay handle associated with the given reader.
 *
 * @param reader OggPlayReader handle associated with the Ogg content
 * @return A new OggPlay handle
 * @retval NULL in case of error.
 */
OggPlay *
oggplay_new_with_reader(OggPlayReader *reader);


/**
 * Initialise the OggPlay handle.
 * 
 * This function creates an Oggz handle and sets it's OggzIO*
 * functions to the OggPlayReader's io_* functions. Moreover
 * it reads the Ogg container's content until it hasn't got 
 * all the streams' headers.
 *
 * @param me OggPlay handle
 * @param block passed as the second argument to the OggPlayReader's initialise 
 * function. E.g. in case of OggPlayTCPReader block == 0 sets the socket to non-blocking
 * mode.
 * @retval E_OGGPLAY_OK on success
 * @retval E_OGGPLAY_OGGZ_UNHAPPY something went wrong while calling oggz_io_set_* functions.
 * @retval E_OGGPLAY_BAD_INPUT got EOF or OGGZ_ERR_HOLE_IN_DATA occured.
 * @retval E_OGGPLAY_OUT_OF_MEMORY ran out of memory or video frame too large.
 * @retval E_OGGPLAY_BAD_OGGPLAY invalid OggPlay handle. 
 */
OggPlayErrorCode
oggplay_initialise(OggPlay *me, int block);

/**
 * Sets a user defined OggPlayDataCallback function for the OggPlay handle.
 *
 * @param me OggPlay handle.
 * @param callback A custom callback function.
 * @param user Arbitrary data one wishes to pass to the callback function.
 * @retval E_OGGPLAY_OK on success 
 * @retval E_OGGPLAY_BUFFER_MODE We are running in buffer mode, i.e. oggplay_use_buffer 
 * has been called earlier.
 * @retval E_OGGPLAY_BAD_OGGPLAY Invalid OggPlay handle.
 */
OggPlayErrorCode
oggplay_set_data_callback(OggPlay *me, OggPlayDataCallback callback, 
                          void *user);


OggPlayErrorCode
oggplay_set_callback_num_frames(OggPlay *me, int stream, int frames);

OggPlayErrorCode
oggplay_set_callback_period(OggPlay *me, int stream, int milliseconds);

OggPlayErrorCode
oggplay_set_offset(OggPlay *me, int track, ogg_int64_t offset);

/**
 * Get the given video track's Y-plane's width and height.
 * 
 * @param me OggPlay handle
 * @param track the track number of the video track
 * @param y_width the width of the Y-plane
 * @param y_height the height of the Y-plane
 * @retval E_OGGPLAY_OK on success.
 * @retval E_OGGPLAY_BAD_OGGPLAY invalid OggPlay handle
 * @retval E_OGGPLAY_BAD_TRACK the given track number does not exists
 * @retval E_OGGPLAY_WRONG_TRACK_TYPE the given track is not an audio track
 * @retval E_OGGPLAY_UNINITIALISED the OggPlay handle is uninitalised.
 */
OggPlayErrorCode
oggplay_get_video_y_size(OggPlay *me, int track, int *y_width, int *y_height);

/**
 * Get the given video track's UV-plane's width and height.
 * 
 * @param me OggPlay handle
 * @param track the track number of the video track
 * @param uv_width the width of the UV-plane
 * @param uv_height the height of the UV-plane
 * @retval E_OGGPLAY_OK on success.
 * @retval E_OGGPLAY_BAD_OGGPLAY invalid OggPlay handle
 * @retval E_OGGPLAY_BAD_TRACK the given track number does not exists
 * @retval E_OGGPLAY_WRONG_TRACK_TYPE the given track is not an audio track
 * @retval E_OGGPLAY_UNINITIALISED the OggPlay handle is uninitalised.
 */
OggPlayErrorCode
oggplay_get_video_uv_size(OggPlay *me, int track, int *uv_width, int *uv_height);

/**
 * Get the number of channels of the audio track.
 * 
 * @param me OggPlay handle
 * @param track the track number of the audio track
 * @param channels the number of channels of the given audio track.
 * @retval E_OGGPLAY_OK on success.
 * @retval E_OGGPLAY_BAD_OGGPLAY invalid OggPlay handle
 * @retval E_OGGPLAY_BAD_TRACK the given track number does not exists
 * @retval E_OGGPLAY_WRONG_TRACK_TYPE the given track is not an audio track
 * @retval E_OGGPLAY_UNINITIALISED the OggPlay handle is uninitalised.
 */
OggPlayErrorCode
oggplay_get_audio_channels(OggPlay *me, int track, int *channels);

/**
 * Get the sample rate of the of the audio track
 * 
 * @param me OggPlay handle
 * @param track the track number of the audio track
 * @param samplerate the sample rate of the audio track.
 * @retval E_OGGPLAY_OK on success.
 * @retval E_OGGPLAY_BAD_OGGPLAY invalid OggPlay handle
 * @retval E_OGGPLAY_BAD_TRACK the given track number does not exists
 * @retval E_OGGPLAY_WRONG_TRACK_TYPE the given track is not an audio track
 * @retval E_OGGPLAY_UNINITIALISED the OggPlay handle is uninitalised.
 */
OggPlayErrorCode
oggplay_get_audio_samplerate(OggPlay *me, int track, int *samplerate); 

/**
 * Get the frame-per-second value the of a given video track.
 * 
 * @param me OggPlay handle
 * @param track the track number of the audio track
 * @param fps_denom the denumerator of the FPS
 * @param fps_num the numerator of the FPS
 * @retval E_OGGPLAY_OK on success.
 * @retval E_OGGPLAY_BAD_OGGPLAY invalid OggPlay handle
 * @retval E_OGGPLAY_BAD_TRACK the given track number does not exists
 * @retval E_OGGPLAY_WRONG_TRACK_TYPE the given track is not an audio track
 * @retval E_OGGPLAY_UNINITIALISED the OggPlay handle is uninitalised.
 */
OggPlayErrorCode
oggplay_get_video_fps(OggPlay *me, int track, int* fps_denom, int* fps_num);

OggPlayErrorCode
oggplay_get_video_aspect_ratio(OggPlay *me, int track, int* aspect_denom, int* aspect_num);

OggPlayErrorCode
oggplay_convert_video_to_rgb(OggPlay *me, int track, int convert, int swap_rgb);

OggPlayErrorCode
oggplay_get_kate_category(OggPlay *me, int track, const char** category);

OggPlayErrorCode
oggplay_get_kate_language(OggPlay *me, int track, const char** language);

OggPlayErrorCode
oggplay_set_kate_tiger_rendering(OggPlay *me, int track, int use_tiger, int swap_rgb, int default_width, int default_height);

OggPlayErrorCode
oggplay_overlay_kate_track_on_video(OggPlay *me, int kate_track, int video_track);

OggPlayErrorCode
oggplay_start_decoding(OggPlay *me);

/**
 * Decode the streams in the Ogg container until we find data that hasn't
 * been presented, yet.
 * 
 * Whenever there is data that hasn't been presented the OggPlayDataCallback
 * function will be called.
 * 
 * @param me OggPlay handle
 * @retval E_OGGPLAY_OK reached the end of the stream or shutdown detected
 * @retval E_OGGPLAY_CONTINUE successfully decoded the frames.
 * @retval E_OGGPLAY_BAD_INPUT OGGZ_ERR_HOLE_IN_DATA occured while decoding
 * @retval E_OGGPLAY_UNINITIALISED the OggPlayDataCallback of the OggPlay handle is not set.
 * @retval E_OGGPLAY_USER_INTERRUPT user interrupted the decoding
 * @retval E_OGGPLAY_OUT_OF_MEMORY ran out of memory while decoding
 */
OggPlayErrorCode
oggplay_step_decoding(OggPlay *me);

/**
 * Use the built-in OggPlayBuffer for buffering the decoded frames
 * 
 * The built-in OggPlayBuffer implements a simple lock-free FIFO for 
 * storing the decoded frames.
 * 
 * It tries to set the OggPlay handle's OggPlayDataCallback function to it's
 * own implementation (oggplay_buffer_callback). Thus it will fail if
 * a user already set OggPlayDataCallback of the OggPlay handle
 * with oggplay_set_data_callback.
 * 
 * One can retrive the next record in the queue by 
 * calling oggplay_buffer_retrieve_next.
 * 
 * @param player the OggPlay handle
 * @param size The size of the buffer, i.e. the number of records it can store
 * @retval E_OGGPLAY_OK on succsess
 * @retval E_OGGPLAY_BAD_OGGPLAY The supplied OggPlay handle is not valid
 * @retval E_OGGPLAY_CALLBACK_MODE The given OggPlay handle's OggPlayDataCallback 
 * function is already set, i.e. running in callback-mode.
 * @retval E_OGGPLAY_OUT_OF_MEMORY Ran out of memory while trying to allocate memory for the buffer.
 */
OggPlayErrorCode
oggplay_use_buffer(OggPlay *player, int size);

/**
 * Retrive the next element in the buffer.
 *
 * @param player OggPlay handle
 * @return array of OggPlayCallbackInfo - one per track.
 * @retval NULL if there was no available.
 */
OggPlayCallbackInfo **
oggplay_buffer_retrieve_next(OggPlay *player);

/**
 * Release the given buffer item.
 *
 * After retrieving and processing one buffer item, in order
 * to remove the given item from the queue and release the 
 * memory allocated by the buffer item one needs to call this 
 * function.
 * 
 * @param player OggPlay handle
 * @param track_info OggPlayCallbackInfo array to release.
 * @retval E_OGGPLAY_OK on success.
 * @retval E_OGGPLAY_BAD_OGGPLAY invalid OggPlay handle.
 */
OggPlayErrorCode
oggplay_buffer_release(OggPlay *player, OggPlayCallbackInfo **track_info);

void
oggplay_prepare_for_close(OggPlay *me);

/**
 * Destroys the OggPlay handle along with the associated OggPlayReader
 * and clears out the buffer and shuts down the callback function. 
 *
 * @param player an OggPlay handle
 * @retval E_OGGPLAY_OK on success 
 */
OggPlayErrorCode
oggplay_close(OggPlay *player);

int
oggplay_get_available(OggPlay *player);

/**
 * Get the duration of the Ogg content.
 * 
 * @param player OggPlay handle.
 * @return The duration of the content in milliseconds.
 * @retval E_OGGPLAY_BAD_OGGPLAY invalid OggPlay handle
 */
ogg_int64_t
oggplay_get_duration(OggPlay * player);

int
oggplay_media_finished_retrieving(OggPlay * player);

/**
 * Sets the maximum video frame size, in pixels, which OggPlay will attempt to
 * decode. Call this after oggplay_new_with_reader() but before
 * oggplay_initialise() to prevent crashes with excessivly large video frame
 * sizes. oggplay_initialise() will return E_OGGPLAY_OUT_OF_MEMORY if the
 * decoded video's frame requires more than max_frame_pixels. Unless
 * oggplay_set_max_video_size() is called, default maximum number of pixels
 * per frame is INT_MAX.
 *
 * @param player OggPlay handle.
 * @param max_frame_pixels max number of pixels per frame.
 * @retval E_OGGPLAY_OK on success.
 * @retval E_OGGPLAY_BAD_OGGPLAY invalid OggPlay handle.
 */
int
oggplay_set_max_video_frame_pixels(OggPlay *player,
                                   int max_frame_pixels);

#ifdef __cplusplus
}
#endif

#endif /* __OGGPLAY_H__ */

/* Copyright (c) 2010-2011 Xiph.Org Foundation, Skype Limited
   Written by Jean-Marc Valin and Koen Vos */
/*
   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:

   - Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.

   - Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.

   - Neither the name of Internet Society, IETF or IETF Trust, nor the
   names of specific contributors, may be used to endorse or promote
   products derived from this software without specific prior written
   permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER
   OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
   EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
   PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
   PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
   LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
   NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

/**
 * @file opus_defines.h
 * @brief Opus reference implementation constants
 */

#ifndef OPUS_DEFINES_H
#define OPUS_DEFINES_H

#include "opus_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @defgroup errorcodes Error codes
 * @{
 */
/** No error @hideinitializer*/
#define OPUS_OK                0
/** One or more invalid/out of range arguments @hideinitializer*/
#define OPUS_BAD_ARG          -1
/** The mode struct passed is invalid @hideinitializer*/
#define OPUS_BUFFER_TOO_SMALL -2
/** An internal error was detected @hideinitializer*/
#define OPUS_INTERNAL_ERROR   -3
/** The compressed data passed is corrupted @hideinitializer*/
#define OPUS_INVALID_PACKET   -4
/** Invalid/unsupported request number @hideinitializer*/
#define OPUS_UNIMPLEMENTED    -5
/** An encoder or decoder structure is invalid or already freed @hideinitializer*/
#define OPUS_INVALID_STATE    -6
/** Memory allocation has failed @hideinitializer*/
#define OPUS_ALLOC_FAIL       -7
/**@}*/

/** @cond OPUS_INTERNAL_DOC */
/**Export control for opus functions */

#if defined(__GNUC__) && defined(OPUS_BUILD)
# define OPUS_EXPORT __attribute__ ((visibility ("default")))
#elif defined(WIN32)
# ifdef OPUS_BUILD
#   define OPUS_EXPORT __declspec(dllexport)
# else
#   define OPUS_EXPORT __declspec(dllimport)
# endif
#else
# define OPUS_EXPORT
#endif

/** These are the actual Encoder CTL ID numbers.
  * They should not be used directly by applications. */
#define OPUS_SET_APPLICATION_REQUEST         4000
#define OPUS_GET_APPLICATION_REQUEST         4001
#define OPUS_SET_BITRATE_REQUEST             4002
#define OPUS_GET_BITRATE_REQUEST             4003
#define OPUS_SET_MAX_BANDWIDTH_REQUEST       4004
#define OPUS_GET_MAX_BANDWIDTH_REQUEST       4005
#define OPUS_SET_VBR_REQUEST                 4006
#define OPUS_GET_VBR_REQUEST                 4007
#define OPUS_SET_BANDWIDTH_REQUEST           4008
#define OPUS_GET_BANDWIDTH_REQUEST           4009
#define OPUS_SET_COMPLEXITY_REQUEST          4010
#define OPUS_GET_COMPLEXITY_REQUEST          4011
#define OPUS_SET_INBAND_FEC_REQUEST          4012
#define OPUS_GET_INBAND_FEC_REQUEST          4013
#define OPUS_SET_PACKET_LOSS_PERC_REQUEST    4014
#define OPUS_GET_PACKET_LOSS_PERC_REQUEST    4015
#define OPUS_SET_DTX_REQUEST                 4016
#define OPUS_GET_DTX_REQUEST                 4017
#define OPUS_SET_VBR_CONSTRAINT_REQUEST      4020
#define OPUS_GET_VBR_CONSTRAINT_REQUEST      4021
#define OPUS_SET_FORCE_CHANNELS_REQUEST      4022
#define OPUS_GET_FORCE_CHANNELS_REQUEST      4023
#define OPUS_SET_SIGNAL_REQUEST              4024
#define OPUS_GET_SIGNAL_REQUEST              4025
#define OPUS_GET_LOOKAHEAD_REQUEST           4027
/* #define OPUS_RESET_STATE 4028 */
#define OPUS_GET_FINAL_RANGE_REQUEST         4031
#define OPUS_GET_PITCH_REQUEST               4033

/* Macros to trigger compilation errors when the wrong types are provided to a CTL */
#define __opus_check_int(x) (((void)((x) == (opus_int32)0)), (opus_int32)(x))
#define __opus_check_int_ptr(ptr) ((ptr) + ((ptr) - (opus_int32*)(ptr)))
#define __opus_check_uint_ptr(ptr) ((ptr) + ((ptr) - (opus_uint32*)(ptr)))
/** @endcond */

/** @defgroup ctlvalues Pre-defined values for CTL interface
  * @see genericctls, encoderctls
  * @{
  */
/* Values for the various encoder CTLs */
#define OPUS_AUTO                           -1000 /**<Auto/default setting @hideinitializer*/
#define OPUS_BITRATE_MAX                       -1 /**<Maximum bitrate @hideinitializer*/

/** Best for most VoIP/videoconference applications where listening quality and intelligibility matter most
 * @hideinitializer */
#define OPUS_APPLICATION_VOIP                2048
/** Best for broadcast/high-fidelity application where the decoded audio should be as close as possible to the input
 * @hideinitializer */
#define OPUS_APPLICATION_AUDIO               2049
/** Only use when lowest-achievable latency is what matters most. Voice-optimized modes cannot be used.
 * @hideinitializer */
#define OPUS_APPLICATION_RESTRICTED_LOWDELAY 2051

#define OPUS_SIGNAL_VOICE                    3001 /**< Signal being encoded is voice */
#define OPUS_SIGNAL_MUSIC                    3002 /**< Signal being encoded is music */
#define OPUS_BANDWIDTH_NARROWBAND            1101 /**< 4kHz bandpass @hideinitializer*/
#define OPUS_BANDWIDTH_MEDIUMBAND            1102 /**< 6kHz bandpass @hideinitializer*/
#define OPUS_BANDWIDTH_WIDEBAND              1103 /**< 8kHz bandpass @hideinitializer*/
#define OPUS_BANDWIDTH_SUPERWIDEBAND         1104 /**<12kHz bandpass @hideinitializer*/
#define OPUS_BANDWIDTH_FULLBAND              1105 /**<20kHz bandpass @hideinitializer*/

/**@}*/


/** @defgroup encoderctls Encoder related CTLs
  *
  * These are convenience macros for use with the \c opus_encode_ctl
  * interface. They are used to generate the appropriate series of
  * arguments for that call, passing the correct type, size and so
  * on as expected for each particular request.
  *
  * Some usage examples:
  *
  * @code
  * int ret;
  * ret = opus_encoder_ctl(enc_ctx, OPUS_SET_BANDWIDTH(OPUS_AUTO));
  * if (ret != OPUS_OK) return ret;
  *
  * int rate;
  * opus_encoder_ctl(enc_ctx, OPUS_GET_BANDWIDTH(&rate));
  *
  * opus_encoder_ctl(enc_ctx, OPUS_RESET_STATE);
  * @endcode
  *
  * @see genericctls, opusencoder
  * @{
  */

/** Configures the encoder's computational complexity.
  * The supported range is 0-10 inclusive with 10 representing the highest complexity.
  * The default value is 10.
  * @param[in] x <tt>int</tt>:   0-10, inclusive
  * @hideinitializer */
#define OPUS_SET_COMPLEXITY(x) OPUS_SET_COMPLEXITY_REQUEST, __opus_check_int(x)
/** Gets the encoder's complexity configuration, @see OPUS_SET_COMPLEXITY
  * @param[out] x <tt>int*</tt>: 0-10, inclusive
  * @hideinitializer */
#define OPUS_GET_COMPLEXITY(x) OPUS_GET_COMPLEXITY_REQUEST, __opus_check_int_ptr(x)

/** Configures the bitrate in the encoder.
  * Rates from 500 to 512000 bits per second are meaningful as well as the
  * special values OPUS_BITRATE_AUTO and OPUS_BITRATE_MAX.
  * The value OPUS_BITRATE_MAX can be used to cause the codec to use as much rate
  * as it can, which is useful for controlling the rate by adjusting the output
  * buffer size.
  * @param[in] x <tt>opus_int32</tt>:   bitrate in bits per second.
  * @hideinitializer */
#define OPUS_SET_BITRATE(x) OPUS_SET_BITRATE_REQUEST, __opus_check_int(x)
/** Gets the encoder's bitrate configuration, @see OPUS_SET_BITRATE
  * @param[out] x <tt>opus_int32*</tt>: bitrate in bits per second.
  * @hideinitializer */
#define OPUS_GET_BITRATE(x) OPUS_GET_BITRATE_REQUEST, __opus_check_int_ptr(x)

/** Configures VBR in the encoder.
  * The following values are currently supported:
  *  - 0 CBR
  *  - 1 VBR (default)
  * The configured bitrate may not be met exactly because frames must
  * be an integer number of bytes in length.
  * @warning Only the MDCT mode of Opus can provide hard CBR behavior.
  * @param[in] x <tt>int</tt>:   0; 1 (default)
  * @hideinitializer */
#define OPUS_SET_VBR(x) OPUS_SET_VBR_REQUEST, __opus_check_int(x)
/** Gets the encoder's VBR configuration, @see OPUS_SET_VBR
  * @param[out] x <tt>int*</tt>: 0; 1
  * @hideinitializer */
#define OPUS_GET_VBR(x) OPUS_GET_VBR_REQUEST, __opus_check_int_ptr(x)

/** Configures constrained VBR in the encoder.
  * The following values are currently supported:
  *  - 0 Unconstrained VBR (default)
  *  - 1 Maximum one frame buffering delay assuming transport with a serialization speed of the nominal bitrate
  * This setting is irrelevant when the encoder is in CBR mode.
  * @warning Only the MDCT mode of Opus currently heeds the constraint.
  *  Speech mode ignores it completely, hybrid mode may fail to obey it
  *  if the LPC layer uses more bitrate than the constraint would have
  *  permitted.
  * @param[in] x <tt>int</tt>:   0 (default); 1
  * @hideinitializer */
#define OPUS_SET_VBR_CONSTRAINT(x) OPUS_SET_VBR_CONSTRAINT_REQUEST, __opus_check_int(x)
/** Gets the encoder's constrained VBR configuration @see OPUS_SET_VBR_CONSTRAINT
  * @param[out] x <tt>int*</tt>: 0; 1
  * @hideinitializer */
#define OPUS_GET_VBR_CONSTRAINT(x) OPUS_GET_VBR_CONSTRAINT_REQUEST, __opus_check_int_ptr(x)

/** Configures mono/stereo forcing in the encoder.
  * This is useful when the caller knows that the input signal is currently a mono
  * source embedded in a stereo stream.
  * @param[in] x <tt>int</tt>:   OPUS_AUTO (default); 1 (forced mono); 2 (forced stereo)
  * @hideinitializer */
#define OPUS_SET_FORCE_CHANNELS(x) OPUS_SET_FORCE_CHANNELS_REQUEST, __opus_check_int(x)
/** Gets the encoder's forced channel configuration, @see OPUS_SET_FORCE_CHANNELS
  * @param[out] x <tt>int*</tt>: OPUS_AUTO; 0; 1
  * @hideinitializer */
#define OPUS_GET_FORCE_CHANNELS(x) OPUS_GET_FORCE_CHANNELS_REQUEST, __opus_check_int_ptr(x)

/** Configures the encoder's maximum bandpass allowed, @see OPUS_GET_MAX_BANDWIDTH
  * The supported values are:
  *  - OPUS_BANDWIDTH_NARROWBAND     4kHz passband
  *  - OPUS_BANDWIDTH_MEDIUMBAND     6kHz passband
  *  - OPUS_BANDWIDTH_WIDEBAND       8kHz passband
  *  - OPUS_BANDWIDTH_SUPERWIDEBAND 12kHz passband
  *  - OPUS_BANDWIDTH_FULLBAND      20kHz passband (default)
  * @param[in] x <tt>int</tt>:   Bandwidth value
  * @hideinitializer */
#define OPUS_SET_MAX_BANDWIDTH(x) OPUS_SET_MAX_BANDWIDTH_REQUEST, __opus_check_int(x)

/** Gets the encoder's configured maximum bandpass allowed, @see OPUS_SET_MAX_BANDWIDTH
  * @param[out] x <tt>int*</tt>: Bandwidth value
  * @hideinitializer */
#define OPUS_GET_MAX_BANDWIDTH(x) OPUS_GET_MAX_BANDWIDTH_REQUEST, __opus_check_int_ptr(x)

/** Configures the encoder's bandpass, @see OPUS_GET_BANDWIDTH
  * The supported values are:
  *  - OPUS_AUTO (default)
  *  - OPUS_BANDWIDTH_NARROWBAND     4kHz passband
  *  - OPUS_BANDWIDTH_MEDIUMBAND     6kHz passband
  *  - OPUS_BANDWIDTH_WIDEBAND       8kHz passband
  *  - OPUS_BANDWIDTH_SUPERWIDEBAND 12kHz passband
  *  - OPUS_BANDWIDTH_FULLBAND      20kHz passband
  * @param[in] x <tt>int</tt>:   Bandwidth value
  * @hideinitializer */
#define OPUS_SET_BANDWIDTH(x) OPUS_SET_BANDWIDTH_REQUEST, __opus_check_int(x)

/** Configures the type of signal being encoded.
  * This is a hint which helps the encoder's mode selection.
  * The supported values are:
  *  - OPUS_SIGNAL_AUTO (default)
  *  - OPUS_SIGNAL_VOICE
  *  - OPUS_SIGNAL_MUSIC
  * @param[in] x <tt>int</tt>:   Signal type
  * @hideinitializer */
#define OPUS_SET_SIGNAL(x) OPUS_SET_SIGNAL_REQUEST, __opus_check_int(x)
/** Gets the encoder's configured signal type, @see OPUS_SET_SIGNAL
  *
  * @param[out] x <tt>int*</tt>: Signal type
  * @hideinitializer */
#define OPUS_GET_SIGNAL(x) OPUS_GET_SIGNAL_REQUEST, __opus_check_int_ptr(x)


/** Configures the encoder's intended application.
  * The initial value is a mandatory argument to the encoder_create function.
  * The supported values are:
  *  - OPUS_APPLICATION_VOIP Process signal for improved speech intelligibility
  *  - OPUS_APPLICATION_AUDIO Favor faithfulness to the original input
  *  - OPUS_APPLICATION_RESTRICTED_LOWDELAY Configure the minimum possible coding delay
  *
  * @param[in] x <tt>int</tt>:     Application value
  * @hideinitializer */
#define OPUS_SET_APPLICATION(x) OPUS_SET_APPLICATION_REQUEST, __opus_check_int(x)
/** Gets the encoder's configured application, @see OPUS_SET_APPLICATION
  *
  * @param[out] x <tt>int*</tt>:   Application value
  * @hideinitializer */
#define OPUS_GET_APPLICATION(x) OPUS_GET_APPLICATION_REQUEST, __opus_check_int_ptr(x)

/** Gets the total samples of delay added by the entire codec.
  * This can be queried by the encoder and then the provided number of samples can be
  * skipped on from the start of the decoder's output to provide time aligned input
  * and output. From the perspective of a decoding application the real data begins this many
  * samples late.
  *
  * The decoder contribution to this delay is identical for all decoders, but the
  * encoder portion of the delay may vary from implementation to implementation,
  * version to version, or even depend on the encoder's initial configuration.
  * Applications needing delay compensation should call this CTL rather than
  * hard-coding a value.
  * @param[out] x <tt>int*</tt>:   Number of lookahead samples
  * @hideinitializer */
#define OPUS_GET_LOOKAHEAD(x) OPUS_GET_LOOKAHEAD_REQUEST, __opus_check_int_ptr(x)

/** Configures the encoder's use of inband forward error correction.
  * @note This is only applicable to the LPC layer
  *
  * @param[in] x <tt>int</tt>:   FEC flag, 0 (disabled) is default
  * @hideinitializer */
#define OPUS_SET_INBAND_FEC(x) OPUS_SET_INBAND_FEC_REQUEST, __opus_check_int(x)
/** Gets encoder's configured use of inband forward error correction, @see OPUS_SET_INBAND_FEC
  *
  * @param[out] x <tt>int*</tt>: FEC flag
  * @hideinitializer */
#define OPUS_GET_INBAND_FEC(x) OPUS_GET_INBAND_FEC_REQUEST, __opus_check_int_ptr(x)

/** Configures the encoder's expected packet loss percentage.
  * Higher values with trigger progressively more loss resistant behavior in the encoder
  * at the expense of quality at a given bitrate in the lossless case, but greater quality
  * under loss.
  *
  * @param[in] x <tt>int</tt>:   Loss percentage in the range 0-100, inclusive.
  * @hideinitializer */
#define OPUS_SET_PACKET_LOSS_PERC(x) OPUS_SET_PACKET_LOSS_PERC_REQUEST, __opus_check_int(x)
/** Gets the encoder's configured packet loss percentage, @see OPUS_SET_PACKET_LOSS_PERC
  *
  * @param[out] x <tt>int*</tt>: Loss percentage in the range 0-100, inclusive.
  * @hideinitializer */
#define OPUS_GET_PACKET_LOSS_PERC(x) OPUS_GET_PACKET_LOSS_PERC_REQUEST, __opus_check_int_ptr(x)

/** Configures the encoder's use of discontinuous transmission.
  * @note This is only applicable to the LPC layer
  *
  * @param[in] x <tt>int</tt>:   DTX flag, 0 (disabled) is default
  * @hideinitializer */
#define OPUS_SET_DTX(x) OPUS_SET_DTX_REQUEST, __opus_check_int(x)
/** Gets encoder's configured use of discontinuous transmission, @see OPUS_SET_DTX
  *
  * @param[out] x <tt>int*</tt>:  DTX flag
  * @hideinitializer */
#define OPUS_GET_DTX(x) OPUS_GET_DTX_REQUEST, __opus_check_int_ptr(x)
/**@}*/

/** @defgroup genericctls Generic CTLs
  *
  * These macros are used with the \c opus_decoder_ctl and
  * \c opus_encoder_ctl calls to generate a particular
  * request.
  *
  * When called on an \c OpusDecoder they apply to that
  * particular decoder instance. When called on an
  * \c OpusEncoder they apply to the corresponding setting
  * on that encoder instance, if present.
  *
  * Some usage examples:
  *
  * @code
  * int ret;
  * opus_int32 pitch;
  * ret = opus_decoder_ctl(dec_ctx, OPUS_GET_PITCH(&pitch));
  * if (ret == OPUS_OK) return ret;
  *
  * opus_encoder_ctl(enc_ctx, OPUS_RESET_STATE);
  * opus_decoder_ctl(dec_ctx, OPUS_RESET_STATE);
  *
  * opus_int32 enc_bw, dec_bw;
  * opus_encoder_ctl(enc_ctx, OPUS_GET_BANDWIDTH(&enc_bw));
  * opus_decoder_ctl(dec_ctx, OPUS_GET_BANDWIDTH(&dec_bw));
  * if (enc_bw != dec_bw) {
  *   printf("packet bandwidth mismatch!\n");
  * }
  * @endcode
  *
  * @see opusencoder, opus_decoder_ctl, opus_encoder_ctl
  * @{
  */

/** Resets the codec state to be equivalent to a freshly initialized state.
  * This should be called when switching streams in order to prevent
  * the back to back decoding from giving different results from
  * one at a time decoding.
  * @hideinitializer */
#define OPUS_RESET_STATE 4028

/** Gets the final state of the codec's entropy coder.
  * This is used for testing purposes,
  * The encoder and decoder state should be identical after coding a payload
  * (assuming no data corruption or software bugs)
  *
  * @param[out] x <tt>opus_uint32*</tt>: Entropy coder state
  *
  * @hideinitializer */
#define OPUS_GET_FINAL_RANGE(x) OPUS_GET_FINAL_RANGE_REQUEST, __opus_check_uint_ptr(x)

/** Gets the pitch of the last decoded frame, if available.
  * This can be used for any post-processing algorithm requiring the use of pitch,
  * e.g. time stretching/shortening. If the last frame was not voiced, or if the
  * pitch was not coded in the frame, then zero is returned.
  *
  * This CTL is only implemented for decoder instances.
  *
  * @param[out] x <tt>opus_int32*</tt>: pitch period at 48 kHz (or 0 if not available)
  *
  * @hideinitializer */
#define OPUS_GET_PITCH(x) OPUS_GET_PITCH_REQUEST, __opus_check_int_ptr(x)

/** Gets the encoder's configured bandpass or the decoder's last bandpass, @see OPUS_SET_BANDWIDTH
  * @param[out] x <tt>int*</tt>: Bandwidth value
  * @hideinitializer */
#define OPUS_GET_BANDWIDTH(x) OPUS_GET_BANDWIDTH_REQUEST, __opus_check_int_ptr(x)

/**@}*/

/** @defgroup libinfo Opus library information functions
  * @{
  */

/** Converts an opus error code into a human readable string.
  *
  * @param[in] error <tt>int</tt>: Error number
  * @returns Error string
  */
OPUS_EXPORT const char *opus_strerror(int error);

/** Gets the libopus version string.
  *
  * @returns Version string
  */
OPUS_EXPORT const char *opus_get_version_string(void);
/**@}*/

#ifdef __cplusplus
}
#endif

#endif /* OPUS_DEFINES_H */

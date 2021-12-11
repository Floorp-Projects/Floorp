/* Simple Plugin API
 *
 * Copyright © 2018 Wim Taymans
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#ifndef SPA_AUDIO_RAW_H
#define SPA_AUDIO_RAW_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#ifndef __FreeBSD__
#include <endian.h>
#endif

#define SPA_AUDIO_MAX_CHANNELS	64u

enum spa_audio_format {
	SPA_AUDIO_FORMAT_UNKNOWN,
	SPA_AUDIO_FORMAT_ENCODED,

	/* interleaved formats */
	SPA_AUDIO_FORMAT_START_Interleaved	= 0x100,
	SPA_AUDIO_FORMAT_S8,
	SPA_AUDIO_FORMAT_U8,
	SPA_AUDIO_FORMAT_S16_LE,
	SPA_AUDIO_FORMAT_S16_BE,
	SPA_AUDIO_FORMAT_U16_LE,
	SPA_AUDIO_FORMAT_U16_BE,
	SPA_AUDIO_FORMAT_S24_32_LE,
	SPA_AUDIO_FORMAT_S24_32_BE,
	SPA_AUDIO_FORMAT_U24_32_LE,
	SPA_AUDIO_FORMAT_U24_32_BE,
	SPA_AUDIO_FORMAT_S32_LE,
	SPA_AUDIO_FORMAT_S32_BE,
	SPA_AUDIO_FORMAT_U32_LE,
	SPA_AUDIO_FORMAT_U32_BE,
	SPA_AUDIO_FORMAT_S24_LE,
	SPA_AUDIO_FORMAT_S24_BE,
	SPA_AUDIO_FORMAT_U24_LE,
	SPA_AUDIO_FORMAT_U24_BE,
	SPA_AUDIO_FORMAT_S20_LE,
	SPA_AUDIO_FORMAT_S20_BE,
	SPA_AUDIO_FORMAT_U20_LE,
	SPA_AUDIO_FORMAT_U20_BE,
	SPA_AUDIO_FORMAT_S18_LE,
	SPA_AUDIO_FORMAT_S18_BE,
	SPA_AUDIO_FORMAT_U18_LE,
	SPA_AUDIO_FORMAT_U18_BE,
	SPA_AUDIO_FORMAT_F32_LE,
	SPA_AUDIO_FORMAT_F32_BE,
	SPA_AUDIO_FORMAT_F64_LE,
	SPA_AUDIO_FORMAT_F64_BE,

	/* planar formats */
	SPA_AUDIO_FORMAT_START_Planar		= 0x200,
	SPA_AUDIO_FORMAT_U8P,
	SPA_AUDIO_FORMAT_S16P,
	SPA_AUDIO_FORMAT_S24_32P,
	SPA_AUDIO_FORMAT_S32P,
	SPA_AUDIO_FORMAT_S24P,
	SPA_AUDIO_FORMAT_F32P,
	SPA_AUDIO_FORMAT_F64P,

	/* other formats start here */
	SPA_AUDIO_FORMAT_START_Other		= 0x400,

	/* Aliases */

	/* DSP formats */
	SPA_AUDIO_FORMAT_DSP_S32 = SPA_AUDIO_FORMAT_S24_32P,
	SPA_AUDIO_FORMAT_DSP_F32 = SPA_AUDIO_FORMAT_F32P,
	SPA_AUDIO_FORMAT_DSP_F64 = SPA_AUDIO_FORMAT_F64P,

	/* native endian */
#if __BYTE_ORDER == __BIG_ENDIAN
	SPA_AUDIO_FORMAT_S16 = SPA_AUDIO_FORMAT_S16_BE,
	SPA_AUDIO_FORMAT_U16 = SPA_AUDIO_FORMAT_U16_BE,
	SPA_AUDIO_FORMAT_S24_32 = SPA_AUDIO_FORMAT_S24_32_BE,
	SPA_AUDIO_FORMAT_U24_32 = SPA_AUDIO_FORMAT_U24_32_BE,
	SPA_AUDIO_FORMAT_S32 = SPA_AUDIO_FORMAT_S32_BE,
	SPA_AUDIO_FORMAT_U32 = SPA_AUDIO_FORMAT_U32_BE,
	SPA_AUDIO_FORMAT_S24 = SPA_AUDIO_FORMAT_S24_BE,
	SPA_AUDIO_FORMAT_U24 = SPA_AUDIO_FORMAT_U24_BE,
	SPA_AUDIO_FORMAT_S20 = SPA_AUDIO_FORMAT_S20_BE,
	SPA_AUDIO_FORMAT_U20 = SPA_AUDIO_FORMAT_U20_BE,
	SPA_AUDIO_FORMAT_S18 = SPA_AUDIO_FORMAT_S18_BE,
	SPA_AUDIO_FORMAT_U18 = SPA_AUDIO_FORMAT_U18_BE,
	SPA_AUDIO_FORMAT_F32 = SPA_AUDIO_FORMAT_F32_BE,
	SPA_AUDIO_FORMAT_F64 = SPA_AUDIO_FORMAT_F64_BE,
	SPA_AUDIO_FORMAT_S16_OE = SPA_AUDIO_FORMAT_S16_LE,
	SPA_AUDIO_FORMAT_U16_OE = SPA_AUDIO_FORMAT_U16_LE,
	SPA_AUDIO_FORMAT_S24_32_OE = SPA_AUDIO_FORMAT_S24_32_LE,
	SPA_AUDIO_FORMAT_U24_32_OE = SPA_AUDIO_FORMAT_U24_32_LE,
	SPA_AUDIO_FORMAT_S32_OE = SPA_AUDIO_FORMAT_S32_LE,
	SPA_AUDIO_FORMAT_U32_OE = SPA_AUDIO_FORMAT_U32_LE,
	SPA_AUDIO_FORMAT_S24_OE = SPA_AUDIO_FORMAT_S24_LE,
	SPA_AUDIO_FORMAT_U24_OE = SPA_AUDIO_FORMAT_U24_LE,
	SPA_AUDIO_FORMAT_S20_OE = SPA_AUDIO_FORMAT_S20_LE,
	SPA_AUDIO_FORMAT_U20_OE = SPA_AUDIO_FORMAT_U20_LE,
	SPA_AUDIO_FORMAT_S18_OE = SPA_AUDIO_FORMAT_S18_LE,
	SPA_AUDIO_FORMAT_U18_OE = SPA_AUDIO_FORMAT_U18_LE,
	SPA_AUDIO_FORMAT_F32_OE = SPA_AUDIO_FORMAT_F32_LE,
	SPA_AUDIO_FORMAT_F64_OE = SPA_AUDIO_FORMAT_F64_LE,
#elif __BYTE_ORDER == __LITTLE_ENDIAN
	SPA_AUDIO_FORMAT_S16 = SPA_AUDIO_FORMAT_S16_LE,
	SPA_AUDIO_FORMAT_U16 = SPA_AUDIO_FORMAT_U16_LE,
	SPA_AUDIO_FORMAT_S24_32 = SPA_AUDIO_FORMAT_S24_32_LE,
	SPA_AUDIO_FORMAT_U24_32 = SPA_AUDIO_FORMAT_U24_32_LE,
	SPA_AUDIO_FORMAT_S32 = SPA_AUDIO_FORMAT_S32_LE,
	SPA_AUDIO_FORMAT_U32 = SPA_AUDIO_FORMAT_U32_LE,
	SPA_AUDIO_FORMAT_S24 = SPA_AUDIO_FORMAT_S24_LE,
	SPA_AUDIO_FORMAT_U24 = SPA_AUDIO_FORMAT_U24_LE,
	SPA_AUDIO_FORMAT_S20 = SPA_AUDIO_FORMAT_S20_LE,
	SPA_AUDIO_FORMAT_U20 = SPA_AUDIO_FORMAT_U20_LE,
	SPA_AUDIO_FORMAT_S18 = SPA_AUDIO_FORMAT_S18_LE,
	SPA_AUDIO_FORMAT_U18 = SPA_AUDIO_FORMAT_U18_LE,
	SPA_AUDIO_FORMAT_F32 = SPA_AUDIO_FORMAT_F32_LE,
	SPA_AUDIO_FORMAT_F64 = SPA_AUDIO_FORMAT_F64_LE,
	SPA_AUDIO_FORMAT_S16_OE = SPA_AUDIO_FORMAT_S16_BE,
	SPA_AUDIO_FORMAT_U16_OE = SPA_AUDIO_FORMAT_U16_BE,
	SPA_AUDIO_FORMAT_S24_32_OE = SPA_AUDIO_FORMAT_S24_32_BE,
	SPA_AUDIO_FORMAT_U24_32_OE = SPA_AUDIO_FORMAT_U24_32_BE,
	SPA_AUDIO_FORMAT_S32_OE = SPA_AUDIO_FORMAT_S32_BE,
	SPA_AUDIO_FORMAT_U32_OE = SPA_AUDIO_FORMAT_U32_BE,
	SPA_AUDIO_FORMAT_S24_OE = SPA_AUDIO_FORMAT_S24_BE,
	SPA_AUDIO_FORMAT_U24_OE = SPA_AUDIO_FORMAT_U24_BE,
	SPA_AUDIO_FORMAT_S20_OE = SPA_AUDIO_FORMAT_S20_BE,
	SPA_AUDIO_FORMAT_U20_OE = SPA_AUDIO_FORMAT_U20_BE,
	SPA_AUDIO_FORMAT_S18_OE = SPA_AUDIO_FORMAT_S18_BE,
	SPA_AUDIO_FORMAT_U18_OE = SPA_AUDIO_FORMAT_U18_BE,
	SPA_AUDIO_FORMAT_F32_OE = SPA_AUDIO_FORMAT_F32_BE,
	SPA_AUDIO_FORMAT_F64_OE = SPA_AUDIO_FORMAT_F64_BE,
#endif
};

#define SPA_AUDIO_FORMAT_IS_INTERLEAVED(fmt)	((fmt) > SPA_AUDIO_FORMAT_START_Interleaved && (fmt) < SPA_AUDIO_FORMAT_START_Planar)
#define SPA_AUDIO_FORMAT_IS_PLANAR(fmt)		((fmt) > SPA_AUDIO_FORMAT_START_Planar && (fmt) < SPA_AUDIO_FORMAT_START_Other)

enum spa_audio_channel {
	SPA_AUDIO_CHANNEL_UNKNOWN,	/**< unspecified */
	SPA_AUDIO_CHANNEL_NA,		/**< N/A, silent */

	SPA_AUDIO_CHANNEL_MONO,         /**< mono stream */

	SPA_AUDIO_CHANNEL_FL,           /**< front left */
	SPA_AUDIO_CHANNEL_FR,           /**< front right */
	SPA_AUDIO_CHANNEL_FC,           /**< front center */
	SPA_AUDIO_CHANNEL_LFE,          /**< LFE */
	SPA_AUDIO_CHANNEL_SL,           /**< side left */
	SPA_AUDIO_CHANNEL_SR,           /**< side right */
	SPA_AUDIO_CHANNEL_FLC,          /**< front left center */
	SPA_AUDIO_CHANNEL_FRC,          /**< front right center */
	SPA_AUDIO_CHANNEL_RC,           /**< rear center */
	SPA_AUDIO_CHANNEL_RL,           /**< rear left */
	SPA_AUDIO_CHANNEL_RR,           /**< rear right */
	SPA_AUDIO_CHANNEL_TC,           /**< top center */
	SPA_AUDIO_CHANNEL_TFL,          /**< top front left */
	SPA_AUDIO_CHANNEL_TFC,          /**< top front center */
	SPA_AUDIO_CHANNEL_TFR,          /**< top front right */
	SPA_AUDIO_CHANNEL_TRL,          /**< top rear left */
	SPA_AUDIO_CHANNEL_TRC,          /**< top rear center */
	SPA_AUDIO_CHANNEL_TRR,          /**< top rear right */
	SPA_AUDIO_CHANNEL_RLC,          /**< rear left center */
	SPA_AUDIO_CHANNEL_RRC,          /**< rear right center */
	SPA_AUDIO_CHANNEL_FLW,          /**< front left wide */
	SPA_AUDIO_CHANNEL_FRW,          /**< front right wide */
	SPA_AUDIO_CHANNEL_LFE2,		/**< LFE 2 */
	SPA_AUDIO_CHANNEL_FLH,          /**< front left high */
	SPA_AUDIO_CHANNEL_FCH,          /**< front center high */
	SPA_AUDIO_CHANNEL_FRH,          /**< front right high */
	SPA_AUDIO_CHANNEL_TFLC,         /**< top front left center */
	SPA_AUDIO_CHANNEL_TFRC,         /**< top front right center */
	SPA_AUDIO_CHANNEL_TSL,          /**< top side left */
	SPA_AUDIO_CHANNEL_TSR,          /**< top side right */
	SPA_AUDIO_CHANNEL_LLFE,         /**< left LFE */
	SPA_AUDIO_CHANNEL_RLFE,         /**< right LFE */
	SPA_AUDIO_CHANNEL_BC,           /**< bottom center */
	SPA_AUDIO_CHANNEL_BLC,          /**< bottom left center */
	SPA_AUDIO_CHANNEL_BRC,          /**< bottom right center */

	SPA_AUDIO_CHANNEL_CUSTOM_START	= 0x10000,
};

/** Extra audio flags */
#define SPA_AUDIO_FLAG_NONE		(0)		/*< no valid flag */
#define SPA_AUDIO_FLAG_UNPOSITIONED	(1 << 0)	/*< the position array explicitly
							 *  contains unpositioned channels. */
/** Audio information description */
struct spa_audio_info_raw {
	enum spa_audio_format format;		/*< format, one of enum spa_audio_format */
	uint32_t flags;				/*< extra flags */
	uint32_t rate;				/*< sample rate */
	uint32_t channels;			/*< number of channels */
	uint32_t position[SPA_AUDIO_MAX_CHANNELS];	/*< channel position from enum spa_audio_channel */
};

#define SPA_AUDIO_INFO_RAW_INIT(...)		(struct spa_audio_info_raw) { __VA_ARGS__ }

#define SPA_KEY_AUDIO_CHANNEL		"audio.channel"		/**< an audio channel as string,
								  *  Ex. "FL" */
#define SPA_KEY_AUDIO_CHANNELS		"audio.channels"	/**< an audio channel count as int */
#define SPA_KEY_AUDIO_RATE		"audio.rate"		/**< an audio sample rate as int */

struct spa_audio_info_dsp {
	enum spa_audio_format format;		/*< format, one of the DSP formats in enum spa_audio_format_dsp */
};

#define SPA_AUDIO_INFO_DSP_INIT(...)		(struct spa_audio_info_dsp) { __VA_ARGS__ }

#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif /* SPA_AUDIO_RAW_H */

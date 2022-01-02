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

#ifndef SPA_VIDEO_RAW_H
#define SPA_VIDEO_RAW_H

#ifdef __cplusplus
extern "C" {
#endif

#include <spa/utils/defs.h>
#include <spa/param/video/chroma.h>
#include <spa/param/video/color.h>
#include <spa/param/video/multiview.h>

#define SPA_VIDEO_MAX_PLANES 4
#define SPA_VIDEO_MAX_COMPONENTS 4

enum spa_video_format {
	SPA_VIDEO_FORMAT_UNKNOWN,
	SPA_VIDEO_FORMAT_ENCODED,

	SPA_VIDEO_FORMAT_I420,
	SPA_VIDEO_FORMAT_YV12,
	SPA_VIDEO_FORMAT_YUY2,
	SPA_VIDEO_FORMAT_UYVY,
	SPA_VIDEO_FORMAT_AYUV,
	SPA_VIDEO_FORMAT_RGBx,
	SPA_VIDEO_FORMAT_BGRx,
	SPA_VIDEO_FORMAT_xRGB,
	SPA_VIDEO_FORMAT_xBGR,
	SPA_VIDEO_FORMAT_RGBA,
	SPA_VIDEO_FORMAT_BGRA,
	SPA_VIDEO_FORMAT_ARGB,
	SPA_VIDEO_FORMAT_ABGR,
	SPA_VIDEO_FORMAT_RGB,
	SPA_VIDEO_FORMAT_BGR,
	SPA_VIDEO_FORMAT_Y41B,
	SPA_VIDEO_FORMAT_Y42B,
	SPA_VIDEO_FORMAT_YVYU,
	SPA_VIDEO_FORMAT_Y444,
	SPA_VIDEO_FORMAT_v210,
	SPA_VIDEO_FORMAT_v216,
	SPA_VIDEO_FORMAT_NV12,
	SPA_VIDEO_FORMAT_NV21,
	SPA_VIDEO_FORMAT_GRAY8,
	SPA_VIDEO_FORMAT_GRAY16_BE,
	SPA_VIDEO_FORMAT_GRAY16_LE,
	SPA_VIDEO_FORMAT_v308,
	SPA_VIDEO_FORMAT_RGB16,
	SPA_VIDEO_FORMAT_BGR16,
	SPA_VIDEO_FORMAT_RGB15,
	SPA_VIDEO_FORMAT_BGR15,
	SPA_VIDEO_FORMAT_UYVP,
	SPA_VIDEO_FORMAT_A420,
	SPA_VIDEO_FORMAT_RGB8P,
	SPA_VIDEO_FORMAT_YUV9,
	SPA_VIDEO_FORMAT_YVU9,
	SPA_VIDEO_FORMAT_IYU1,
	SPA_VIDEO_FORMAT_ARGB64,
	SPA_VIDEO_FORMAT_AYUV64,
	SPA_VIDEO_FORMAT_r210,
	SPA_VIDEO_FORMAT_I420_10BE,
	SPA_VIDEO_FORMAT_I420_10LE,
	SPA_VIDEO_FORMAT_I422_10BE,
	SPA_VIDEO_FORMAT_I422_10LE,
	SPA_VIDEO_FORMAT_Y444_10BE,
	SPA_VIDEO_FORMAT_Y444_10LE,
	SPA_VIDEO_FORMAT_GBR,
	SPA_VIDEO_FORMAT_GBR_10BE,
	SPA_VIDEO_FORMAT_GBR_10LE,
	SPA_VIDEO_FORMAT_NV16,
	SPA_VIDEO_FORMAT_NV24,
	SPA_VIDEO_FORMAT_NV12_64Z32,
	SPA_VIDEO_FORMAT_A420_10BE,
	SPA_VIDEO_FORMAT_A420_10LE,
	SPA_VIDEO_FORMAT_A422_10BE,
	SPA_VIDEO_FORMAT_A422_10LE,
	SPA_VIDEO_FORMAT_A444_10BE,
	SPA_VIDEO_FORMAT_A444_10LE,
	SPA_VIDEO_FORMAT_NV61,
	SPA_VIDEO_FORMAT_P010_10BE,
	SPA_VIDEO_FORMAT_P010_10LE,
	SPA_VIDEO_FORMAT_IYU2,
	SPA_VIDEO_FORMAT_VYUY,
	SPA_VIDEO_FORMAT_GBRA,
	SPA_VIDEO_FORMAT_GBRA_10BE,
	SPA_VIDEO_FORMAT_GBRA_10LE,
	SPA_VIDEO_FORMAT_GBR_12BE,
	SPA_VIDEO_FORMAT_GBR_12LE,
	SPA_VIDEO_FORMAT_GBRA_12BE,
	SPA_VIDEO_FORMAT_GBRA_12LE,
	SPA_VIDEO_FORMAT_I420_12BE,
	SPA_VIDEO_FORMAT_I420_12LE,
	SPA_VIDEO_FORMAT_I422_12BE,
	SPA_VIDEO_FORMAT_I422_12LE,
	SPA_VIDEO_FORMAT_Y444_12BE,
	SPA_VIDEO_FORMAT_Y444_12LE,

	SPA_VIDEO_FORMAT_RGBA_F16,
	SPA_VIDEO_FORMAT_RGBA_F32,

	/* Aliases */
	SPA_VIDEO_FORMAT_DSP_F32 = SPA_VIDEO_FORMAT_RGBA_F32,
};

/**
 * spa_video_flags:
 * @SPA_VIDEO_FLAG_NONE: no flags
 * @SPA_VIDEO_FLAG_VARIABLE_FPS: a variable fps is selected, fps_n and fps_d
 *     denote the maximum fps of the video
 * @SPA_VIDEO_FLAG_PREMULTIPLIED_ALPHA: Each color has been scaled by the alpha
 *     value.
 *
 * Extra video flags
 */
enum spa_video_flags {
	SPA_VIDEO_FLAG_NONE = 0,
	SPA_VIDEO_FLAG_VARIABLE_FPS = (1 << 0),
	SPA_VIDEO_FLAG_PREMULTIPLIED_ALPHA = (1 << 1)
};

/**
 * spa_video_interlace_mode:
 * @SPA_VIDEO_INTERLACE_MODE_PROGRESSIVE: all frames are progressive
 * @SPA_VIDEO_INTERLACE_MODE_INTERLEAVED: 2 fields are interleaved in one video
 *     frame. Extra buffer flags describe the field order.
 * @SPA_VIDEO_INTERLACE_MODE_MIXED: frames contains both interlaced and
 *     progressive video, the buffer flags describe the frame and fields.
 * @SPA_VIDEO_INTERLACE_MODE_FIELDS: 2 fields are stored in one buffer, use the
 *     frame ID to get access to the required field. For multiview (the
 *     'views' property > 1) the fields of view N can be found at frame ID
 *     (N * 2) and (N * 2) + 1.
 *     Each field has only half the amount of lines as noted in the
 *     height property. This mode requires multiple spa_data
 *     to describe the fields.
 *
 * The possible values of the #spa_video_interlace_mode describing the interlace
 * mode of the stream.
 */
enum spa_video_interlace_mode {
	SPA_VIDEO_INTERLACE_MODE_PROGRESSIVE = 0,
	SPA_VIDEO_INTERLACE_MODE_INTERLEAVED,
	SPA_VIDEO_INTERLACE_MODE_MIXED,
	SPA_VIDEO_INTERLACE_MODE_FIELDS
};

/**
 * spa_video_info_raw:
 * @format: the format
 * @modifier: format modifier
 * @size: the frame size of the video
 * @framerate: the framerate of the video 0/1 means variable rate
 * @max_framerate: the maximum framerate of the video. This is only valid when
 *             @framerate is 0/1
 * @views: the number of views in this video
 * @interlace_mode: the interlace mode
 * @pixel_aspect_ratio: The pixel aspect ratio
 * @multiview_mode: multiview mode
 * @multiview_flags: multiview flags
 * @chroma_site: the chroma siting
 * @color_range: the color range. This is the valid range for the samples.
 *         It is used to convert the samples to Y'PbPr values.
 * @color_matrix: the color matrix. Used to convert between Y'PbPr and
 *         non-linear RGB (R'G'B')
 * @transfer_function: the transfer function. used to convert between R'G'B' and RGB
 * @color_primaries: color primaries. used to convert between R'G'B' and CIE XYZ
 */
struct spa_video_info_raw {
	enum spa_video_format format;
	int64_t modifier;
	struct spa_rectangle size;
	struct spa_fraction framerate;
	struct spa_fraction max_framerate;
	uint32_t views;
	enum spa_video_interlace_mode interlace_mode;
	struct spa_fraction pixel_aspect_ratio;
	enum spa_video_multiview_mode multiview_mode;
	enum spa_video_multiview_flags multiview_flags;
	enum spa_video_chroma_site chroma_site;
	enum spa_video_color_range color_range;
	enum spa_video_color_matrix color_matrix;
	enum spa_video_transfer_function transfer_function;
	enum spa_video_color_primaries color_primaries;
};

#define SPA_VIDEO_INFO_RAW_INIT(...)	(struct spa_video_info_raw) { __VA_ARGS__ }

struct spa_video_info_dsp {
	enum spa_video_format format;
	int64_t modifier;
};

#define SPA_VIDEO_INFO_DSP_INIT(...)	(struct spa_video_info_dsp) { __VA_ARGS__ }

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* SPA_VIDEO_RAW_H */

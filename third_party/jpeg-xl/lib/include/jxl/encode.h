/* Copyright (c) the JPEG XL Project Authors. All rights reserved.
 *
 * Use of this source code is governed by a BSD-style
 * license that can be found in the LICENSE file.
 */

/** @addtogroup libjxl_encoder
 * @{
 * @file encode.h
 * @brief Encoding API for JPEG XL.
 */

#ifndef JXL_ENCODE_H_
#define JXL_ENCODE_H_

#include "jxl/codestream_header.h"
#include "jxl/decode.h"
#include "jxl/jxl_export.h"
#include "jxl/memory_manager.h"
#include "jxl/parallel_runner.h"

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

/**
 * Encoder library version.
 *
 * @return the encoder library version as an integer:
 * MAJOR_VERSION * 1000000 + MINOR_VERSION * 1000 + PATCH_VERSION. For example,
 * version 1.2.3 would return 1002003.
 */
JXL_EXPORT uint32_t JxlEncoderVersion(void);

/**
 * Opaque structure that holds the JPEG XL encoder.
 *
 * Allocated and initialized with JxlEncoderCreate().
 * Cleaned up and deallocated with JxlEncoderDestroy().
 */
typedef struct JxlEncoderStruct JxlEncoder;

/**
 * Settings and metadata for a single image frame. This includes encoder options
 * for a frame such as compression quality and speed.
 *
 * Allocated and initialized with JxlEncoderFrameSettingsCreate().
 * Cleaned up and deallocated when the encoder is destroyed with
 * JxlEncoderDestroy().
 */
typedef struct JxlEncoderFrameSettingsStruct JxlEncoderFrameSettings;

/** DEPRECATED: Use JxlEncoderFrameSettings instead.
 */
typedef JxlEncoderFrameSettings JxlEncoderOptions;

/**
 * Return value for multiple encoder functions.
 */
typedef enum {
  /** Function call finished successfully, or encoding is finished and there is
   * nothing more to be done.
   */
  JXL_ENC_SUCCESS = 0,

  /** An error occurred, for example out of memory.
   */
  JXL_ENC_ERROR = 1,

  /** The encoder needs more output buffer to continue encoding.
   */
  JXL_ENC_NEED_MORE_OUTPUT = 2,

  /** DEPRECATED: the encoder does not return this status and there is no need
   * to handle or expect it.
   */
  JXL_ENC_NOT_SUPPORTED = 3,

} JxlEncoderStatus;

/**
 * Id of encoder options for a frame. This includes options such as the
 * image quality and compression speed for this frame. This does not include
 * non-frame related encoder options such as for boxes.
 */
typedef enum {
  /** Sets encoder effort/speed level without affecting decoding speed. Valid
   * values are, from faster to slower speed: 1:lightning 2:thunder 3:falcon
   * 4:cheetah 5:hare 6:wombat 7:squirrel 8:kitten 9:tortoise.
   * Default: squirrel (7).
   */
  JXL_ENC_FRAME_SETTING_EFFORT = 0,

  /** Sets the decoding speed tier for the provided options. Minimum is 0
   * (slowest to decode, best quality/density), and maximum is 4 (fastest to
   * decode, at the cost of some quality/density). Default is 0.
   */
  JXL_ENC_FRAME_SETTING_DECODING_SPEED = 1,

  /** Sets resampling option. If enabled, the image is downsampled before
   * compression, and upsampled to original size in the decoder. Integer option,
   * use -1 for the default behavior (resampling only applied for low quality),
   * 1 for no downsampling (1x1), 2 for 2x2 downsampling, 4 for 4x4
   * downsampling, 8 for 8x8 downsampling.
   */
  JXL_ENC_FRAME_SETTING_RESAMPLING = 2,

  /** Similar to JXL_ENC_FRAME_SETTING_RESAMPLING, but for extra channels.
   * Integer option, use -1 for the default behavior (depends on encoder
   * implementation), 1 for no downsampling (1x1), 2 for 2x2 downsampling, 4 for
   * 4x4 downsampling, 8 for 8x8 downsampling.
   */
  JXL_ENC_FRAME_SETTING_EXTRA_CHANNEL_RESAMPLING = 3,

  /** Indicates the frame added with @ref JxlEncoderAddImageFrame is already
   * downsampled by the downsampling factor set with @ref
   * JXL_ENC_FRAME_SETTING_RESAMPLING. The input frame must then be given in the
   * downsampled resolution, not the full image resolution. The downsampled
   * resolution is given by ceil(xsize / resampling), ceil(ysize / resampling)
   * with xsize and ysize the dimensions given in the basic info, and resampling
   * the factor set with @ref JXL_ENC_FRAME_SETTING_RESAMPLING.
   * Use 0 to disable, 1 to enable. Default value is 0.
   */
  JXL_ENC_FRAME_SETTING_ALREADY_DOWNSAMPLED = 4,

  /** Adds noise to the image emulating photographic film noise, the higher the
   * given number, the grainier the image will be. As an example, a value of 100
   * gives low noise whereas a value of 3200 gives a lot of noise. The default
   * value is 0.
   */
  JXL_ENC_FRAME_SETTING_PHOTON_NOISE = 5,

  /** Enables adaptive noise generation. This setting is not recommended for
   * use, please use JXL_ENC_FRAME_SETTING_PHOTON_NOISE instead. Use -1 for the
   * default (encoder chooses), 0 to disable, 1 to enable.
   */
  JXL_ENC_FRAME_SETTING_NOISE = 6,

  /** Enables or disables dots generation. Use -1 for the default (encoder
   * chooses), 0 to disable, 1 to enable.
   */
  JXL_ENC_FRAME_SETTING_DOTS = 7,

  /** Enables or disables patches generation. Use -1 for the default (encoder
   * chooses), 0 to disable, 1 to enable.
   */
  JXL_ENC_FRAME_SETTING_PATCHES = 8,

  /** Edge preserving filter level, -1 to 3. Use -1 for the default (encoder
   * chooses), 0 to 3 to set a strength.
   */
  JXL_ENC_FRAME_SETTING_EPF = 9,

  /** Enables or disables the gaborish filter. Use -1 for the default (encoder
   * chooses), 0 to disable, 1 to enable.
   */
  JXL_ENC_FRAME_SETTING_GABORISH = 10,

  /** Enables modular encoding. Use -1 for default (encoder
   * chooses), 0 to enforce VarDCT mode (e.g. for photographic images), 1 to
   * enforce modular mode (e.g. for lossless images).
   */
  JXL_ENC_FRAME_SETTING_MODULAR = 11,

  /** Enables or disables preserving color of invisible pixels. Use -1 for the
   * default (1 if lossless, 0 if lossy), 0 to disable, 1 to enable.
   */
  JXL_ENC_FRAME_SETTING_KEEP_INVISIBLE = 12,

  /** Determines the order in which 256x256 regions are stored in the codestream
   * for progressive rendering. Use -1 for the encoder
   * default, 0 for scanline order, 1 for center-first order.
   */
  JXL_ENC_FRAME_SETTING_GROUP_ORDER = 13,

  /** Determines the horizontal position of center for the center-first group
   * order. Use -1 to automatically use the middle of the image, 0..xsize to
   * specifically set it.
   */
  JXL_ENC_FRAME_SETTING_GROUP_ORDER_CENTER_X = 14,

  /** Determines the center for the center-first group order. Use -1 to
   * automatically use the middle of the image, 0..ysize to specifically set it.
   */
  JXL_ENC_FRAME_SETTING_GROUP_ORDER_CENTER_Y = 15,

  /** Enables or disables progressive encoding for modular mode. Use -1 for the
   * encoder default, 0 to disable, 1 to enable.
   */
  JXL_ENC_FRAME_SETTING_RESPONSIVE = 16,

  /** Set the progressive mode for the AC coefficients of VarDCT, using spectral
   * progression from the DCT coefficients. Use -1 for the encoder default, 0 to
   * disable, 1 to enable.
   */
  JXL_ENC_FRAME_SETTING_PROGRESSIVE_AC = 17,

  /** Set the progressive mode for the AC coefficients of VarDCT, using
   * quantization of the least significant bits. Use -1 for the encoder default,
   * 0 to disable, 1 to enable.
   */
  JXL_ENC_FRAME_SETTING_QPROGRESSIVE_AC = 18,

  /** Set the progressive mode using lower-resolution DC images for VarDCT. Use
   * -1 for the encoder default, 0 to disable, 1 to have an extra 64x64 lower
   * resolution pass, 2 to have a 512x512 and 64x64 lower resolution pass.
   */
  JXL_ENC_FRAME_SETTING_PROGRESSIVE_DC = 19,

  /** Use Global channel palette if the amount of colors is smaller than this
   * percentage of range. Use 0-100 to set an explicit percentage, -1 to use the
   * encoder default. Used for modular encoding.
   */
  JXL_ENC_FRAME_SETTING_CHANNEL_COLORS_GLOBAL_PERCENT = 20,

  /** Use Local (per-group) channel palette if the amount of colors is smaller
   * than this percentage of range. Use 0-100 to set an explicit percentage, -1
   * to use the encoder default. Used for modular encoding.
   */
  JXL_ENC_FRAME_SETTING_CHANNEL_COLORS_GROUP_PERCENT = 21,

  /** Use color palette if amount of colors is smaller than or equal to this
   * amount, or -1 to use the encoder default. Used for modular encoding.
   */
  JXL_ENC_FRAME_SETTING_PALETTE_COLORS = 22,

  /** Enables or disables delta palette. Use -1 for the default (encoder
   * chooses), 0 to disable, 1 to enable. Used in modular mode.
   */
  JXL_ENC_FRAME_SETTING_LOSSY_PALETTE = 23,

  /** Color transform for internal encoding: -1 = default, 0=XYB, 1=none (RGB),
   * 2=YCbCr. The XYB setting performs the forward XYB transform. None and
   * YCbCr both perform no transform, but YCbCr is used to indicate that the
   * encoded data losslessly represents YCbCr values.
   */
  JXL_ENC_FRAME_SETTING_COLOR_TRANSFORM = 24,

  /** Color space for modular encoding: -1=default, 0-35=reverse color transform
   * index, e.g. index 0 = none, index 6 = YCoCg.
   * The default behavior is to try several, depending on the speed setting.
   */
  JXL_ENC_FRAME_SETTING_MODULAR_COLOR_SPACE = 25,

  /** Group size for modular encoding: -1=default, 0=128, 1=256, 2=512, 3=1024.
   */
  JXL_ENC_FRAME_SETTING_MODULAR_GROUP_SIZE = 26,

  /** Predictor for modular encoding. -1 = default, 0=zero, 1=left, 2=top,
   * 3=avg0, 4=select, 5=gradient, 6=weighted, 7=topright, 8=topleft,
   * 9=leftleft, 10=avg1, 11=avg2, 12=avg3, 13=toptop predictive average 14=mix
   * 5 and 6, 15=mix everything.
   */
  JXL_ENC_FRAME_SETTING_MODULAR_PREDICTOR = 27,

  /** Fraction of pixels used to learn MA trees as a percentage. -1 = default,
   * 0 = no MA and fast decode, 50 = default value, 100 = all, values above
   * 100 are also permitted. Higher values use more encoder memory.
   */
  JXL_ENC_FRAME_SETTING_MODULAR_MA_TREE_LEARNING_PERCENT = 28,

  /** Number of extra (previous-channel) MA tree properties to use. -1 =
   * default, 0-11 = valid values. Recommended values are in the range 0 to 3,
   * or 0 to amount of channels minus 1 (including all extra channels, and
   * excluding color channels when using VarDCT mode). Higher value gives slower
   * encoding and slower decoding.
   */
  JXL_ENC_FRAME_SETTING_MODULAR_NB_PREV_CHANNELS = 29,

  /** Enable or disable CFL (chroma-from-luma) for lossless JPEG recompression.
   * -1 = default, 0 = disable CFL, 1 = enable CFL.
   */
  JXL_ENC_FRAME_SETTING_JPEG_RECON_CFL = 30,

  /** Enum value not to be used as an option. This value is added to force the
   * C compiler to have the enum to take a known size.
   */
  JXL_ENC_FRAME_SETTING_FILL_ENUM = 65535,

} JxlEncoderFrameSettingId;

/**
 * Creates an instance of JxlEncoder and initializes it.
 *
 * @p memory_manager will be used for all the library dynamic allocations made
 * from this instance. The parameter may be NULL, in which case the default
 * allocator will be used. See jpegxl/memory_manager.h for details.
 *
 * @param memory_manager custom allocator function. It may be NULL. The memory
 *        manager will be copied internally.
 * @return @c NULL if the instance can not be allocated or initialized
 * @return pointer to initialized JxlEncoder otherwise
 */
JXL_EXPORT JxlEncoder* JxlEncoderCreate(const JxlMemoryManager* memory_manager);

/**
 * Re-initializes a JxlEncoder instance, so it can be re-used for encoding
 * another image. All state and settings are reset as if the object was
 * newly created with JxlEncoderCreate, but the memory manager is kept.
 *
 * @param enc instance to be re-initialized.
 */
JXL_EXPORT void JxlEncoderReset(JxlEncoder* enc);

/**
 * Deinitializes and frees JxlEncoder instance.
 *
 * @param enc instance to be cleaned up and deallocated.
 */
JXL_EXPORT void JxlEncoderDestroy(JxlEncoder* enc);

/**
 * Sets the color management system (CMS) that will be used for color conversion
 * (if applicable) during encoding. May only be set before starting encoding. If
 * left unset, the default CMS implementation will be used.
 *
 * @param enc encoder object.
 * @param cms structure representing a CMS implementation. See JxlCmsInterface
 * for more details.
 */
JXL_EXPORT void JxlEncoderSetCms(JxlEncoder* enc, JxlCmsInterface cms);

/**
 * Set the parallel runner for multithreading. May only be set before starting
 * encoding.
 *
 * @param enc encoder object.
 * @param parallel_runner function pointer to runner for multithreading. It may
 *        be NULL to use the default, single-threaded, runner. A multithreaded
 *        runner should be set to reach fast performance.
 * @param parallel_runner_opaque opaque pointer for parallel_runner.
 * @return JXL_ENC_SUCCESS if the runner was set, JXL_ENC_ERROR
 * otherwise (the previous runner remains set).
 */
JXL_EXPORT JxlEncoderStatus
JxlEncoderSetParallelRunner(JxlEncoder* enc, JxlParallelRunner parallel_runner,
                            void* parallel_runner_opaque);

/**
 * Encodes JPEG XL file using the available bytes. @p *avail_out indicates how
 * many output bytes are available, and @p *next_out points to the input bytes.
 * *avail_out will be decremented by the amount of bytes that have been
 * processed by the encoder and *next_out will be incremented by the same
 * amount, so *next_out will now point at the amount of *avail_out unprocessed
 * bytes.
 *
 * The returned status indicates whether the encoder needs more output bytes.
 * When the return value is not JXL_ENC_ERROR or JXL_ENC_SUCCESS, the encoding
 * requires more JxlEncoderProcessOutput calls to continue.
 *
 * This encodes the frames and/or boxes added so far. If the last frame or last
 * box has been added, @ref JxlEncoderCloseInput, @ref JxlEncoderCloseFrames
 * and/or @ref JxlEncoderCloseBoxes must be called before the next
 * @ref JxlEncoderProcessOutput call, or the codestream won't be encoded
 * correctly.
 *
 * @param enc encoder object.
 * @param next_out pointer to next bytes to write to.
 * @param avail_out amount of bytes available starting from *next_out.
 * @return JXL_ENC_SUCCESS when encoding finished and all events handled.
 * @return JXL_ENC_ERROR when encoding failed, e.g. invalid input.
 * @return JXL_ENC_NEED_MORE_OUTPUT more output buffer is necessary.
 */
JXL_EXPORT JxlEncoderStatus JxlEncoderProcessOutput(JxlEncoder* enc,
                                                    uint8_t** next_out,
                                                    size_t* avail_out);

/**
 * Sets the frame information for this frame to the encoder. This includes
 * animation information such as frame duration to store in the frame header.
 * The frame header fields represent the frame as passed to the encoder, but not
 * necessarily the exact values as they will be encoded file format: the encoder
 * could change crop and blending options of a frame for more efficient encoding
 * or introduce additional internal frames. Animation duration and time code
 * information is not altered since those are immutable metadata of the frame.
 *
 * It is not required to use this function, however if have_animation is set
 * to true in the basic info, then this function should be used to set the
 * time duration of this individual frame. By default individual frames have a
 * time duration of 0, making them form a composite still. See @ref
 * JxlFrameHeader for more information.
 *
 * This information is stored in the JxlEncoderFrameSettings and so is used for
 * any frame encoded with these JxlEncoderFrameSettings. It is ok to change
 * between @ref JxlEncoderAddImageFrame calls, each added image frame will have
 * the frame header that was set in the options at the time of calling
 * JxlEncoderAddImageFrame.
 *
 * The is_last and name_length fields of the JxlFrameHeader are ignored, use
 * @ref JxlEncoderCloseFrames to indicate last frame, and @ref
 * JxlEncoderSetFrameName to indicate the name and its length instead.
 * Calling this function will clear any name that was previously set with @ref
 * JxlEncoderSetFrameName.
 *
 * @param frame_settings set of options and metadata for this frame. Also
 * includes reference to the encoder object.
 * @param frame_header frame header data to set. Object owned by the caller and
 * does not need to be kept in memory, its information is copied internally.
 * @return JXL_ENC_SUCCESS on success, JXL_ENC_ERROR on error
 */
JXL_EXPORT JxlEncoderStatus
JxlEncoderSetFrameHeader(JxlEncoderFrameSettings* frame_settings,
                         const JxlFrameHeader* frame_header);

/**
 * Sets blend info of an extra channel. The blend info of extra channels is set
 * separately from that of the color channels, the color channels are set with
 * @ref JxlEncoderSetFrameHeader.
 *
 * @param frame_settings set of options and metadata for this frame. Also
 * includes reference to the encoder object.
 * @param index index of the extra channel to use.
 * @param blend_info blend info to set for the extra channel
 * @return JXL_ENC_SUCCESS on success, JXL_ENC_ERROR on error
 */
JXL_EXPORT JxlEncoderStatus JxlEncoderSetExtraChannelBlendInfo(
    JxlEncoderFrameSettings* frame_settings, size_t index,
    const JxlBlendInfo* blend_info);

/**
 * Sets the name of the animation frame. This function is optional, frames are
 * not required to have a name. This setting is a part of the frame header, and
 * the same principles as for @ref JxlEncoderSetFrameHeader apply. The
 * name_length field of JxlFrameHeader is ignored by the encoder, this function
 * determines the name length instead as the length in bytes of the C string.
 *
 * The maximum possible name length is 1071 bytes (excluding terminating null
 * character).
 *
 * Calling @ref JxlEncoderSetFrameHeader clears any name that was
 * previously set.
 *
 * @param frame_settings set of options and metadata for this frame. Also
 * includes reference to the encoder object.
 * @param frame_name name of the next frame to be encoded, as a UTF-8 encoded C
 * string (zero terminated). Owned by the caller, and copied internally.
 * @return JXL_ENC_SUCCESS on success, JXL_ENC_ERROR on error
 */
JXL_EXPORT JxlEncoderStatus JxlEncoderSetFrameName(
    JxlEncoderFrameSettings* frame_settings, const char* frame_name);

/**
 * Sets the buffer to read JPEG encoded bytes from for the next frame to encode.
 *
 * If JxlEncoderSetBasicInfo has not yet been called, calling
 * JxlEncoderAddJPEGFrame will implicitly call it with the parameters of the
 * added JPEG frame.
 *
 * If JxlEncoderSetColorEncoding or JxlEncoderSetICCProfile has not yet been
 * called, calling JxlEncoderAddJPEGFrame will implicitly call it with the
 * parameters of the added JPEG frame.
 *
 * If the encoder is set to store JPEG reconstruction metadata using @ref
 * JxlEncoderStoreJPEGMetadata and a single JPEG frame is added, it will be
 * possible to losslessly reconstruct the JPEG codestream.
 *
 * If this is the last frame, @ref JxlEncoderCloseInput or @ref
 * JxlEncoderCloseFrames must be called before the next
 * @ref JxlEncoderProcessOutput call.
 *
 * @param frame_settings set of options and metadata for this frame. Also
 * includes reference to the encoder object.
 * @param buffer bytes to read JPEG from. Owned by the caller and its contents
 * are copied internally.
 * @param size size of buffer in bytes.
 * @return JXL_ENC_SUCCESS on success, JXL_ENC_ERROR on error
 */
JXL_EXPORT JxlEncoderStatus
JxlEncoderAddJPEGFrame(const JxlEncoderFrameSettings* frame_settings,
                       const uint8_t* buffer, size_t size);

/**
 * Sets the buffer to read pixels from for the next image to encode. Must call
 * JxlEncoderSetBasicInfo before JxlEncoderAddImageFrame.
 *
 * Currently only some data types for pixel formats are supported:
 * - JXL_TYPE_UINT8, with range 0..255
 * - JXL_TYPE_UINT16, with range 0..65535
 * - JXL_TYPE_FLOAT16, with nominal range 0..1
 * - JXL_TYPE_FLOAT, with nominal range 0..1
 *
 * Note: the sample data type in pixel_format is allowed to be different from
 * what is described in the JxlBasicInfo. The type in pixel_format describes the
 * format of the uncompressed pixel buffer. The bits_per_sample and
 * exponent_bits_per_sample in the JxlBasicInfo describes what will actually be
 * encoded in the JPEG XL codestream. For example, to encode a 12-bit image, you
 * would set bits_per_sample to 12, and you could use e.g. JXL_TYPE_UINT16
 * (where the values are rescaled to 16-bit, i.e. multiplied by 65535/4095) or
 * JXL_TYPE_FLOAT (where the values are rescaled to 0..1, i.e. multiplied
 * by 1.f/4095.f). While it is allowed, it is obviously not recommended to use a
 * pixel_format with lower precision than what is specified in the JxlBasicInfo.
 *
 * We support interleaved channels as described by the JxlPixelFormat:
 * - single-channel data, e.g. grayscale
 * - single-channel + alpha
 * - trichromatic, e.g. RGB
 * - trichromatic + alpha
 *
 * Extra channels not handled here need to be set by @ref
 * JxlEncoderSetExtraChannelBuffer.
 *
 * The color profile of the pixels depends on the value of uses_original_profile
 * in the JxlBasicInfo. If true, the pixels are assumed to be encoded in the
 * original profile that is set with JxlEncoderSetColorEncoding or
 * JxlEncoderSetICCProfile. If false, the pixels are assumed to be nonlinear
 * sRGB for integer data types (JXL_TYPE_UINT8, JXL_TYPE_UINT16), and linear
 * sRGB for floating point data types (JXL_TYPE_FLOAT16, JXL_TYPE_FLOAT).
 * Sample values in floating-point pixel formats are allowed to be outside the
 * nominal range, e.g. to represent out-of-sRGB-gamut colors in the
 * uses_original_profile=false case. They are however not allowed to be NaN or
 * +-infinity.
 *
 * If this is the last frame, @ref JxlEncoderCloseInput or @ref
 * JxlEncoderCloseFrames must be called before the next
 * @ref JxlEncoderProcessOutput call.
 *
 * @param frame_settings set of options and metadata for this frame. Also
 * includes reference to the encoder object.
 * @param pixel_format format for pixels. Object owned by the caller and its
 * contents are copied internally.
 * @param buffer buffer type to input the pixel data from. Owned by the caller
 * and its contents are copied internally.
 * @param size size of buffer in bytes.
 * @return JXL_ENC_SUCCESS on success, JXL_ENC_ERROR on error
 */
JXL_EXPORT JxlEncoderStatus JxlEncoderAddImageFrame(
    const JxlEncoderFrameSettings* frame_settings,
    const JxlPixelFormat* pixel_format, const void* buffer, size_t size);

/**
 * Sets the buffer to read pixels from for an extra channel at a given index.
 * The index must be smaller than the num_extra_channels in the associated
 * JxlBasicInfo. Must call @ref JxlEncoderSetExtraChannelInfo before
 * JxlEncoderSetExtraChannelBuffer.
 *
 * TODO(firsching): mention what data types in pixel formats are supported.
 *
 * It is required to call this function for every extra channel, except for the
 * alpha channel if that was already set through @ref JxlEncoderAddImageFrame.
 *
 * @param frame_settings set of options and metadata for this frame. Also
 * includes reference to the encoder object.
 * @param pixel_format format for pixels. Object owned by the caller and its
 * contents are copied internally. The num_channels value is ignored, since the
 * number of channels for an extra channel is always assumed to be one.
 * @param buffer buffer type to input the pixel data from. Owned by the caller
 * and its contents are copied internally.
 * @param size size of buffer in bytes.
 * @param index index of the extra channel to use.
 * @return JXL_ENC_SUCCESS on success, JXL_ENC_ERROR on error
 */
JXL_EXPORT JxlEncoderStatus JxlEncoderSetExtraChannelBuffer(
    const JxlEncoderFrameSettings* frame_settings,
    const JxlPixelFormat* pixel_format, const void* buffer, size_t size,
    uint32_t index);

/** Adds a metadata box to the file format. JxlEncoderProcessOutput must be used
 * to effectively write the box to the output. @ref JxlEncoderUseBoxes must
 * be enabled before using this function.
 *
 * Background information about the container format and boxes follows here:
 *
 * For users of libjxl, boxes allow inserting application-specific data and
 * metadata (Exif, XML, JUMBF and user defined boxes).
 *
 * The box format follows ISO BMFF and shares features and box types with other
 * image and video formats, including the Exif, XML and JUMBF boxes. The box
 * format for JPEG XL is specified in ISO/IEC 18181-2.
 *
 * Boxes in general don't contain other boxes inside, except a JUMBF superbox.
 * Boxes follow each other sequentially and are byte-aligned. If the container
 * format is used, the JXL stream exists out of 3 or more concatenated boxes.
 * It is also possible to use a direct codestream without boxes, but in that
 * case metadata cannot be added.
 *
 * Each box generally has the following byte structure in the file:
 * - 4 bytes: box size including box header (Big endian. If set to 0, an
 *   8-byte 64-bit size follows instead).
 * - 4 bytes: type, e.g. "JXL " for the signature box, "jxlc" for a codestream
 *   box.
 * - N bytes: box contents.
 *
 * Only the box contents are provided to the contents argument of this function,
 * the encoder encodes the size header itself.
 *
 * Box types are given by 4 characters. A list of known types follows:
 * - "JXL ": mandatory signature box, must come first, 12 bytes long including
 *   the box header
 * - "ftyp": a second mandatory signature box, must come second, 20 bytes long
 *   including the box header
 * - "jxll": A JXL level box. This indicates if the codestream is level 5 or
 *   level 10 compatible. If not present, it is level 5. Level 10 allows more
 *   features such as very high image resolution and bit-depths above 16 bits
 *   per channel. Added automatically by the encoder when
 *   JxlEncoderSetCodestreamLevel is used
 * - "jxlc": a box with the image codestream, in case the codestream is not
 *   split across multiple boxes. The codestream contains the JPEG XL image
 *   itself, including the basic info such as image dimensions, ICC color
 *   profile, and all the pixel data of all the image frames.
 * - "jxlp": a codestream box in case it is split across multiple boxes. The
 *   encoder will automatically do this if necessary. The contents are the same
 *   as in case of a jxlc box, when concatenated.
 * - "Exif": a box with EXIF metadata, can be added by libjxl users, or is
 *   automatically added when needed for JPEG reconstruction. The contents of
 *   this box must be prepended by a 4-byte tiff header offset, which may
 *   be 4 zero bytes.
 * - "XML ": a box with XMP or IPTC metadata, can be added by libjxl users, or
 *   is automatically added when needed for JPEG reconstruction
 * - "jumb": a JUMBF superbox, which can contain boxes with different types of
 *   metadata inside. This box type can be added by the encoder transparently,
 *   and other libraries to create and handle JUMBF content exist.
 * - "brob": a Brotli-compressed box, which otherwise represents an existing
 *   type of box such as Exif or XML. The encoder creates these when enabled and
 *   users of libjxl don't need to create them directly. Some box types are not
 *   allowed to be compressed: any of the signature, jxl* and jbrd boxes.
 * - "jxli": frame index box, can list the keyframes in case of a JXL animation,
 *   allowing the decoder to jump to individual frames more efficiently. This
 *   box type is specified, but not currently supported by the encoder or
 *   decoder.
 * - "jbrd": JPEG reconstruction box, contains the information required to
 *   byte-for-byte losslessly recontruct a JPEG-1 image. The JPEG coefficients
 *   (pixel content) themselves are encoded in the JXL codestream (jxlc or jxlp)
 *   itself. Exif and XMP metadata will be encoded in Exif and XMP boxes. The
 *   jbrd box itself contains information such as the app markers of the JPEG-1
 *   file and everything else required to fit the information together into the
 *   exact original JPEG file. This box is added automatically by the encoder
 *   when needed, and only when JPEG reconstruction is used.
 * - other: other application-specific boxes can be added. Their typename should
 *   not begin with "jxl" or "JXL" or conflict with other existing typenames.
 *
 * Most boxes are automatically added by the encoder and should not be added
 * with JxlEncoderAddBox. Boxes that one may wish to add with JxlEncoderAddBox
 * are: Exif and XML (but not when using JPEG reconstruction since if the
 * JPEG has those, these boxes are already added automatically), jumb, and
 * application-specific boxes.
 *
 * Adding metadata boxes increases the filesize. When adding Exif metadata, the
 * data must be in sync with what is encoded in the JPEG XL codestream,
 * specifically the image orientation. While this is not recommended in
 * practice, in case of conflicting metadata, the JPEG XL codestream takes
 * precedence.
 *
 * It is possible to create a codestream without boxes, then what would be in
 * the jxlc box is written directly to the output
 *
 * It is possible to split the codestream across multiple boxes, in that case
 * multiple boxes of type jxlp are used. This is handled by the encoder when
 * needed.
 *
 * @param enc encoder object.
 * @param type the box type, e.g. "Exif" for EXIF metadata, "XML " for XMP or
 * IPTC metadata, "jumb" for JUMBF metadata.
 * @param contents the full contents of the box, for example EXIF
 * data. For an "Exif" box, the EXIF data must be prepended by a 4-byte tiff
 * header offset, which may be 4 zero-bytes. The ISO BMFF box header must not
 * be included, only the contents. Owned by the caller and its contents are
 * copied internally.
 * @param size size of the box contents.
 * @param compress_box Whether to compress this box as a "brob" box. Requires
 * Brotli support.
 * @return JXL_ENC_SUCCESS on success, JXL_ENC_ERROR on error, such as when
 * using this function without JxlEncoderUseContainer, or adding a box type
 * that would result in an invalid file format.
 */
JXL_EXPORT JxlEncoderStatus JxlEncoderAddBox(JxlEncoder* enc,
                                             const JxlBoxType type,
                                             const uint8_t* contents,
                                             size_t size,
                                             JXL_BOOL compress_box);

/**
 * Indicates the intention to add metadata boxes. This allows @ref
 * JxlEncoderAddBox to be used. When using this function, then it is required
 * to use @ref JxlEncoderCloseBoxes at the end.
 *
 * By default the encoder assumes no metadata boxes will be added.
 *
 * This setting can only be set at the beginning, before encoding starts.
 *
 * @param enc encoder object.
 */
JXL_EXPORT JxlEncoderStatus JxlEncoderUseBoxes(JxlEncoder* enc);

/**
 * Declares that no further boxes will be added with @ref JxlEncoderAddBox.
 * This function must be called after the last box is added so the encoder knows
 * the stream will be finished. It is not necessary to use this function if
 * @ref JxlEncoderUseBoxes is not used. Further frames may still be added.
 *
 * Must be called between JxlEncoderAddBox of the last box
 * and the next call to JxlEncoderProcessOutput, or @ref JxlEncoderProcessOutput
 * won't output the last box correctly.
 *
 * NOTE: if you don't need to close frames and boxes at separate times, you can
 * use @ref JxlEncoderCloseInput instead to close both at once.
 *
 * @param enc encoder object.
 */
JXL_EXPORT void JxlEncoderCloseBoxes(JxlEncoder* enc);

/**
 * Declares that no frames will be added and @ref JxlEncoderAddImageFrame and
 * @ref JxlEncoderAddJPEGFrame won't be called anymore. Further metadata boxes
 * may still be added. This function or @ref JxlEncoderCloseInput must be called
 * after adding the last frame and the next call to
 * @ref JxlEncoderProcessOutput, or the frame won't be properly marked as last.
 *
 * NOTE: if you don't need to close frames and boxes at separate times, you can
 * use @ref JxlEncoderCloseInput instead to close both at once.
 *
 * @param enc encoder object.
 */
JXL_EXPORT void JxlEncoderCloseFrames(JxlEncoder* enc);

/**
 * Closes any input to the encoder, equivalent to calling JxlEncoderCloseFrames
 * as well as calling JxlEncoderCloseBoxes if needed. No further input of any
 * kind may be given to the encoder, but further @ref JxlEncoderProcessOutput
 * calls should be done to create the final output.
 *
 * The requirements of both @ref JxlEncoderCloseFrames and @ref
 * JxlEncoderCloseBoxes apply to this function. Either this function or the
 * other two must be called after the final frame and/or box, and the next
 * @ref JxlEncoderProcessOutput call, or the codestream won't be encoded
 * correctly.
 *
 * @param enc encoder object.
 */
JXL_EXPORT void JxlEncoderCloseInput(JxlEncoder* enc);

/**
 * Sets the original color encoding of the image encoded by this encoder. This
 * is an alternative to JxlEncoderSetICCProfile and only one of these two must
 * be used. This one sets the color encoding as a @ref JxlColorEncoding, while
 * the other sets it as ICC binary data.
 *
 * @param enc encoder object.
 * @param color color encoding. Object owned by the caller and its contents are
 * copied internally.
 * @return JXL_ENC_SUCCESS if the operation was successful, JXL_ENC_ERROR or
 * JXL_ENC_NOT_SUPPORTED otherwise
 */
JXL_EXPORT JxlEncoderStatus
JxlEncoderSetColorEncoding(JxlEncoder* enc, const JxlColorEncoding* color);

/**
 * Sets the original color encoding of the image encoded by this encoder as an
 * ICC color profile. This is an alternative to JxlEncoderSetColorEncoding and
 * only one of these two must be used. This one sets the color encoding as ICC
 * binary data, while the other defines it as a @ref JxlColorEncoding.
 *
 * @param enc encoder object.
 * @param icc_profile bytes of the original ICC profile
 * @param size size of the icc_profile buffer in bytes
 * @return JXL_ENC_SUCCESS if the operation was successful, JXL_ENC_ERROR or
 * JXL_ENC_NOT_SUPPORTED otherwise
 */
JXL_EXPORT JxlEncoderStatus JxlEncoderSetICCProfile(JxlEncoder* enc,
                                                    const uint8_t* icc_profile,
                                                    size_t size);

/**
 * Initializes a JxlBasicInfo struct to default values.
 * For forwards-compatibility, this function has to be called before values
 * are assigned to the struct fields.
 * The default values correspond to an 8-bit RGB image, no alpha or any
 * other extra channels.
 *
 * @param info global image metadata. Object owned by the caller.
 */
JXL_EXPORT void JxlEncoderInitBasicInfo(JxlBasicInfo* info);

/**
 * Initializes a JxlFrameHeader struct to default values.
 * For forwards-compatibility, this function has to be called before values
 * are assigned to the struct fields.
 * The default values correspond to a frame with no animation duration and the
 * 'replace' blend mode. After using this function, For animation duration must
 * be set, for composite still blend settings must be set.
 *
 * @param frame_header frame metadata. Object owned by the caller.
 */
JXL_EXPORT void JxlEncoderInitFrameHeader(JxlFrameHeader* frame_header);

/**
 * Initializes a JxlBlendInfo struct to default values.
 * For forwards-compatibility, this function has to be called before values
 * are assigned to the struct fields.
 *
 * @param blend_info blending info. Object owned by the caller.
 */
JXL_EXPORT void JxlEncoderInitBlendInfo(JxlBlendInfo* blend_info);

/**
 * Sets the global metadata of the image encoded by this encoder.
 *
 * If the JxlBasicInfo contains information of extra channels beyond an alpha
 * channel, then @ref JxlEncoderSetExtraChannelInfo must be called between
 * JxlEncoderSetBasicInfo and @ref JxlEncoderAddImageFrame. In order to indicate
 * extra channels, the value of `info.num_extra_channels` should be set to the
 * number of extra channels, also counting the alpha channel if present.
 *
 * @param enc encoder object.
 * @param info global image metadata. Object owned by the caller and its
 * contents are copied internally.
 * @return JXL_ENC_SUCCESS if the operation was successful,
 * JXL_ENC_ERROR or JXL_ENC_NOT_SUPPORTED otherwise
 */
JXL_EXPORT JxlEncoderStatus JxlEncoderSetBasicInfo(JxlEncoder* enc,
                                                   const JxlBasicInfo* info);

/**
 * Initializes a JxlExtraChannelInfo struct to default values.
 * For forwards-compatibility, this function has to be called before values
 * are assigned to the struct fields.
 * The default values correspond to an 8-bit channel of the provided type.
 *
 * @param type type of the extra channel.
 * @param info global extra channel metadata. Object owned by the caller and its
 * contents are copied internally.
 * @return JXL_ENC_SUCCESS on success, JXL_ENC_ERROR on error
 */
JXL_EXPORT void JxlEncoderInitExtraChannelInfo(JxlExtraChannelType type,
                                               JxlExtraChannelInfo* info);

/**
 * Sets information for the extra channel at the given index. The index
 * must be smaller than num_extra_channels in the associated JxlBasicInfo.
 *
 * @param enc encoder object
 * @param index index of the extra channel to set.
 * @param info global extra channel metadata. Object owned by the caller and its
 * contents are copied internally.
 * @return JXL_ENC_SUCCESS on success, JXL_ENC_ERROR on error
 */
JXL_EXPORT JxlEncoderStatus JxlEncoderSetExtraChannelInfo(
    JxlEncoder* enc, size_t index, const JxlExtraChannelInfo* info);

/**
 * Sets the name for the extra channel at the given index in UTF-8. The index
 * must be smaller than the num_extra_channels in the associated JxlBasicInfo.
 *
 * TODO(lode): remove size parameter for consistency with
 * JxlEncoderSetFrameName
 *
 * @param enc encoder object
 * @param index index of the extra channel to set.
 * @param name buffer with the name of the extra channel.
 * @param size size of the name buffer in bytes, not counting the terminating
 * character.
 * @return JXL_ENC_SUCCESS on success, JXL_ENC_ERROR on error
 */
JXL_EXPORT JxlEncoderStatus JxlEncoderSetExtraChannelName(JxlEncoder* enc,
                                                          size_t index,
                                                          const char* name,
                                                          size_t size);

/**
 * Sets a frame-specific option of integer type to the encoder options.
 * The JxlEncoderFrameSettingId argument determines which option is set.
 *
 * @param frame_settings set of options and metadata for this frame. Also
 * includes reference to the encoder object.
 * @param option ID of the option to set.
 * @param value Integer value to set for this option.
 * @return JXL_ENC_SUCCESS if the operation was successful, JXL_ENC_ERROR in
 * case of an error, such as invalid or unknown option id, or invalid integer
 * value for the given option. If an error is returned, the state of the
 * JxlEncoderFrameSettings object is still valid and is the same as before this
 * function was called.
 */
JXL_EXPORT JxlEncoderStatus JxlEncoderFrameSettingsSetOption(
    JxlEncoderFrameSettings* frame_settings, JxlEncoderFrameSettingId option,
    int32_t value);

/** Forces the encoder to use the box-based container format (BMFF) even
 * when not necessary.
 *
 * When using @ref JxlEncoderUseBoxes, @ref JxlEncoderStoreJPEGMetadata or @ref
 * JxlEncoderSetCodestreamLevel with level 10, the encoder will automatically
 * also use the container format, it is not necessary to use
 * JxlEncoderUseContainer for those use cases.
 *
 * By default this setting is disabled.
 *
 * This setting can only be set at the beginning, before encoding starts.
 *
 * @param enc encoder object.
 * @param use_container true if the encoder should always output the JPEG XL
 * container format, false to only output it when necessary.
 * @return JXL_ENC_SUCCESS if the operation was successful, JXL_ENC_ERROR
 * otherwise.
 */
JXL_EXPORT JxlEncoderStatus JxlEncoderUseContainer(JxlEncoder* enc,
                                                   JXL_BOOL use_container);

/**
 * Configure the encoder to store JPEG reconstruction metadata in the JPEG XL
 * container.
 *
 * If this is set to true and a single JPEG frame is added, it will be
 * possible to losslessly reconstruct the JPEG codestream.
 *
 * This setting can only be set at the beginning, before encoding starts.
 *
 * @param enc encoder object.
 * @param store_jpeg_metadata true if the encoder should store JPEG metadata.
 * @return JXL_ENC_SUCCESS if the operation was successful, JXL_ENC_ERROR
 * otherwise.
 */
JXL_EXPORT JxlEncoderStatus
JxlEncoderStoreJPEGMetadata(JxlEncoder* enc, JXL_BOOL store_jpeg_metadata);

/** Sets the feature level of the JPEG XL codestream. Valid values are 5 and
 * 10. Keeping the default value of 5 is recommended for compatibility with all
 * decoders.
 *
 * Level 5: for end-user image delivery, this level is the most widely
 * supported level by image decoders and the recommended level to use unless a
 * level 10 feature is absolutely necessary. Supports a maximum resolution
 * 268435456 pixels total with a maximum width or height of 262144 pixels,
 * maximum 16-bit color channel depth, maximum 120 frames per second for
 * animation, maximum ICC color profile size of 4 MiB, it allows all color
 * models and extra channel types except CMYK and the JXL_CHANNEL_BLACK extra
 * channel, and a maximum of 4 extra channels in addition to the 3 color
 * channels. It also sets boundaries to certain internally used coding tools.
 *
 * Level 10: this level removes or increases the bounds of most of the level
 * 5 limitations, allows CMYK color and up to 32 bits per color channel, but
 * may be less widely supported.
 *
 * The default value is 5. To use level 10 features, the setting must be
 * explicitly set to 10, the encoder will not automatically enable it. If
 * incompatible parameters such as too high image resolution for the current
 * level are set, the encoder will return an error. For internal coding tools,
 * the encoder will only use those compatible with the level setting.
 *
 * This setting can only be set at the beginning, before encoding starts.
 *
 * @param enc encoder object.
 * @param level the level value to set, must be 5 or 10.
 * @return JXL_ENC_SUCCESS if the operation was successful, JXL_ENC_ERROR
 * otherwise.
 */
JXL_EXPORT JxlEncoderStatus JxlEncoderSetCodestreamLevel(JxlEncoder* enc,
                                                         int level);

/** Returns the codestream level required to support the currently configured
 * settings and basic info. This function can only be used at the beginning,
 * before encoding starts, but after setting basic info.
 *
 * This does not support per-frame settings, only global configuration, such as
 * the image dimensions, that are known at the time of writing the header of
 * the JPEG XL file.
 *
 * If this returns 5, nothing needs to be done and the codestream can be
 * compatible with any decoder. If this returns 10, JxlEncoderSetCodestreamLevel
 * has to be used to set the codestream level to 10, or the encoder can be
 * configured differently to allow using the more compatible level 5.
 *
 * @param enc encoder object.
 * @return -1 if no level can support the configuration (e.g. image dimensions
 * larger than even level 10 supports), 5 if level 5 is supported, 10 if setting
 * the codestream level to 10 is required.
 *
 */
JXL_EXPORT int JxlEncoderGetRequiredCodestreamLevel(const JxlEncoder* enc);

/**
 * Enables lossless encoding.
 *
 * This is not an option like the others on itself, but rather while enabled it
 * overrides a set of existing options (such as distance, modular mode and
 * color transform) that enables bit-for-bit lossless encoding.
 *
 * When disabled, those options are not overridden, but since those options
 * could still have been manually set to a combination that operates losslessly,
 * using this function with lossless set to JXL_DEC_FALSE does not guarantee
 * lossy encoding, though the default set of options is lossy.
 *
 * @param frame_settings set of options and metadata for this frame. Also
 * includes reference to the encoder object.
 * @param lossless whether to override options for lossless mode
 * @return JXL_ENC_SUCCESS if the operation was successful, JXL_ENC_ERROR
 * otherwise.
 */
JXL_EXPORT JxlEncoderStatus JxlEncoderSetFrameLossless(
    JxlEncoderFrameSettings* frame_settings, JXL_BOOL lossless);

/** DEPRECATED: use JxlEncoderSetFrameLossless instead.
 */
JXL_EXPORT JxlEncoderStatus
JxlEncoderOptionsSetLossless(JxlEncoderFrameSettings*, JXL_BOOL);

/**
 * @param frame_settings set of options and metadata for this frame. Also
 * includes reference to the encoder object.
 * @param effort the effort value to set.
 * @return JXL_ENC_SUCCESS if the operation was successful, JXL_ENC_ERROR
 * otherwise.
 *
 * DEPRECATED: use JxlEncoderFrameSettingsSetOption(frame_settings,
 * JXL_ENC_FRAME_SETTING_EFFORT, effort) instead.
 */
JXL_EXPORT JXL_DEPRECATED JxlEncoderStatus
JxlEncoderOptionsSetEffort(JxlEncoderFrameSettings* frame_settings, int effort);

/**
 * @param frame_settings set of options and metadata for this frame. Also
 * includes reference to the encoder object.
 * @param tier the decoding speed tier to set.
 * @return JXL_ENC_SUCCESS if the operation was successful, JXL_ENC_ERROR
 * otherwise.
 *
 * DEPRECATED: use JxlEncoderFrameSettingsSetOption(frame_settings,
 * JXL_ENC_FRAME_SETTING_DECODING_SPEED, tier) instead.
 */
JXL_EXPORT JXL_DEPRECATED JxlEncoderStatus JxlEncoderOptionsSetDecodingSpeed(
    JxlEncoderFrameSettings* frame_settings, int tier);

/**
 * Sets the distance level for lossy compression: target max butteraugli
 * distance, lower = higher quality. Range: 0 .. 15.
 * 0.0 = mathematically lossless (however, use JxlEncoderSetFrameLossless
 * instead to use true lossless, as setting distance to 0 alone is not the only
 * requirement). 1.0 = visually lossless. Recommended range: 0.5 .. 3.0. Default
 * value: 1.0.
 *
 * @param frame_settings set of options and metadata for this frame. Also
 * includes reference to the encoder object.
 * @param distance the distance value to set.
 * @return JXL_ENC_SUCCESS if the operation was successful, JXL_ENC_ERROR
 * otherwise.
 */
JXL_EXPORT JxlEncoderStatus JxlEncoderSetFrameDistance(
    JxlEncoderFrameSettings* frame_settings, float distance);

/** DEPRECATED: use JxlEncoderSetFrameDistance instead.
 */
JXL_EXPORT JxlEncoderStatus
JxlEncoderOptionsSetDistance(JxlEncoderFrameSettings*, float);

/**
 * Create a new set of encoder options, with all values initially copied from
 * the @p source options, or set to default if @p source is NULL.
 *
 * The returned pointer is an opaque struct tied to the encoder and it will be
 * deallocated by the encoder when JxlEncoderDestroy() is called. For functions
 * taking both a @ref JxlEncoder and a @ref JxlEncoderFrameSettings, only
 * JxlEncoderFrameSettings created with this function for the same encoder
 * instance can be used.
 *
 * @param enc encoder object.
 * @param source source options to copy initial values from, or NULL to get
 * defaults initialized to defaults.
 * @return the opaque struct pointer identifying a new set of encoder options.
 */
JXL_EXPORT JxlEncoderFrameSettings* JxlEncoderFrameSettingsCreate(
    JxlEncoder* enc, const JxlEncoderFrameSettings* source);

/** DEPRECATED: use JxlEncoderFrameSettingsCreate instead.
 */
JXL_EXPORT JxlEncoderFrameSettings* JxlEncoderOptionsCreate(
    JxlEncoder*, const JxlEncoderFrameSettings*);

/**
 * Sets a color encoding to be sRGB.
 *
 * @param color_encoding color encoding instance.
 * @param is_gray whether the color encoding should be gray scale or color.
 */
JXL_EXPORT void JxlColorEncodingSetToSRGB(JxlColorEncoding* color_encoding,
                                          JXL_BOOL is_gray);

/**
 * Sets a color encoding to be linear sRGB.
 *
 * @param color_encoding color encoding instance.
 * @param is_gray whether the color encoding should be gray scale or color.
 */
JXL_EXPORT void JxlColorEncodingSetToLinearSRGB(
    JxlColorEncoding* color_encoding, JXL_BOOL is_gray);

#if defined(__cplusplus) || defined(c_plusplus)
}
#endif

#endif /* JXL_ENCODE_H_ */

/** @}*/

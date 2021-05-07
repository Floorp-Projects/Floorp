/* Copyright (c) the JPEG XL Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/** @file decode.h
 * @brief Decoding API for JPEG XL.
 */

#ifndef JXL_DECODE_H_
#define JXL_DECODE_H_

#include <stddef.h>
#include <stdint.h>

#include "jxl/codestream_header.h"
#include "jxl/color_encoding.h"
#include "jxl/jxl_export.h"
#include "jxl/memory_manager.h"
#include "jxl/parallel_runner.h"
#include "jxl/types.h"

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

/**
 * Decoder library version.
 *
 * @return the decoder library version as an integer:
 * MAJOR_VERSION * 1000000 + MINOR_VERSION * 1000 + PATCH_VERSION. For example,
 * version 1.2.3 would return 1002003.
 */
JXL_EXPORT uint32_t JxlDecoderVersion(void);

/** The result of JxlSignatureCheck.
 */
typedef enum {
  /** Not enough bytes were passed to determine if a valid signature was found.
   */
  JXL_SIG_NOT_ENOUGH_BYTES = 0,

  /** No valid JPEGXL header was found. */
  JXL_SIG_INVALID = 1,

  /** A valid JPEG XL codestream signature was found, that is a JPEG XL image
   * without container.
   */
  JXL_SIG_CODESTREAM = 2,

  /** A valid container signature was found, that is a JPEG XL image embedded
   * in a box format container.
   */
  JXL_SIG_CONTAINER = 3,
} JxlSignature;

/**
 * JPEG XL signature identification.
 *
 * Checks if the passed buffer contains a valid JPEG XL signature. The passed @p
 * buf of size
 * @p size doesn't need to be a full image, only the beginning of the file.
 *
 * @return a flag indicating if a JPEG XL signature was found and what type.
 *   - JXL_SIG_NOT_ENOUGH_BYTES not enough bytes were passed to determine
 *       if a valid signature is there.
 *   - JXL_SIG_INVALID: no valid signature found for JPEG XL decoding.
 *   - JXL_SIG_CODESTREAM a valid JPEG XL codestream signature was found.
 *   - JXL_SIG_CONTAINER a valid JPEG XL container signature was found.
 */
JXL_EXPORT JxlSignature JxlSignatureCheck(const uint8_t* buf, size_t len);

/**
 * Opaque structure that holds the JPEGXL decoder.
 *
 * Allocated and initialized with JxlDecoderCreate().
 * Cleaned up and deallocated with JxlDecoderDestroy().
 */
typedef struct JxlDecoderStruct JxlDecoder;

/**
 * Creates an instance of JxlDecoder and initializes it.
 *
 * @p memory_manager will be used for all the library dynamic allocations made
 * from this instance. The parameter may be NULL, in which case the default
 * allocator will be used. See jpegxl/memory_manager.h for details.
 *
 * @param memory_manager custom allocator function. It may be NULL. The memory
 *        manager will be copied internally.
 * @return @c NULL if the instance can not be allocated or initialized
 * @return pointer to initialized JxlDecoder otherwise
 */
JXL_EXPORT JxlDecoder* JxlDecoderCreate(const JxlMemoryManager* memory_manager);

/**
 * Re-initializes a JxlDecoder instance, so it can be re-used for decoding
 * another image. All state and settings are reset as if the object was
 * newly created with JxlDecoderCreate, but the memory manager is kept.
 *
 * @param dec instance to be re-initialized.
 */
JXL_EXPORT void JxlDecoderReset(JxlDecoder* dec);

/**
 * Deinitializes and frees JxlDecoder instance.
 *
 * @param dec instance to be cleaned up and deallocated.
 */
JXL_EXPORT void JxlDecoderDestroy(JxlDecoder* dec);

/**
 * Return value for JxlDecoderProcessInput.
 * The values above 0x40 are optional informal events that can be subscribed to,
 * they are never returned if they have not been registered with
 * JxlDecoderSubscribeEvents.
 */
typedef enum {
  /** Function call finished successfully, or decoding is finished and there is
   * nothing more to be done.
   */
  JXL_DEC_SUCCESS = 0,

  /** An error occured, for example invalid input file or out of memory.
   * TODO(lode): add function to get error information from decoder.
   */
  JXL_DEC_ERROR = 1,

  /** The decoder needs more input bytes to continue. In the next
   * JxlDecoderProcessInput call, next_in and avail_in must point to more
   * bytes to continue. If *avail_in is not 0, the new bytes must be appended to
   * the *avail_in last previous bytes.
   */
  JXL_DEC_NEED_MORE_INPUT = 2,

  /** The decoder is able to decode a preview image and requests setting a
   * preview output buffer using JxlDecoderSetPreviewOutBuffer. This occurs if
   * JXL_DEC_PREVIEW_IMAGE is requested and it is possible to decode a preview
   * image from the codestream and the preview out buffer was not yet set. There
   * is maximum one preview image in a codestream.
   */
  JXL_DEC_NEED_PREVIEW_OUT_BUFFER = 3,

  /** The decoder is able to decode a DC image and requests setting a DC output
   * buffer using JxlDecoderSetDCOutBuffer. This occurs if JXL_DEC_DC_IMAGE is
   * requested and it is possible to decode a DC image from the codestream and
   * the DC out buffer was not yet set. This event re-occurs for new frames
   * if there are multiple animation frames.
   */
  JXL_DEC_NEED_DC_OUT_BUFFER = 4,

  /** The decoder requests an output buffer to store the full resolution image,
   * which can be set with JxlDecoderSetImageOutBuffer or with
   * JxlDecoderSetImageOutCallback. This event re-occurs for new frames if there
   * are multiple animation frames and requires setting an output again.
   */
  JXL_DEC_NEED_IMAGE_OUT_BUFFER = 5,

  /** Informative event by JxlDecoderProcessInput: JPEG reconstruction buffer is
   * too small for reconstructed JPEG codestream to fit.
   * JxlDecoderSetJPEGBuffer must be called again to make room for remaining
   * bytes. This event may occur multiple times after
   * JXL_DEC_JPEG_RECONSTRUCTION
   */
  JXL_DEC_JPEG_NEED_MORE_OUTPUT = 6,

  /** Informative event by JxlDecoderProcessInput: basic information such as
   * image dimensions and extra channels. This event occurs max once per image.
   */
  JXL_DEC_BASIC_INFO = 0x40,

  /** Informative event by JxlDecoderProcessInput: user extensions of the
   * codestream header. This event occurs max once per image and always later
   * than JXL_DEC_BASIC_INFO and earlier than any pixel data.
   */
  JXL_DEC_EXTENSIONS = 0x80,

  /** Informative event by JxlDecoderProcessInput: color encoding or ICC
   * profile from the codestream header. This event occurs max once per image
   * and always later than JXL_DEC_BASIC_INFO and earlier than any pixel
   * data.
   */
  JXL_DEC_COLOR_ENCODING = 0x100,

  /** Informative event by JxlDecoderProcessInput: Preview image, a small
   * frame, decoded. This event can only happen if the image has a preview
   * frame encoded. This event occurs max once for the codestream and always
   * later than JXL_DEC_COLOR_ENCODING and before JXL_DEC_FRAME.
   * This event is different than JXL_DEC_PREVIEW_HEADER because the latter only
   * outputs the dimensions of the preview image.
   */
  JXL_DEC_PREVIEW_IMAGE = 0x200,

  /** Informative event by JxlDecoderProcessInput: Beginning of a frame.
   * JxlDecoderGetFrameHeader can be used at this point. A note on frames:
   * a JPEG XL image can have internal frames that are not intended to be
   * displayed (e.g. used for compositing a final frame), but this only returns
   * displayed frames. A displayed frame either has an animation duration or is
   * the only or last frame in the image. This event occurs max once per
   * displayed frame, always later than JXL_DEC_COLOR_ENCODING, and always
   * earlier than any pixel data. While JPEG XL supports encoding a single frame
   * as the composition of multiple internal sub-frames also called frames, this
   * event is not indicated for the internal frames.
   */
  JXL_DEC_FRAME = 0x400,

  /** Informative event by JxlDecoderProcessInput: DC image, 8x8 sub-sampled
   * frame, decoded. It is not guaranteed that the decoder will always return DC
   * separately, but when it does it will do so before outputting the full
   * frame. JxlDecoderSetDCOutBuffer must be used after getting the basic
   * image information to be able to get the DC pixels, if not this return
   * status only indicates we're past this point in the codestream. This event
   * occurs max once per frame and always later than JXL_DEC_FRAME_HEADER
   * and other header events and earlier than full resolution pixel data.
   */
  JXL_DEC_DC_IMAGE = 0x800,

  /** Informative event by JxlDecoderProcessInput: full frame decoded.
   * JxlDecoderSetImageOutBuffer must be used after getting the basic image
   * information to be able to get the image pixels, if not this return status
   * only indicates we're past this point in the codestream. This event occurs
   * max once per frame and always later than JXL_DEC_DC_IMAGE.
   */
  JXL_DEC_FULL_IMAGE = 0x1000,

  /** Informative event by JxlDecoderProcessInput: JPEG reconstruction data
   * decoded. JxlDecoderSetJPEGBuffer may be used to set a JPEG
   * reconstruction buffer after getting the JPEG reconstruction data. If a JPEG
   * reconstruction buffer is set a byte stream identical to the JPEG codestream
   * used to encode the image will be written to the JPEG reconstruction buffer
   * instead of pixels to the image out buffer. This event occurs max once per
   * image and always before JXL_DEC_FULL_IMAGE.
   */
  JXL_DEC_JPEG_RECONSTRUCTION = 0x2000,
} JxlDecoderStatus;

/**
 * Get the default pixel format for this decoder.
 *
 * Requires that the decoder can produce JxlBasicInfo.
 *
 * @param dec JxlDecoder to query when creating the recommended pixel format.
 * @param format JxlPixelFormat to populate with the recommended settings for
 * the data loaded into this decoder.
 * @return JXL_DEC_SUCCESS if no error, JXL_DEC_NEED_MORE_INPUT if the
 * basic info isn't yet available, and JXL_DEC_ERROR otherwise.
 */
JXL_EXPORT JxlDecoderStatus
JxlDecoderDefaultPixelFormat(const JxlDecoder* dec, JxlPixelFormat* format);

/**
 * Set the parallel runner for multithreading. May only be set before starting
 * decoding.
 *
 * @param dec decoder object
 * @param parallel_runner function pointer to runner for multithreading. It may
 *        be NULL to use the default, single-threaded, runner. A multithreaded
 *        runner should be set to reach fast performance.
 * @param parallel_runner_opaque opaque pointer for parallel_runner.
 * @return JXL_DEC_SUCCESS if the runner was set, JXL_DEC_ERROR
 * otherwise (the previous runner remains set).
 */
JXL_EXPORT JxlDecoderStatus
JxlDecoderSetParallelRunner(JxlDecoder* dec, JxlParallelRunner parallel_runner,
                            void* parallel_runner_opaque);

/**
 * Returns a hint indicating how many more bytes the decoder is expected to
 * need to make JxlDecoderGetBasicInfo available after the next
 * JxlDecoderProcessInput call. This is a suggested large enough value for
 * the *avail_in parameter, but it is not guaranteed to be an upper bound nor
 * a lower bound.
 * Can be used before the first JxlDecoderProcessInput call, and is correct
 * the first time in most cases. If not, JxlDecoderSizeHintBasicInfo can be
 * called again to get an updated hint.
 *
 * @param dec decoder object
 * @return the size hint in bytes if the basic info is not yet fully decoded.
 * @return 0 when the basic info is already available.
 */
JXL_EXPORT size_t JxlDecoderSizeHintBasicInfo(const JxlDecoder* dec);

/** Select for which informative events (JXL_DEC_BASIC_INFO, etc...) the
 * decoder should return with a status. It is not required to subscribe to any
 * events, data can still be requested from the decoder as soon as it available.
 * By default, the decoder is subscribed to no events (events_wanted == 0), and
 * the decoder will then only return when it cannot continue because it needs
 * more input data or more output buffer. This function may only be be called
 * before using JxlDecoderProcessInput
 *
 * @param dec decoder object
 * @param events_wanted bitfield of desired events.
 * @return JXL_DEC_SUCCESS if no error, JXL_DEC_ERROR otherwise.
 */
JXL_EXPORT JxlDecoderStatus JxlDecoderSubscribeEvents(JxlDecoder* dec,
                                                      int events_wanted);

/** Enables or disables preserving of original orientation. Some images are
 * encoded with an orientation tag indicating the image is rotated and/or
 * mirrored (here called the original orientation).
 *
 * *) If keep_orientation is JXL_FALSE (the default): the decoder will perform
 * work to undo the transformation. This ensures the decoded pixels will not
 * be rotated or mirrored. The decoder will always set the orientation field
 * of the JxlBasicInfo to JXL_ORIENT_IDENTITY to match the returned pixel data.
 * The decoder may also swap xsize and ysize in the JxlBasicInfo compared to the
 * values inside of the codestream, to correctly match the decoded pixel data,
 * e.g. when a 90 degree rotation was performed.
 *
 * *) If this option is JXL_TRUE: then the image is returned as-is, which may be
 * rotated or mirrored, and the user must check the orientation field in
 * JxlBasicInfo after decoding to correctly interpret the decoded pixel data.
 * This may be faster to decode since the decoder doesn't have to apply the
 * transformation, but can cause wrong display of the image if the orientation
 * tag is not correctly taken into account by the user.
 *
 * By default, this option is disabled, and the decoder automatically corrects
 * the orientation.
 *
 * @see JxlBasicInfo for the orientation field, and @see JxlOrientation for the
 * possible values.
 *
 * @param dec decoder object
 * @param keep_orientation JXL_TRUE to enable, JXL_FALSE to disable.
 * @return JXL_DEC_SUCCESS if no error, JXL_DEC_ERROR otherwise.
 */
JXL_EXPORT JxlDecoderStatus
JxlDecoderSetKeepOrientation(JxlDecoder* dec, JXL_BOOL keep_orientation);

/**
 * Decodes JPEG XL file using the available bytes. Requires input has been
 * set with JxlDecoderSetInput. After JxlDecoderProcessInput, input can
 * optionally be released with JxlDecoderReleaseInput and then set again to
 * next bytes in the stream. JxlDecoderReleaseInput returns how many bytes are
 * not yet processed, before a next call to JxlDecoderProcessInput all
 * unprocessed bytes must be provided again (the address need not match, but the
 * contents must), and more bytes may be concatenated after the unprocessed
 * bytes.
 *
 * The returned status indicates whether the decoder needs more input bytes, or
 * more output buffer for a certain type of output data. No matter what the
 * returned status is (other than JXL_DEC_ERROR), new information, such as
 * JxlDecoderGetBasicInfo, may have become available after this call. When
 * the return value is not JXL_DEC_ERROR or JXL_DEC_SUCCESS, the decoding
 * requires more JxlDecoderProcessInput calls to continue.
 *
 * @param dec decoder object
 * @return JXL_DEC_SUCCESS when decoding finished and all events handled.
 * @return JXL_DEC_ERROR when decoding failed, e.g. invalid codestream.
 * TODO(lode) document the input data mechanism
 * @return JXL_DEC_NEED_MORE_INPUT more input data is necessary.
 * @return JXL_DEC_BASIC_INFO when basic info such as image dimensions is
 * available and this informative event is subscribed to.
 * @return JXL_DEC_EXTENSIONS when JPEG XL codestream user extensions are
 * available and this informative event is subscribed to.
 * @return JXL_DEC_COLOR_ENCODING when color profile information is
 * available and this informative event is subscribed to.
 * @return JXL_DEC_PREVIEW_IMAGE when preview pixel information is available and
 * output in the preview buffer.
 * @return JXL_DEC_DC_IMAGE when DC pixel information (8x8 downscaled version
 * of the image) is available and output in the DC buffer.
 * @return JXL_DEC_FULL_IMAGE when all pixel information at highest detail is
 * available and has been output in the pixel buffer.
 */
JXL_EXPORT JxlDecoderStatus JxlDecoderProcessInput(JxlDecoder* dec);

/**
 * Sets input data for JxlDecoderProcessInput. The data is owned by the caller
 * and may be used by the decoder until JxlDecoderReleaseInput is called or
 * the decoder is destroyed or reset so must be kept alive until then.
 * @param dec decoder object
 * @param data pointer to next bytes to read from
 * @param size amount of bytes available starting from data
 * @return JXL_DEC_ERROR if input was already set without releasing,
 * JXL_DEC_SUCCESS otherwise
 */
JXL_EXPORT JxlDecoderStatus JxlDecoderSetInput(JxlDecoder* dec,
                                               const uint8_t* data,
                                               size_t size);

/**
 * Releases input which was provided with JxlDecoderSetInput. Between
 * JxlDecoderProcessInput and JxlDecoderReleaseInput, the user may not alter
 * the data in the buffer. Calling JxlDecoderReleaseInput is required whenever
 * any input is already set and new input needs to be added with
 * JxlDecoderSetInput, but is not required before JxlDecoderDestroy or
 * JxlDecoderReset. Calling JxlDecoderReleaseInput when no input is set is
 * not an error and returns 0.
 * @param dec decoder object
 * @return the amount of bytes the decoder has not yet processed that are
 * still remaining in the data set by JxlDecoderSetInput, or 0 if no input is
 * set or JxlDecoderReleaseInput was already called. For a next call to
 * JxlDecoderProcessInput, the buffer must start with these unprocessed bytes.
 * This value doesn't provide information about how many bytes the decoder
 * truly processed internally or how large the original JPEG XL codestream or
 * file are.
 */
JXL_EXPORT size_t JxlDecoderReleaseInput(JxlDecoder* dec);

/**
 * Outputs the basic image information, such as image dimensions, bit depth and
 * all other JxlBasicInfo fields, if available.
 *
 * @param dec decoder object
 * @param info struct to copy the information into, or NULL to only check
 * whether the information is available through the return value.
 * @return JXL_DEC_SUCCESS if the value is available,
 *    JXL_DEC_NEED_MORE_INPUT if not yet available, JXL_DEC_ERROR in case
 *    of other error conditions.
 */
JXL_EXPORT JxlDecoderStatus JxlDecoderGetBasicInfo(const JxlDecoder* dec,
                                                   JxlBasicInfo* info);

/**
 * Outputs information for extra channel at the given index. The index must be
 * smaller than num_extra_channels in the associated JxlBasicInfo.
 *
 * @param dec decoder object
 * @param index index of the extra channel to query.
 * @param info struct to copy the information into, or NULL to only check
 * whether the information is available through the return value.
 * @return JXL_DEC_SUCCESS if the value is available,
 *    JXL_DEC_NEED_MORE_INPUT if not yet available, JXL_DEC_ERROR in case
 *    of other error conditions.
 */
JXL_EXPORT JxlDecoderStatus JxlDecoderGetExtraChannelInfo(
    const JxlDecoder* dec, size_t index, JxlExtraChannelInfo* info);

/**
 * Outputs name for extra channel at the given index in UTF-8. The index must be
 * smaller than num_extra_channels in the associated JxlBasicInfo. The buffer
 * for name must have at least name_length + 1 bytes allocated, gotten from
 * the associated JxlExtraChannelInfo.
 *
 * @param dec decoder object
 * @param index index of the extra channel to query.
 * @param name buffer to copy the name into
 * @param size size of the name buffer in bytes
 * @return JXL_DEC_SUCCESS if the value is available,
 *    JXL_DEC_NEED_MORE_INPUT if not yet available, JXL_DEC_ERROR in case
 *    of other error conditions.
 */
JXL_EXPORT JxlDecoderStatus JxlDecoderGetExtraChannelName(const JxlDecoder* dec,
                                                          size_t index,
                                                          char* name,
                                                          size_t size);

/** Defines which color profile to get: the profile from the codestream
 * metadata header, which represents the color profile of the original image,
 * or the color profile from the pixel data received by the decoder. Both are
 * the same if the basic has uses_original_profile set.
 */
typedef enum {
  /** Get the color profile of the original image from the metadata..
   */
  JXL_COLOR_PROFILE_TARGET_ORIGINAL = 0,

  /** Get the color profile of the pixel data the decoder outputs. */
  JXL_COLOR_PROFILE_TARGET_DATA = 1,
} JxlColorProfileTarget;

/**
 * Outputs the color profile as JPEG XL encoded structured data, if available.
 * This is an alternative to an ICC Profile, which can represent a more limited
 * amount of color spaces, but represents them exactly through enum values.
 *
 * It is often possible to use JxlDecoderGetColorAsICCProfile as an
 * alternative anyway. The following scenarios are possible:
 * - The JPEG XL image has an attached ICC Profile, in that case, the encoded
 *   structured data is not available, this function will return an error status
 *   and you must use JxlDecoderGetColorAsICCProfile instead.
 * - The JPEG XL image has an encoded structured color profile, and it
 *   represents an RGB or grayscale color space. This function will return it.
 *   You can still use JxlDecoderGetColorAsICCProfile as well as an
 *   alternative if desired, though depending on which RGB color space is
 *   represented, the ICC profile may be a close approximation. It is also not
 *   always feasible to deduce from an ICC profile which named color space it
 *   exactly represents, if any, as it can represent any arbitrary space.
 * - The JPEG XL image has an encoded structured color profile, and it indicates
 *   an unknown or xyb color space. In that case,
 *   JxlDecoderGetColorAsICCProfile is not available.
 *
 * If you wish to render the image using a system that supports ICC profiles,
 * use JxlDecoderGetColorAsICCProfile first. If you're looking for a specific
 * color space possibly indicated in the JPEG XL image, use
 * JxlDecoderGetColorAsEncodedProfile first.
 *
 * @param dec decoder object
 * @param format pixel format to output the data to. Only used for
 * JXL_COLOR_PROFILE_TARGET_DATA, may be nullptr otherwise.
 * @param target whether to get the original color profile from the metadata
 *     or the color profile of the decoded pixels.
 * @param color_encoding struct to copy the information into, or NULL to only
 * check whether the information is available through the return value.
 * @return JXL_DEC_SUCCESS if the data is available and returned,
 *    JXL_DEC_NEED_MORE_INPUT if not yet available, JXL_DEC_ERROR in case
 *    the encuded structured color profile does not exist in the codestream.
 */
JXL_EXPORT JxlDecoderStatus JxlDecoderGetColorAsEncodedProfile(
    const JxlDecoder* dec, const JxlPixelFormat* format,
    JxlColorProfileTarget target, JxlColorEncoding* color_encoding);

/**
 * Outputs the size in bytes of the ICC profile returned by
 * JxlDecoderGetColorAsICCProfile, if available, or indicates there is none
 * available. In most cases, the image will have an ICC profile available, but
 * if it does not, JxlDecoderGetColorAsEncodedProfile must be used instead.
 * @see JxlDecoderGetColorAsEncodedProfile for more information. The ICC
 * profile is either the exact ICC profile attached to the codestream metadata,
 * or a close approximation generated from JPEG XL encoded structured data,
 * depending of what is encoded in the codestream.
 *
 * @param dec decoder object
 * @param format pixel format to output the data to. Only used for
 * JXL_COLOR_PROFILE_TARGET_DATA, may be nullptr otherwise.
 * @param target whether to get the original color profile from the metadata
 *     or the color profile of the decoded pixels.
 * @param size variable to output the size into, or NULL to only check the
 *    return status.
 * @return JXL_DEC_SUCCESS if the ICC profile is available,
 *    JXL_DEC_NEED_MORE_INPUT if the decoder has not yet received enough
 *    input data to determine whether an ICC profile is available or what its
 *    size is, JXL_DEC_ERROR in case the ICC profile is not available and
 *    cannot be generated.
 */
JXL_EXPORT JxlDecoderStatus
JxlDecoderGetICCProfileSize(const JxlDecoder* dec, const JxlPixelFormat* format,
                            JxlColorProfileTarget target, size_t* size);

/**
 * Outputs ICC profile if available. The profile is only available if
 * JxlDecoderGetICCProfileSize returns success. The output buffer must have
 * at least as many bytes as given by JxlDecoderGetICCProfileSize.
 *
 * @param dec decoder object
 * @param format pixel format to output the data to. Only used for
 * JXL_COLOR_PROFILE_TARGET_DATA, may be nullptr otherwise.
 * @param target whether to get the original color profile from the metadata
 *     or the color profile of the decoded pixels.
 * @param icc_profile buffer to copy the ICC profile into
 * @param size size of the icc_profile buffer in bytes
 * @return JXL_DEC_SUCCESS if the profile was successfully returned is
 *    available, JXL_DEC_NEED_MORE_INPUT if not yet available,
 *    JXL_DEC_ERROR if the profile doesn't exist or the output size is not
 *    large enough.
 */
JXL_EXPORT JxlDecoderStatus JxlDecoderGetColorAsICCProfile(
    const JxlDecoder* dec, const JxlPixelFormat* format,
    JxlColorProfileTarget target, uint8_t* icc_profile, size_t size);

/**
 * Returns the minimum size in bytes of the preview image output pixel buffer
 * for the given format. This is the buffer for JxlDecoderSetPreviewOutBuffer.
 * Requires the preview header information is available in the decoder.
 *
 * @param dec decoder object
 * @param format format of pixels
 * @param size output value, buffer size in bytes
 * @return JXL_DEC_SUCCESS on success, JXL_DEC_ERROR on error, such as
 *    information not available yet.
 */
JXL_EXPORT JxlDecoderStatus JxlDecoderPreviewOutBufferSize(
    const JxlDecoder* dec, const JxlPixelFormat* format, size_t* size);

/**
 * Sets the buffer to write the small resolution preview image
 * to. The size of the buffer must be at least as large as given by
 * JxlDecoderPreviewOutBufferSize. The buffer follows the format described by
 * JxlPixelFormat. The preview image dimensions are given by the
 * JxlPreviewHeader. The buffer is owned by the caller.
 *
 * @param dec decoder object
 * @param format format of pixels. Object owned by user and its contents are
 * copied internally.
 * @param buffer buffer type to output the pixel data to
 * @param size size of buffer in bytes
 * @return JXL_DEC_SUCCESS on success, JXL_DEC_ERROR on error, such as
 * size too small.
 */
JXL_EXPORT JxlDecoderStatus JxlDecoderSetPreviewOutBuffer(
    JxlDecoder* dec, const JxlPixelFormat* format, void* buffer, size_t size);

/**
 * Outputs the information from the frame, such as duration when have_animation.
 * This function can be called when JXL_DEC_FRAME occurred for the current
 * frame, even when have_animation in the JxlBasicInfo is JXL_FALSE.
 *
 * @param dec decoder object
 * @param header struct to copy the information into, or NULL to only check
 * whether the information is available through the return value.
 * @return JXL_DEC_SUCCESS if the value is available,
 *    JXL_DEC_NEED_MORE_INPUT if not yet available, JXL_DEC_ERROR in case
 *    of other error conditions.
 */
JXL_EXPORT JxlDecoderStatus JxlDecoderGetFrameHeader(const JxlDecoder* dec,
                                                     JxlFrameHeader* header);

/**
 * Outputs name for the current frame. The buffer
 * for name must have at least name_length + 1 bytes allocated, gotten from
 * the associated JxlFrameHeader.
 *
 * @param dec decoder object
 * @param name buffer to copy the name into
 * @param size size of the name buffer in bytes, includig zero termination
 *    character, so this must be at least JxlFrameHeader.name_length + 1.
 * @return JXL_DEC_SUCCESS if the value is available,
 *    JXL_DEC_NEED_MORE_INPUT if not yet available, JXL_DEC_ERROR in case
 *    of other error conditions.
 */
JXL_EXPORT JxlDecoderStatus JxlDecoderGetFrameName(const JxlDecoder* dec,
                                                   char* name, size_t size);

/**
 * Returns the minimum size in bytes of the DC image output buffer
 * for the given format. This is the buffer for JxlDecoderSetDCOutBuffer.
 * Requires the basic image information is available in the decoder.
 *
 * @param dec decoder object
 * @param format format of pixels
 * @param size output value, buffer size in bytes
 * @return JXL_DEC_SUCCESS on success, JXL_DEC_ERROR on error, such as
 *    information not available yet.
 */
JXL_EXPORT JxlDecoderStatus JxlDecoderDCOutBufferSize(
    const JxlDecoder* dec, const JxlPixelFormat* format, size_t* size);

/**
 * Sets the buffer to write the lower resolution (8x8 sub-sampled) DC image
 * to. The size of the buffer must be at least as large as given by
 * JxlDecoderDCOutBufferSize. The buffer follows the format described by
 * JxlPixelFormat. The DC image has dimensions ceil(xsize / 8) * ceil(ysize /
 * 8). The buffer is owned by the caller.
 *
 * @param dec decoder object
 * @param format format of pixels. Object owned by user and its contents are
 * copied internally.
 * @param buffer buffer type to output the pixel data to
 * @param size size of buffer in bytes
 * @return JXL_DEC_SUCCESS on success, JXL_DEC_ERROR on error, such as
 * size too small.
 */
JXL_EXPORT JxlDecoderStatus JxlDecoderSetDCOutBuffer(
    JxlDecoder* dec, const JxlPixelFormat* format, void* buffer, size_t size);

/**
 * Returns the minimum size in bytes of the image output pixel buffer for the
 * given format. This is the buffer for JxlDecoderSetImageOutBuffer. Requires
 * the basic image information is available in the decoder.
 *
 * @param dec decoder object
 * @param format format of the pixels.
 * @param size output value, buffer size in bytes
 * @return JXL_DEC_SUCCESS on success, JXL_DEC_ERROR on error, such as
 *    information not available yet.
 */
JXL_EXPORT JxlDecoderStatus JxlDecoderImageOutBufferSize(
    const JxlDecoder* dec, const JxlPixelFormat* format, size_t* size);

/**
 * Sets output buffer for reconstructed JPEG codestream.
 *
 * The data is owned by the caller
 * and may be used by the decoder until JxlDecoderReleaseJPEGBuffer is called or
 * the decoder is destroyed or reset so must be kept alive until then.
 *
 * @param dec decoder object
 * @param data pointer to next bytes to write to
 * @param size amount of bytes available starting from data
 * @return JXL_DEC_ERROR if input was already set without releasing,
 * JXL_DEC_SUCCESS otherwise
 */
JXL_EXPORT JxlDecoderStatus JxlDecoderSetJPEGBuffer(JxlDecoder* dec,
                                                    uint8_t* data, size_t size);

/**
 * Releases buffer which was provided with JxlDecoderSetJPEGBuffer.
 *
 * Calling JxlDecoderReleaseJPEGBuffer is required whenever
 * a buffer is already set and a new buffer needs to be added with
 * JxlDecoderSetJPEGBuffer, but is not required before JxlDecoderDestroy or
 * JxlDecoderReset.
 *
 * Calling JxlDecoderReleaseJPEGBuffer when no input is set is
 * not an error and returns 0.
 *
 * @param dec decoder object
 * @return the amount of bytes the decoder has not yet written to of the data
 * set by JxlDecoderSetJPEGBuffer, or 0 if no buffer is set or
 * JxlDecoderReleaseJPEGBuffer was already called.
 */
JXL_EXPORT size_t JxlDecoderReleaseJPEGBuffer(JxlDecoder* dec);

/**
 * Sets the buffer to write the full resolution image to. This can be set when
 * the JXL_DEC_FRAME event occurs, must be set when the
 * JXL_DEC_NEED_IMAGE_OUT_BUFFER event occurs, and applies only for the current
 * frame. The size of the buffer must be at least as large as given by
 * JxlDecoderImageOutBufferSize. The buffer follows the format described by
 * JxlPixelFormat. The buffer is owned by the caller.
 *
 * @param dec decoder object
 * @param format format of the pixels. Object owned by user and its contents
 * are copied internally.
 * @param buffer buffer type to output the pixel data to
 * @param size size of buffer in bytes
 * @return JXL_DEC_SUCCESS on success, JXL_DEC_ERROR on error, such as
 * size too small.
 */
JXL_EXPORT JxlDecoderStatus JxlDecoderSetImageOutBuffer(
    JxlDecoder* dec, const JxlPixelFormat* format, void* buffer, size_t size);

/**
 * Callback function type for JxlDecoderSetImageOutCallback. @see
 * JxlDecoderSetImageOutCallback for usage.
 *
 * The callback bay be called simultaneously by different threads when using a
 * threaded parallel runner, on different pixels.
 *
 * @param opaque optional user data, as given to JxlDecoderSetImageOutCallback.
 * @param x horizontal position of leftmost pixel of the pixel data.
 * @param y vertical position of the pixel data.
 * @param num_pixels amount of pixels included in the pixel data, horizontally.
 * This is not the same as xsize of the full image, it may be smaller.
 * @param pixels pixel data as a horizontal stripe, in the format passed to
 * JxlDecoderSetImageOutCallback. The memory is not owned by the user, and is
 * only valid during the time the callback is running.
 */
typedef void (*JxlImageOutCallback)(void* opaque, size_t x, size_t y,
                                    size_t num_pixels, const void* pixels);

/**
 * Sets pixel output callback. This is an alternative to
 * JxlDecoderSetImageOutBuffer. This can be set when the JXL_DEC_FRAME event
 * occurs, must be set when the JXL_DEC_NEED_IMAGE_OUT_BUFFER event occurs, and
 * applies only for the current frame. Only one of JxlDecoderSetImageOutBuffer
 * or JxlDecoderSetImageOutCallback may be used for the same frame, not both at
 * the same time.
 *
 * The callback will be called multiple times, to receive the image
 * data in small chunks. The callback receives a horizontal stripe of pixel
 * data, 1 pixel high, xsize pixels wide, called a scanline. The xsize here is
 * not the same as the full image width, the scanline may be a partial section,
 * and xsize may differ between calls. The user can then process and/or copy the
 * partial scanline to an image buffer. The callback bay be called
 * simultaneously by different threads when using a threaded parallel runner, on
 * different pixels.
 *
 * If JxlDecoderFlushImage is not used, then each pixel will be visited exactly
 * once by the different callback calls, during processing with one or more
 * JxlDecoderProcessInput calls. These pixels are decoded to full detail, they
 * are not part of a lower resolution or lower quality progressive pass, but the
 * final pass.
 *
 * If JxlDecoderFlushImage is used, then in addition each pixel will be visited
 * zero or one times during the blocking JxlDecoderFlushImage call. Pixels
 * visited as a result of JxlDecoderFlushImage may represent a lower resolution
 * or lower quality intermediate progressive pass of the image. Any visited
 * pixel will be of a quality at least as good or better than previous visits of
 * this pixel. A pixel may be visited zero times if it cannot be decoded yet
 * or if it was already decoded to full precision (this behavior is not
 * guaranteed).
 *
 * @param dec decoder object
 * @param format format of the pixels. Object owned by user and its contents
 * are copied internally.
 * @param callback the callback function receiving partial scanlines of pixel
 * data.
 * @param opaque optional user data, which will be passed on to the callback,
 * may be NULL.
 * @return JXL_DEC_SUCCESS on success, JXL_DEC_ERROR on error, such as
 * JxlDecoderSetImageOutBuffer already set.
 */
JXL_EXPORT JxlDecoderStatus
JxlDecoderSetImageOutCallback(JxlDecoder* dec, const JxlPixelFormat* format,
                              JxlImageOutCallback callback, void* opaque);

/* TODO(lode): add way to output extra channels */

/**
 * Outputs progressive step towards the decoded image so far when only partial
 * input was received. If the flush was successful, the buffer set with
 * JxlDecoderSetImageOutBuffer will contain partial image data.
 *
 * Can be called when JxlDecoderProcessInput returns JXL_DEC_NEED_MORE_INPUT,
 * after the JXL_DEC_FRAME event already occured and before the
 * JXL_DEC_FULL_IMAGE event occured for a frame.
 *
 * @param dec decoder object
 * @return JXL_DEC_SUCCESS if image data was flushed to the output buffer, or
 * JXL_DEC_ERROR when no flush was done, e.g. if not enough image data was
 * available yet even for flush, or no output buffer was set yet. An error is
 * not fatal, it only indicates no flushed image is available now, regular,
 *  decoding can still be performed.
 */
JXL_EXPORT JxlDecoderStatus JxlDecoderFlushImage(JxlDecoder* dec);

#if defined(__cplusplus) || defined(c_plusplus)
}
#endif

#endif /* JXL_DECODE_H_ */

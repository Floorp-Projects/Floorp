/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Stuart Parmenter <stuart@mozilla.com>
 *   Andrew Smith
 *   Federico Mena-Quintero <federico@novell.com>
 *   Bobby Holley <bobbyholley@gmail.com>
 *   Glenn Randers-Pehrson <glennrp@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsPNGDecoder.h"

#include "nsMemory.h"
#include "nsRect.h"

#include "nsIInputStream.h"

#include "RasterImage.h"
#include "imgIContainerObserver.h"

#include "gfxColor.h"
#include "nsColor.h"

#include "nspr.h"
#include "png.h"

#include "gfxPlatform.h"

namespace mozilla {
namespace imagelib {

#ifdef PR_LOGGING
static PRLogModuleInfo *gPNGLog = PR_NewLogModule("PNGDecoder");
static PRLogModuleInfo *gPNGDecoderAccountingLog =
                        PR_NewLogModule("PNGDecoderAccounting");
#endif

/* limit image dimensions (bug #251381) */
#define MOZ_PNG_MAX_DIMENSION 1000000L

// For size decodes
#define WIDTH_OFFSET 16
#define HEIGHT_OFFSET (WIDTH_OFFSET + 4)
#define BYTES_NEEDED_FOR_DIMENSIONS (HEIGHT_OFFSET + 4)

// This is defined in the PNG spec as an invariant. We use it to
// do manual validation without libpng.
static const PRUint8 pngSignatureBytes[] =
               { 137, 80, 78, 71, 13, 10, 26, 10 };

nsPNGDecoder::nsPNGDecoder() :
  mPNG(nsnull), mInfo(nsnull),
  mCMSLine(nsnull), interlacebuf(nsnull),
  mInProfile(nsnull), mTransform(nsnull),
  mHeaderBuf(nsnull), mHeaderBytesRead(0),
  mChannels(0), mFrameIsHidden(PR_FALSE),
  mNotifiedDone(PR_FALSE)
{
}

nsPNGDecoder::~nsPNGDecoder()
{
  if (mPNG)
    png_destroy_read_struct(&mPNG, mInfo ? &mInfo : NULL, NULL);
  if (mCMSLine)
    nsMemory::Free(mCMSLine);
  if (interlacebuf)
    nsMemory::Free(interlacebuf);
  if (mInProfile) {
    qcms_profile_release(mInProfile);

    /* mTransform belongs to us only if mInProfile is non-null */
    if (mTransform)
      qcms_transform_release(mTransform);
  }
  if (mHeaderBuf)
    nsMemory::Free(mHeaderBuf);
}

// CreateFrame() is used for both simple and animated images
void nsPNGDecoder::CreateFrame(png_uint_32 x_offset, png_uint_32 y_offset,
                               PRInt32 width, PRInt32 height,
                               gfxASurface::gfxImageFormat format)
{
  PRUint32 imageDataLength;
  nsresult rv = mImage->AppendFrame(x_offset, y_offset, width, height, format,
                                    &mImageData, &imageDataLength);
  if (NS_FAILED(rv))
    longjmp(png_jmpbuf(mPNG), 5); // NS_ERROR_OUT_OF_MEMORY

  mFrameRect.x = x_offset;
  mFrameRect.y = y_offset;
  mFrameRect.width = width;
  mFrameRect.height = height;

#ifdef PNG_APNG_SUPPORTED
  if (png_get_valid(mPNG, mInfo, PNG_INFO_acTL))
    SetAnimFrameInfo();
#endif

  // Tell the superclass we're starting a frame
  PostFrameStart();

  PR_LOG(gPNGDecoderAccountingLog, PR_LOG_DEBUG,
         ("PNGDecoderAccounting: nsPNGDecoder::CreateFrame -- created "
          "image frame with %dx%d pixels in container %p",
          width, height,
          mImage.get ()));

  mFrameHasNoAlpha = PR_TRUE;
}

#ifdef PNG_APNG_SUPPORTED
// set timeout and frame disposal method for the current frame
void nsPNGDecoder::SetAnimFrameInfo()
{
  png_uint_16 delay_num, delay_den;
  /* delay, in seconds is delay_num/delay_den */
  png_byte dispose_op;
  png_byte blend_op;
  PRInt32 timeout; /* in milliseconds */

  delay_num = png_get_next_frame_delay_num(mPNG, mInfo);
  delay_den = png_get_next_frame_delay_den(mPNG, mInfo);
  dispose_op = png_get_next_frame_dispose_op(mPNG, mInfo);
  blend_op = png_get_next_frame_blend_op(mPNG, mInfo);

  if (delay_num == 0) {
    timeout = 0; // SetFrameTimeout() will set to a minimum
  } else {
    if (delay_den == 0)
      delay_den = 100; // so says the APNG spec

    // Need to cast delay_num to float to have a proper division and
    // the result to int to avoid compiler warning
    timeout = static_cast<PRInt32>
              (static_cast<PRFloat64>(delay_num) * 1000 / delay_den);
  }

  PRUint32 numFrames = mImage->GetNumFrames();

  mImage->SetFrameTimeout(numFrames - 1, timeout);

  if (dispose_op == PNG_DISPOSE_OP_PREVIOUS)
      mImage->SetFrameDisposalMethod(numFrames - 1,
                                     RasterImage::kDisposeRestorePrevious);
  else if (dispose_op == PNG_DISPOSE_OP_BACKGROUND)
      mImage->SetFrameDisposalMethod(numFrames - 1,
                                     RasterImage::kDisposeClear);
  else
      mImage->SetFrameDisposalMethod(numFrames - 1,
                                     RasterImage::kDisposeKeep);

  if (blend_op == PNG_BLEND_OP_SOURCE)
      mImage->SetFrameBlendMethod(numFrames - 1, RasterImage::kBlendSource);
  /*else // 'over' is the default
      mImage->SetFrameBlendMethod(numFrames - 1, RasterImage::kBlendOver); */
}
#endif

// set timeout and frame disposal method for the current frame
void nsPNGDecoder::EndImageFrame()
{
  PRUint32 numFrames = 1;
#ifdef PNG_APNG_SUPPORTED
  numFrames = mImage->GetNumFrames();

  // We can't use mPNG->num_frames_read as it may be one ahead.
  if (numFrames > 1) {
    // Tell the image renderer that the frame is complete
    if (mFrameHasNoAlpha)
      mImage->SetFrameHasNoAlpha(numFrames - 1);

    PostInvalidation(mFrameRect);
  }
#endif

  PostFrameStop();
}

void
nsPNGDecoder::InitInternal()
{

#ifdef PNG_HANDLE_AS_UNKNOWN_SUPPORTED
  static png_byte color_chunks[]=
       { 99,  72,  82,  77, '\0',   /* cHRM */
        105,  67,  67,  80, '\0'};  /* iCCP */
  static png_byte unused_chunks[]=
       { 98,  75,  71,  68, '\0',   /* bKGD */
        104,  73,  83,  84, '\0',   /* hIST */
        105,  84,  88, 116, '\0',   /* iTXt */
        111,  70,  70, 115, '\0',   /* oFFs */
        112,  67,  65,  76, '\0',   /* pCAL */
        115,  67,  65,  76, '\0',   /* sCAL */
        112,  72,  89, 115, '\0',   /* pHYs */
        115,  66,  73,  84, '\0',   /* sBIT */
        115,  80,  76,  84, '\0',   /* sPLT */
        116,  69,  88, 116, '\0',   /* tEXt */
        116,  73,  77,  69, '\0',   /* tIME */
        122,  84,  88, 116, '\0'};  /* zTXt */
#endif

  // For size decodes, we only need a small buffer
  if (IsSizeDecode()) {
    mHeaderBuf = (PRUint8 *)nsMemory::Alloc(BYTES_NEEDED_FOR_DIMENSIONS);
    if (!mHeaderBuf) {
      PostDecoderError(NS_ERROR_OUT_OF_MEMORY);
      return;
    }
    return;
  }

  /* For full decodes, do png init stuff */

  /* Initialize the container's source image header. */
  /* Always decode to 24 bit pixdepth */

  mPNG = png_create_read_struct(PNG_LIBPNG_VER_STRING,
                                NULL, nsPNGDecoder::error_callback,
                                nsPNGDecoder::warning_callback);
  if (!mPNG) {
    PostDecoderError(NS_ERROR_OUT_OF_MEMORY);
    return;
  }

  mInfo = png_create_info_struct(mPNG);
  if (!mInfo) {
    PostDecoderError(NS_ERROR_OUT_OF_MEMORY);
    png_destroy_read_struct(&mPNG, NULL, NULL);
    return;
  }

#ifdef PNG_HANDLE_AS_UNKNOWN_SUPPORTED
  /* Ignore unused chunks */
  if (gfxPlatform::GetCMSMode() == eCMSMode_Off)
    png_set_keep_unknown_chunks(mPNG, 1, color_chunks, 2);

  png_set_keep_unknown_chunks(mPNG, 1, unused_chunks,
                              (int)sizeof(unused_chunks)/5);   
#endif

#ifdef PNG_SET_CHUNK_MALLOC_LIMIT_SUPPORTED
  if (gfxPlatform::GetCMSMode() != eCMSMode_Off)
    png_set_chunk_malloc_max(mPNG, 4000000L);
#endif

  /* use this as libpng "progressive pointer" (retrieve in callbacks) */
  png_set_progressive_read_fn(mPNG, static_cast<png_voidp>(this),
                              nsPNGDecoder::info_callback,
                              nsPNGDecoder::row_callback,
                              nsPNGDecoder::end_callback);

}

void
nsPNGDecoder::FinishInternal()
{

  // If we're a full/success decode but haven't sent stop notifications yet,
  // we didn't get all the data we needed. Send error notifications.
  if (!IsSizeDecode() && !mNotifiedDone)
    NotifyDone(/* aSuccess = */ PR_FALSE);
}

void
nsPNGDecoder::WriteInternal(const char *aBuffer, PRUint32 aCount)
{
  // We use gotos, so we need to declare variables here
  PRUint32 width = 0;
  PRUint32 height = 0;

  // No forgiveness if we previously hit an error
  if (IsError())
    return;

  // If we only want width/height, we don't need to go through libpng
  if (IsSizeDecode()) {

    // Are we done?
    if (mHeaderBytesRead == BYTES_NEEDED_FOR_DIMENSIONS)
      return;

    // Read data into our header buffer
    PRUint32 bytesToRead = PR_MIN(aCount, BYTES_NEEDED_FOR_DIMENSIONS -
                                  mHeaderBytesRead);
    memcpy(mHeaderBuf + mHeaderBytesRead, aBuffer, bytesToRead);
    mHeaderBytesRead += bytesToRead;

    // If we're done now, verify the data and set up the container
    if (mHeaderBytesRead == BYTES_NEEDED_FOR_DIMENSIONS) {

      // Check that the signature bytes are right
      if (memcmp(mHeaderBuf, pngSignatureBytes, sizeof(pngSignatureBytes))) {
        PostDataError();
        return;
      }

      // Grab the width and height, accounting for endianness (thanks libpng!)
      width = png_get_uint_32(mHeaderBuf + WIDTH_OFFSET);
      height = png_get_uint_32(mHeaderBuf + HEIGHT_OFFSET);

      // Too big?
      if ((width > MOZ_PNG_MAX_DIMENSION) || (height > MOZ_PNG_MAX_DIMENSION)) {
        PostDataError();
        return;
      }

      // Post our size to the superclass
      PostSize(width, height);
    }
  }

  // Otherwise, we're doing a standard decode
  else {

    // libpng uses setjmp/longjmp for error handling - set the buffer
    if (setjmp(png_jmpbuf(mPNG))) {

      // We might not really know what caused the error, but it makes more
      // sense to blame the data.
      if (!IsError())
        PostDataError();

      png_destroy_read_struct(&mPNG, &mInfo, NULL);
      return;
    }

    // Pass the data off to libpng
    png_process_data(mPNG, mInfo, (unsigned char *)aBuffer, aCount);

  }
}

void
nsPNGDecoder::NotifyDone(PRBool aSuccess)
{
  // We should only be called once
  NS_ABORT_IF_FALSE(!mNotifiedDone, "Calling NotifyDone twice!");

  // Notify
  if (!mFrameIsHidden)
    EndImageFrame();
  if (aSuccess)
    mImage->DecodingComplete();
  if (mObserver) {
    mObserver->OnStopContainer(nsnull, mImage);
    mObserver->OnStopDecode(nsnull, aSuccess ? NS_OK : NS_ERROR_FAILURE,
                            nsnull);
  }

  // Mark that we've been called
  mNotifiedDone = PR_TRUE;
}

// Sets up gamma pre-correction in libpng before our callback gets called.
// We need to do this if we don't end up with a CMS profile.
static void
PNGDoGammaCorrection(png_structp png_ptr, png_infop info_ptr)
{
  double aGamma;

  if (png_get_gAMA(png_ptr, info_ptr, &aGamma)) {
    if ((aGamma <= 0.0) || (aGamma > 21474.83)) {
      aGamma = 0.45455;
      png_set_gAMA(png_ptr, info_ptr, aGamma);
    }
    png_set_gamma(png_ptr, 2.2, aGamma);
  }
  else
    png_set_gamma(png_ptr, 2.2, 0.45455);

}

// Adapted from http://www.littlecms.com/pngchrm.c example code
static qcms_profile *
PNGGetColorProfile(png_structp png_ptr, png_infop info_ptr,
                   int color_type, qcms_data_type *inType, PRUint32 *intent)
{
  qcms_profile *profile = nsnull;
  *intent = QCMS_INTENT_PERCEPTUAL; // Our default

  // First try to see if iCCP chunk is present
  if (png_get_valid(png_ptr, info_ptr, PNG_INFO_iCCP)) {
    png_uint_32 profileLen;
    char *profileData, *profileName;
    int compression;

    png_get_iCCP(png_ptr, info_ptr, &profileName, &compression,
                 &profileData, &profileLen);

    profile = qcms_profile_from_memory(profileData, profileLen);
    if (profile) {
      PRUint32 profileSpace = qcms_profile_get_color_space(profile);

      PRBool mismatch = PR_FALSE;
      if (color_type & PNG_COLOR_MASK_COLOR) {
        if (profileSpace != icSigRgbData)
          mismatch = PR_TRUE;
      } else {
        if (profileSpace == icSigRgbData)
          png_set_gray_to_rgb(png_ptr);
        else if (profileSpace != icSigGrayData)
          mismatch = PR_TRUE;
      }

      if (mismatch) {
        qcms_profile_release(profile);
        profile = nsnull;
      } else {
        *intent = qcms_profile_get_rendering_intent(profile);
      }
    }
  }

  // Check sRGB chunk
  if (!profile && png_get_valid(png_ptr, info_ptr, PNG_INFO_sRGB)) {
    profile = qcms_profile_sRGB();

    if (profile) {
      int fileIntent;
      png_set_gray_to_rgb(png_ptr);
      png_get_sRGB(png_ptr, info_ptr, &fileIntent);
      PRUint32 map[] = { QCMS_INTENT_PERCEPTUAL,
                         QCMS_INTENT_RELATIVE_COLORIMETRIC,
                         QCMS_INTENT_SATURATION,
                         QCMS_INTENT_ABSOLUTE_COLORIMETRIC };
      *intent = map[fileIntent];
    }
  }

  // Check gAMA/cHRM chunks
  if (!profile &&
       png_get_valid(png_ptr, info_ptr, PNG_INFO_gAMA) &&
       png_get_valid(png_ptr, info_ptr, PNG_INFO_cHRM)) {
    qcms_CIE_xyYTRIPLE primaries;
    qcms_CIE_xyY whitePoint;

    png_get_cHRM(png_ptr, info_ptr,
                 &whitePoint.x, &whitePoint.y,
                 &primaries.red.x,   &primaries.red.y,
                 &primaries.green.x, &primaries.green.y,
                 &primaries.blue.x,  &primaries.blue.y);
    whitePoint.Y =
      primaries.red.Y = primaries.green.Y = primaries.blue.Y = 1.0;

    double gammaOfFile;

    png_get_gAMA(png_ptr, info_ptr, &gammaOfFile);

    profile = qcms_profile_create_rgb_with_gamma(whitePoint, primaries,
                                                 1.0/gammaOfFile);

    if (profile)
      png_set_gray_to_rgb(png_ptr);
  }

  if (profile) {
    PRUint32 profileSpace = qcms_profile_get_color_space(profile);
    if (profileSpace == icSigGrayData) {
      if (color_type & PNG_COLOR_MASK_ALPHA)
        *inType = QCMS_DATA_GRAYA_8;
      else
        *inType = QCMS_DATA_GRAY_8;
    } else {
      if (color_type & PNG_COLOR_MASK_ALPHA ||
          png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS))
        *inType = QCMS_DATA_RGBA_8;
      else
        *inType = QCMS_DATA_RGB_8;
    }
  }

  return profile;
}

void
nsPNGDecoder::info_callback(png_structp png_ptr, png_infop info_ptr)
{
/*  int number_passes;   NOT USED  */
  png_uint_32 width, height;
  int bit_depth, color_type, interlace_type, compression_type, filter_type;
  unsigned int channels;

  png_bytep trans = NULL;
  int num_trans = 0;

  nsPNGDecoder *decoder =
               static_cast<nsPNGDecoder*>(png_get_progressive_ptr(png_ptr));

  /* always decode to 24-bit RGB or 32-bit RGBA  */
  png_get_IHDR(png_ptr, info_ptr, &width, &height, &bit_depth, &color_type,
               &interlace_type, &compression_type, &filter_type);

  /* Are we too big? */
  if (width > MOZ_PNG_MAX_DIMENSION || height > MOZ_PNG_MAX_DIMENSION)
    longjmp(png_jmpbuf(decoder->mPNG), 1);

  // Post our size to the superclass
  decoder->PostSize(width, height);

  if (color_type == PNG_COLOR_TYPE_PALETTE)
    png_set_expand(png_ptr);

  if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8)
    png_set_expand(png_ptr);

  if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS)) {
    int sample_max = (1 << bit_depth);
    png_color_16p trans_values;
    png_get_tRNS(png_ptr, info_ptr, &trans, &num_trans, &trans_values);
    /* libpng doesn't reject a tRNS chunk with out-of-range samples
       so we check it here to avoid setting up a useless opacity
       channel or producing unexpected transparent pixels when using
       libpng-1.2.19 through 1.2.26 (bug #428045) */
    if ((color_type == PNG_COLOR_TYPE_GRAY &&
       (int)trans_values->gray > sample_max) ||
       (color_type == PNG_COLOR_TYPE_RGB &&
       ((int)trans_values->red > sample_max ||
       (int)trans_values->green > sample_max ||
       (int)trans_values->blue > sample_max)))
      {
        /* clear the tRNS valid flag and release tRNS memory */
        png_free_data(png_ptr, info_ptr, PNG_FREE_TRNS, 0);
      }
    else
      png_set_expand(png_ptr);
  }

  if (bit_depth == 16)
    png_set_strip_16(png_ptr);

  qcms_data_type inType;
  PRUint32 intent = -1;
  PRUint32 pIntent;
  if (gfxPlatform::GetCMSMode() != eCMSMode_Off) {
    intent = gfxPlatform::GetRenderingIntent();
    decoder->mInProfile = PNGGetColorProfile(png_ptr, info_ptr,
                                             color_type, &inType, &pIntent);
    /* If we're not mandating an intent, use the one from the image. */
    if (intent == PRUint32(-1))
      intent = pIntent;
  }
  if (decoder->mInProfile && gfxPlatform::GetCMSOutputProfile()) {
    qcms_data_type outType;

    if (color_type & PNG_COLOR_MASK_ALPHA || num_trans)
      outType = QCMS_DATA_RGBA_8;
    else
      outType = QCMS_DATA_RGB_8;

    decoder->mTransform = qcms_transform_create(decoder->mInProfile,
                                           inType,
                                           gfxPlatform::GetCMSOutputProfile(),
                                           outType,
                                           (qcms_intent)intent);
  } else {
    png_set_gray_to_rgb(png_ptr);
    PNGDoGammaCorrection(png_ptr, info_ptr);

    if (gfxPlatform::GetCMSMode() == eCMSMode_All) {
      if (color_type & PNG_COLOR_MASK_ALPHA || num_trans)
        decoder->mTransform = gfxPlatform::GetCMSRGBATransform();
      else
        decoder->mTransform = gfxPlatform::GetCMSRGBTransform();
    }
  }

  /* let libpng expand interlaced images */
  if (interlace_type == PNG_INTERLACE_ADAM7) {
    /* number_passes = */
    png_set_interlace_handling(png_ptr);
  }

  /* now all of those things we set above are used to update various struct
   * members and whatnot, after which we can get channels, rowbytes, etc. */
  png_read_update_info(png_ptr, info_ptr);
  decoder->mChannels = channels = png_get_channels(png_ptr, info_ptr);

  /*---------------------------------------------------------------*/
  /* copy PNG info into imagelib structs (formerly png_set_dims()) */
  /*---------------------------------------------------------------*/

  PRInt32 alpha_bits = 1;

  if (channels == 2 || channels == 4) {
    /* check if alpha is coming from a tRNS chunk and is binary */
    if (num_trans) {
      /* if it's not an indexed color image, tRNS means binary */
      if (color_type == PNG_COLOR_TYPE_PALETTE) {
        for (int i=0; i<num_trans; i++) {
          if ((trans[i] != 0) && (trans[i] != 255)) {
            alpha_bits = 8;
            break;
          }
        }
      }
    } else {
      alpha_bits = 8;
    }
  }

  if (channels == 1 || channels == 3)
    decoder->format = gfxASurface::ImageFormatRGB24;
  else if (channels == 2 || channels == 4)
    decoder->format = gfxASurface::ImageFormatARGB32;

#ifdef PNG_APNG_SUPPORTED
  if (png_get_valid(png_ptr, info_ptr, PNG_INFO_acTL))
    png_set_progressive_frame_fn(png_ptr, nsPNGDecoder::frame_info_callback, NULL);

  if (png_get_first_frame_is_hidden(png_ptr, info_ptr)) {
    decoder->mFrameIsHidden = PR_TRUE;
  } else {
#endif
    decoder->CreateFrame(0, 0, width, height, decoder->format);
#ifdef PNG_APNG_SUPPORTED
  }
#endif

  if (decoder->mTransform &&
      (channels <= 2 || interlace_type == PNG_INTERLACE_ADAM7)) {
    PRUint32 bpp[] = { 0, 3, 4, 3, 4 };
    decoder->mCMSLine =
      (PRUint8 *)nsMemory::Alloc(bpp[channels] * width);
    if (!decoder->mCMSLine) {
      longjmp(png_jmpbuf(decoder->mPNG), 5); // NS_ERROR_OUT_OF_MEMORY
    }
  }

  if (interlace_type == PNG_INTERLACE_ADAM7) {
    if (height < PR_INT32_MAX / (width * channels))
      decoder->interlacebuf = (PRUint8 *)nsMemory::Alloc(channels *
                                                         width * height);
    if (!decoder->interlacebuf) {
      longjmp(png_jmpbuf(decoder->mPNG), 5); // NS_ERROR_OUT_OF_MEMORY
    }
  }

  /* Reject any ancillary chunk after IDAT with a bad CRC (bug #397593).
   * It would be better to show the default frame (if one has already been
   * successfully decoded) before bailing, but it's simpler to just bail
   * out with an error message.
   */
  png_set_crc_action(png_ptr, NULL, PNG_CRC_ERROR_QUIT);

  return;
}

void
nsPNGDecoder::row_callback(png_structp png_ptr, png_bytep new_row,
                           png_uint_32 row_num, int pass)
{
  /* libpng comments:
   *
   * this function is called for every row in the image.  If the
   * image is interlacing, and you turned on the interlace handler,
   * this function will be called for every row in every pass.
   * Some of these rows will not be changed from the previous pass.
   * When the row is not changed, the new_row variable will be NULL.
   * The rows and passes are called in order, so you don't really
   * need the row_num and pass, but I'm supplying them because it
   * may make your life easier.
   *
   * For the non-NULL rows of interlaced images, you must call
   * png_progressive_combine_row() passing in the row and the
   * old row.  You can call this function for NULL rows (it will
   * just return) and for non-interlaced images (it just does the
   * memcpy for you) if it will make the code easier.  Thus, you
   * can just do this for all cases:
   *
   *    png_progressive_combine_row(png_ptr, old_row, new_row);
   *
   * where old_row is what was displayed for previous rows.  Note
   * that the first pass (pass == 0 really) will completely cover
   * the old row, so the rows do not have to be initialized.  After
   * the first pass (and only for interlaced images), you will have
   * to pass the current row, and the function will combine the
   * old row and the new row.
   */
  nsPNGDecoder *decoder =
               static_cast<nsPNGDecoder*>(png_get_progressive_ptr(png_ptr));

  // skip this frame
  if (decoder->mFrameIsHidden)
    return;

  if (row_num >= (png_uint_32) decoder->mFrameRect.height)
    return;

  if (new_row) {
    PRInt32 width = decoder->mFrameRect.width;
    PRUint32 iwidth = decoder->mFrameRect.width;

    png_bytep line = new_row;
    if (decoder->interlacebuf) {
      line = decoder->interlacebuf + (row_num * decoder->mChannels * width);
      png_progressive_combine_row(png_ptr, line, new_row);
    }

    PRUint32 bpr = width * sizeof(PRUint32);
    PRUint32 *cptr32 = (PRUint32*)(decoder->mImageData + (row_num*bpr));
    PRBool rowHasNoAlpha = PR_TRUE;

    if (decoder->mTransform) {
      if (decoder->mCMSLine) {
        qcms_transform_data(decoder->mTransform, line, decoder->mCMSLine,
                            iwidth);
        /* copy alpha over */
        PRUint32 channels = decoder->mChannels;
        if (channels == 2 || channels == 4) {
          for (PRUint32 i = 0; i < iwidth; i++)
            decoder->mCMSLine[4 * i + 3] = line[channels * i + channels - 1];
        }
        line = decoder->mCMSLine;
      } else {
        qcms_transform_data(decoder->mTransform, line, line, iwidth);
       }
     }

    switch (decoder->format) {
      case gfxASurface::ImageFormatRGB24:
      {
        // counter for while() loops below
        PRUint32 idx = iwidth;

        // copy as bytes until source pointer is 32-bit-aligned
        for (; (NS_PTR_TO_UINT32(line) & 0x3) && idx; --idx) {
          *cptr32++ = GFX_PACKED_PIXEL(0xFF, line[0], line[1], line[2]);
          line += 3;
        }

        // copy pixels in blocks of 4
        while (idx >= 4) {
          GFX_BLOCK_RGB_TO_FRGB(line, cptr32);
          idx    -=  4;
          line   += 12;
          cptr32 +=  4;
        }

        // copy remaining pixel(s)
        while (idx--) {
          // 32-bit read of final pixel will exceed buffer, so read bytes
          *cptr32++ = GFX_PACKED_PIXEL(0xFF, line[0], line[1], line[2]);
          line += 3;
        }
      }
      break;
      case gfxASurface::ImageFormatARGB32:
      {
        for (PRUint32 x=width; x>0; --x) {
          *cptr32++ = GFX_PACKED_PIXEL(line[3], line[0], line[1], line[2]);
          if (line[3] != 0xff)
            rowHasNoAlpha = PR_FALSE;
          line += 4;
        }
      }
      break;
      default:
        longjmp(png_jmpbuf(decoder->mPNG), 1);
    }

    if (!rowHasNoAlpha)
      decoder->mFrameHasNoAlpha = PR_FALSE;

    PRUint32 numFrames = decoder->mImage->GetNumFrames();
    if (numFrames <= 1) {
      // Only do incremental image display for the first frame
      // XXXbholley - this check should be handled in the superclass
      nsIntRect r(0, row_num, width, 1);
      decoder->PostInvalidation(r);
    }
  }
}

// got the header of a new frame that's coming
void
nsPNGDecoder::frame_info_callback(png_structp png_ptr, png_uint_32 frame_num)
{
#ifdef PNG_APNG_SUPPORTED
  png_uint_32 x_offset, y_offset;
  PRInt32 width, height;

  nsPNGDecoder *decoder =
               static_cast<nsPNGDecoder*>(png_get_progressive_ptr(png_ptr));

  // old frame is done
  if (!decoder->mFrameIsHidden)
    decoder->EndImageFrame();

  decoder->mFrameIsHidden = PR_FALSE;

  x_offset = png_get_next_frame_x_offset(png_ptr, decoder->mInfo);
  y_offset = png_get_next_frame_y_offset(png_ptr, decoder->mInfo);
  width = png_get_next_frame_width(png_ptr, decoder->mInfo);
  height = png_get_next_frame_height(png_ptr, decoder->mInfo);

  decoder->CreateFrame(x_offset, y_offset, width, height, decoder->format);
#endif
}

void
nsPNGDecoder::end_callback(png_structp png_ptr, png_infop info_ptr)
{
  /* libpng comments:
   *
   * this function is called when the whole image has been read,
   * including any chunks after the image (up to and including
   * the IEND).  You will usually have the same info chunk as you
   * had in the header, although some data may have been added
   * to the comments and time fields.
   *
   * Most people won't do much here, perhaps setting a flag that
   * marks the image as finished.
   */

  nsPNGDecoder *decoder =
               static_cast<nsPNGDecoder*>(png_get_progressive_ptr(png_ptr));

  // We shouldn't get here if we've hit an error
  NS_ABORT_IF_FALSE(!decoder->IsError(), "Finishing up PNG but hit error!");

#ifdef PNG_APNG_SUPPORTED
  if (png_get_valid(png_ptr, info_ptr, PNG_INFO_acTL)) {
    PRInt32 num_plays = png_get_num_plays(png_ptr, info_ptr);
    decoder->mImage->SetLoopCount(num_plays - 1);
  }
#endif

  // Send final notifications
  decoder->NotifyDone(/* aSuccess = */ PR_TRUE);
}


void
nsPNGDecoder::error_callback(png_structp png_ptr, png_const_charp error_msg)
{
  PR_LOG(gPNGLog, PR_LOG_ERROR, ("libpng error: %s\n", error_msg));
  longjmp(png_jmpbuf(png_ptr), 1);
}


void
nsPNGDecoder::warning_callback(png_structp png_ptr, png_const_charp warning_msg)
{
  PR_LOG(gPNGLog, PR_LOG_WARNING, ("libpng warning: %s\n", warning_msg));
}

} // namespace imagelib
} // namespace mozilla

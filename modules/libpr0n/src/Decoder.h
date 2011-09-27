/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
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
 * the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010.
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Bobby Holley <bobbyholley@gmail.com>
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

#ifndef MOZILLA_IMAGELIB_DECODER_H_
#define MOZILLA_IMAGELIB_DECODER_H_

#include "RasterImage.h"

#include "imgIDecoderObserver.h"

namespace mozilla {
namespace imagelib {

class Decoder
{
public:

  Decoder();
  virtual ~Decoder();

  /**
   * Initialize an image decoder. Decoders may not be re-initialized.
   *
   * @param aContainer The image container to decode to.
   * @param aObserver The observer for decode notification events.
   *
   * Notifications Sent: TODO
   */
  void Init(RasterImage* aImage, imgIDecoderObserver* aObserver);


  /**
   * Initializes a decoder whose aImage and aObserver is already being used by a
   * parent decoder. Decoders may not be re-initialized.
   *
   * @param aContainer The image container to decode to.
   * @param aObserver The observer for decode notification events.
   *
   * Notifications Sent: TODO
   */
  void InitSharedDecoder(RasterImage* aImage, imgIDecoderObserver* aObserver);

  /**
   * Writes data to the decoder.
   *
   * @param aBuffer buffer containing the data to be written
   * @param aCount the number of bytes to write
   *
   * Any errors are reported by setting the appropriate state on the decoder.
   *
   * Notifications Sent: TODO
   */
  void Write(const char* aBuffer, PRUint32 aCount);

  /**
   * Informs the decoder that all the data has been written.
   *
   * Notifications Sent: TODO
   */
  void Finish();

  /**
   * Informs the shared decoder that all the data has been written.
   * Should only be used if InitSharedDecoder was useed
   *
   * Notifications Sent: TODO
   */
  void FinishSharedDecoder();

  /**
   * Tells the decoder to flush any pending invalidations. This informs the image
   * frame of its decoded region, and sends the appropriate OnDataAvailable call
   * to consumers.
   *
   * This can be called any time when we're midway through decoding a frame,
   * and must be called after finishing a frame (before starting a new one).
   */
  void FlushInvalidations();

  // We're not COM-y, so we don't get refcounts by default
  NS_INLINE_DECL_REFCOUNTING(Decoder)

  /*
   * State.
   */

  // If we're doing a "size decode", we more or less pass through the image
  // data, stopping only to scoop out the image dimensions. A size decode
  // must be enabled by SetSizeDecode() _before_calling Init().
  bool IsSizeDecode() { return mSizeDecode; };
  void SetSizeDecode(bool aSizeDecode)
  {
    NS_ABORT_IF_FALSE(!mInitialized, "Can't set size decode after Init()!");
    mSizeDecode = aSizeDecode;
  }

  // The number of frames we have, including anything in-progress. Thus, this
  // is only 0 if we haven't begun any frames.
  PRUint32 GetFrameCount() { return mFrameCount; }

  // The number of complete frames we have (ie, not including anything in-progress).
  PRUint32 GetCompleteFrameCount() { return mInFrame ? mFrameCount - 1 : mFrameCount; }

  // Error tracking
  bool HasError() { return HasDataError() || HasDecoderError(); };
  bool HasDataError() { return mDataError; };
  bool HasDecoderError() { return NS_FAILED(mFailCode); };
  nsresult GetDecoderError() { return mFailCode; };
  void PostResizeError() { PostDataError(); }
  bool GetDecodeDone() const {
    return mDecodeDone;
  }

  // flags.  Keep these in sync with imgIContainer.idl.
  // SetDecodeFlags must be called before Init(), otherwise
  // default flags are assumed.
  enum {
    DECODER_NO_PREMULTIPLY_ALPHA = 0x2,
    DECODER_NO_COLORSPACE_CONVERSION = 0x4
  };
  void SetDecodeFlags(PRUint32 aFlags) { mDecodeFlags = aFlags; }
  PRUint32 GetDecodeFlags() { return mDecodeFlags; }

  // Use HistogramCount as an invalid Histogram ID
  virtual Telemetry::ID SpeedHistogram() { return Telemetry::HistogramCount; }

protected:

  /*
   * Internal hooks. Decoder implementations may override these and
   * only these methods.
   */
  virtual void InitInternal();
  virtual void WriteInternal(const char* aBuffer, PRUint32 aCount);
  virtual void FinishInternal();

  /*
   * Progress notifications.
   */

  // Called by decoders when they determine the size of the image. Informs
  // the image of its size and sends notifications.
  void PostSize(PRInt32 aWidth, PRInt32 aHeight);

  // Called by decoders when they begin/end a frame. Informs the image, sends
  // notifications, and does internal book-keeping.
  void PostFrameStart();
  void PostFrameStop();

  // Called by the decoders when they have a region to invalidate. We may not
  // actually pass these invalidations on right away.
  void PostInvalidation(nsIntRect& aRect);

  // Called by the decoders when they have successfully decoded the image. This
  // may occur as the result of the decoder getting to the appropriate point in
  // the stream, or by us calling FinishInternal().
  //
  // May not be called mid-frame.
  void PostDecodeDone();

  // Data errors are the fault of the source data, decoder errors are our fault
  void PostDataError();
  void PostDecoderError(nsresult aFailCode);

  /*
   * Member variables.
   *
   */
  nsRefPtr<RasterImage> mImage;
  nsCOMPtr<imgIDecoderObserver> mObserver;

  PRUint32 mDecodeFlags;
  bool mDecodeDone;
  bool mDataError;

private:
  PRUint32 mFrameCount; // Number of frames, including anything in-progress

  nsIntRect mInvalidRect; // Tracks an invalidation region in the current frame.

  nsresult mFailCode;

  bool mInitialized;
  bool mSizeDecode;
  bool mInFrame;
};

} // namespace imagelib
} // namespace mozilla

#endif // MOZILLA_IMAGELIB_DECODER_H_

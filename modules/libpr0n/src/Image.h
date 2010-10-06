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

#ifndef MOZILLA_IMAGELIB_IMAGE_H_
#define MOZILLA_IMAGELIB_IMAGE_H_

#include "imgIContainer.h"
#include "imgStatusTracker.h"
#include "prtypes.h"

namespace mozilla {
namespace imagelib {

class Image : public imgIContainer
{
public:
  imgStatusTracker& GetStatusTracker() { return *mStatusTracker; }

  /**
   * Flags for Image initialization.
   *
   * Meanings:
   *
   * INIT_FLAG_NONE: Lack of flags
   *
   * INIT_FLAG_DISCARDABLE: The container should be discardable
   *
   * INIT_FLAG_DECODE_ON_DRAW: The container should decode on draw rather than
   * decoding on load.
   *
   * INIT_FLAG_MULTIPART: The container will be used to display a stream of
   * images in a multipart channel. If this flag is set, INIT_FLAG_DISCARDABLE
   * and INIT_FLAG_DECODE_ON_DRAW must not be set.
   */
  static const PRUint32 INIT_FLAG_NONE           = 0x0;
  static const PRUint32 INIT_FLAG_DISCARDABLE    = 0x1;
  static const PRUint32 INIT_FLAG_DECODE_ON_DRAW = 0x2;
  static const PRUint32 INIT_FLAG_MULTIPART      = 0x4;

  /**
   * Creates a new image container.
   *
   * @param aObserver Observer to send decoder and animation notifications to.
   * @param aMimeType The mimetype of the image.
   * @param aFlags Initialization flags of the INIT_FLAG_* variety.
   */
  virtual nsresult Init(imgIDecoderObserver* aObserver,
                        const char* aMimeType,
                        const char* aURIString,
                        PRUint32 aFlags) = 0;

  /**
   * The rectangle defining the location and size of the currently displayed
   * frame.
   */
  virtual void GetCurrentFrameRect(nsIntRect& aRect) = 0;

  /**
   * The size, in bytes, occupied by the significant data portions of the image.
   * This includes both compressed source data and decoded frames.
   */
  PRUint32 GetDataSize();

  /**
   * The components that make up GetDataSize().
   */      
  virtual PRUint32 GetDecodedDataSize() = 0;
  virtual PRUint32 GetSourceDataSize() = 0;

  // Mimetype translation
  enum eDecoderType {
    eDecoderType_png     = 0,
    eDecoderType_gif     = 1,
    eDecoderType_jpeg    = 2,
    eDecoderType_bmp     = 3,
    eDecoderType_ico     = 4,
    eDecoderType_icon    = 5,
    eDecoderType_unknown = 6
  };
  static eDecoderType GetDecoderType(const char *aMimeType);

  void IncrementAnimationConsumers();
  void DecrementAnimationConsumers();
#ifdef DEBUG
  PRUint32 GetAnimationConsumers() { return mAnimationConsumers; }
#endif

protected:
  Image(imgStatusTracker* aStatusTracker);

  /**
   * Decides whether animation should or should not be happening,
   * and makes sure the right thing is being done.
   */
  virtual void EvaluateAnimation();

  virtual nsresult StartAnimation() = 0;
  virtual nsresult StopAnimation() = 0;

  // Member data shared by all implementations of this abstract class
  nsAutoPtr<imgStatusTracker> mStatusTracker;
  PRUint32                    mAnimationConsumers;
  PRPackedBool                mInitialized;   // Have we been initalized?
  PRPackedBool                mAnimating;
  PRPackedBool                mError;         // Error handling

  /**
   * Extended by child classes, if they have additional
   * conditions for being able to animate
   */
  virtual PRBool ShouldAnimate() { return mAnimationConsumers > 0; }
};

} // namespace imagelib
} // namespace mozilla

#endif // MOZILLA_IMAGELIB_IMAGE_H_

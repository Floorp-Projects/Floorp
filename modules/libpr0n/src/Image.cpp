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

#include "Image.h"

namespace mozilla {
namespace imagelib {

// Constructor
Image::Image(imgStatusTracker* aStatusTracker) :
  mAnimationConsumers(0),
  mInitialized(PR_FALSE),
  mAnimating(PR_FALSE),
  mError(PR_FALSE)
{
  if (aStatusTracker) {
    mStatusTracker = aStatusTracker;
    mStatusTracker->SetImage(this);
  } else {
    mStatusTracker = new imgStatusTracker(this);
  }
}

PRUint32
Image::GetDataSize()
{
  if (mError)
    return 0;
  
  return GetSourceDataSize() + GetDecodedDataSize();
}

// Translates a mimetype into a concrete decoder
Image::eDecoderType
Image::GetDecoderType(const char *aMimeType)
{
  // By default we don't know
  eDecoderType rv = eDecoderType_unknown;

  // PNG
  if (!strcmp(aMimeType, "image/png"))
    rv = eDecoderType_png;
  else if (!strcmp(aMimeType, "image/x-png"))
    rv = eDecoderType_png;

  // GIF
  else if (!strcmp(aMimeType, "image/gif"))
    rv = eDecoderType_gif;


  // JPEG
  else if (!strcmp(aMimeType, "image/jpeg"))
    rv = eDecoderType_jpeg;
  else if (!strcmp(aMimeType, "image/pjpeg"))
    rv = eDecoderType_jpeg;
  else if (!strcmp(aMimeType, "image/jpg"))
    rv = eDecoderType_jpeg;

  // BMP
  else if (!strcmp(aMimeType, "image/bmp"))
    rv = eDecoderType_bmp;
  else if (!strcmp(aMimeType, "image/x-ms-bmp"))
    rv = eDecoderType_bmp;


  // ICO
  else if (!strcmp(aMimeType, "image/x-icon"))
    rv = eDecoderType_ico;
  else if (!strcmp(aMimeType, "image/vnd.microsoft.icon"))
    rv = eDecoderType_ico;

  // Icon
  else if (!strcmp(aMimeType, "image/icon"))
    rv = eDecoderType_icon;

  return rv;
}

void
Image::IncrementAnimationConsumers()
{
  mAnimationConsumers++;
  EvaluateAnimation();
}

void
Image::DecrementAnimationConsumers()
{
  NS_ABORT_IF_FALSE(mAnimationConsumers >= 1, "Invalid no. of animation consumers!");
  mAnimationConsumers--;
  EvaluateAnimation();
}

void
Image::EvaluateAnimation()
{
  if (!mAnimating && ShouldAnimate()) {
    nsresult rv = StartAnimation();
    mAnimating = NS_SUCCEEDED(rv);
  } else if (mAnimating && !ShouldAnimate()) {
    StopAnimation();
    mAnimating = PR_FALSE;
  }
}

} // namespace imagelib
} // namespace mozilla

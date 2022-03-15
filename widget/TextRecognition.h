/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_widget_nsTextRecognition__
#define mozilla_widget_nsTextRecognition__

#include "mozilla/MozPromise.h"
#include "mozilla/gfx/Point.h"
#include "nsTArray.h"
#include "nsITextRecognition.h"

class imgIContainer;
namespace mozilla::gfx {
class SourceSurface;
}

namespace mozilla::widget {

struct TextRecognitionQuad {
  gfx::Point mPoints[4];
  nsString mString;
  // Confidence from zero to one.
  float mConfidence = 0.0f;
};

struct TextRecognitionResult {
  nsTArray<TextRecognitionQuad> mQuads;
};

class TextRecognition final : public nsITextRecognition {
 public:
  using NativePromise = MozPromise<TextRecognitionResult, nsCString,
                                   /* IsExclusive = */ true>;

  NS_DECL_NSITEXTRECOGNITION
  NS_DECL_ISUPPORTS

  TextRecognition() = default;

  // This should be implemented in the OS specific file.
  static RefPtr<NativePromise> FindText(gfx::SourceSurface&);

 protected:
  ~TextRecognition() = default;
};

}  // namespace mozilla::widget

#endif

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ImageDecoderSupport_h__
#define ImageDecoderSupport_h__

#include "mozilla/java/ImageDecoderNatives.h"

class imgIContainerCallback;

namespace mozilla {
namespace widget {

class ImageDecoderSupport final
    : public java::ImageDecoder::Natives<ImageDecoderSupport> {
 public:
  static void Decode(jni::String::Param aUri, int32_t aDesiredLength,
                     jni::Object::Param aResult);

 private:
  static nsresult DecodeInternal(const nsAString& aUri,
                                 imgIContainerCallback* aCallback,
                                 imgINotificationObserver* aObserver);
};

}  // namespace widget
}  // namespace mozilla

#endif  // ImageDecoderSupport_h__

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_widget_nsTextRecognition__
#define mozilla_widget_nsTextRecognition__

#include "mozilla/MozPromise.h"
#include "mozilla/gfx/Point.h"
#include "nsTArray.h"

class imgIContainer;
namespace mozilla {

namespace dom {
class ShadowRoot;
class TextRecognitionResultOrError;
class TextRecognitionResult;
}  // namespace dom

namespace gfx {
class SourceSurface;
class DataSourceSurface;
}  // namespace gfx

namespace widget {

class TextRecognition final {
 public:
  using NativePromise = MozPromise<dom::TextRecognitionResult, nsCString,
                                   /* IsExclusive = */ true>;

  TextRecognition() = default;

  static void FillShadow(dom::ShadowRoot&, const dom::TextRecognitionResult&);

  static RefPtr<NativePromise> FindText(imgIContainer&,
                                        const nsTArray<nsCString>&);
  static RefPtr<NativePromise> FindText(gfx::DataSourceSurface&,
                                        const nsTArray<nsCString>&);
  static bool IsSupported();

 protected:
  // This should be implemented in the OS specific file.
  static RefPtr<NativePromise> DoFindText(gfx::DataSourceSurface&,
                                          const nsTArray<nsCString>&);

  ~TextRecognition() = default;
};

}  // namespace widget
}  // namespace mozilla

#endif

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#import <Vision/Vision.h>

#include "mozilla/dom/Promise.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/ErrorResult.h"
#include "ErrorList.h"
#include "nsClipboard.h"
#include "nsCocoaUtils.h"
#include "mozilla/MacStringHelpers.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/widget/TextRecognition.h"
#include "mozilla/dom/PContent.h"

namespace mozilla::widget {

auto TextRecognition::DoFindText(gfx::DataSourceSurface& aSurface,
                                 const nsTArray<nsCString>& aLanguages)
    -> RefPtr<NativePromise> {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK

  // TODO - Is this the most efficient path? Maybe we can write a new
  // CreateCGImageFromXXX that enables more efficient marshalling of the data.
  CGImageRef imageRef = NULL;
  nsresult rv = nsCocoaUtils::CreateCGImageFromSurface(&aSurface, &imageRef);
  if (NS_FAILED(rv) || !imageRef) {
    return NativePromise::CreateAndReject("Failed to create CGImage"_ns,
                                          __func__);
  }

  auto promise = MakeRefPtr<NativePromise::Private>(__func__);

  NSMutableArray* recognitionLanguages = [[NSMutableArray alloc] init];
  for (const auto& locale : aLanguages) {
    [recognitionLanguages addObject:nsCocoaUtils::ToNSString(locale)];
  }

  NS_DispatchBackgroundTask(
      NS_NewRunnableFunction(
          __func__,
          [promise, imageRef, recognitionLanguages] {
            auto unrefImage = MakeScopeExit([&] {
              ::CGImageRelease(imageRef);
              [recognitionLanguages release];
            });

            dom::TextRecognitionResult result;
            dom::TextRecognitionResult* pResult = &result;

            // Define the request to use, which also handles the result. It will
            // be run below directly in this thread. After creating this
            // request.
            VNRecognizeTextRequest* textRecognitionRequest =
                [[VNRecognizeTextRequest alloc] initWithCompletionHandler:^(
                                                    VNRequest* _Nonnull request,
                                                    NSError* _Nullable error) {
                  NSArray<VNRecognizedTextObservation*>* observations =
                      request.results;

                  [observations enumerateObjectsUsingBlock:^(
                                    VNRecognizedTextObservation* _Nonnull obj,
                                    NSUInteger idx, BOOL* _Nonnull stop) {
                    // Requests the n top candidates for a recognized text
                    // string.
                    VNRecognizedText* recognizedText =
                        [obj topCandidates:1].firstObject;

                    // https://developer.apple.com/documentation/vision/vnrecognizedtext?language=objc
                    auto& quad = *pResult->quads().AppendElement();
                    CopyCocoaStringToXPCOMString(recognizedText.string,
                                                 quad.string());
                    quad.confidence() = recognizedText.confidence;

                    auto ToImagePoint = [](CGPoint aPoint) -> ImagePoint {
                      return {static_cast<float>(aPoint.x),
                              static_cast<float>(aPoint.y)};
                    };
                    *quad.points().AppendElement() =
                        ToImagePoint(obj.bottomLeft);
                    *quad.points().AppendElement() = ToImagePoint(obj.topLeft);
                    *quad.points().AppendElement() = ToImagePoint(obj.topRight);
                    *quad.points().AppendElement() =
                        ToImagePoint(obj.bottomRight);
                  }];
                }];

            textRecognitionRequest.recognitionLevel =
                VNRequestTextRecognitionLevelAccurate;
            textRecognitionRequest.recognitionLanguages = recognitionLanguages;
            textRecognitionRequest.usesLanguageCorrection = true;

            // Send out the request. This blocks execution of this thread with
            // an expensive CPU call.
            NSError* error = nil;
            VNImageRequestHandler* requestHandler =
                [[[VNImageRequestHandler alloc] initWithCGImage:imageRef
                                                        options:@{}]
                    autorelease];

            [requestHandler performRequests:@[ textRecognitionRequest ]
                                      error:&error];
            if (error != nil) {
              promise->Reject(
                  nsPrintfCString(
                      "Failed to perform text recognition request (%ld)\n",
                      error.code),
                  __func__);
            } else {
              promise->Resolve(std::move(result), __func__);
            }
          }),
      NS_DISPATCH_EVENT_MAY_BLOCK);
  return promise;

  NS_OBJC_END_TRY_IGNORE_BLOCK
}

}  // namespace mozilla::widget

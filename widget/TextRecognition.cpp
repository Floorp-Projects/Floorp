/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TextRecognition.h"
#include "mozilla/dom/Promise.h"
#include "imgIContainer.h"

namespace mozilla::widget {

NS_IMPL_ISUPPORTS(TextRecognition, nsITextRecognition);

static bool ToJSValue(JSContext* aCx, widget::TextRecognitionResult& aResult,
                      JS::MutableHandle<JS::Value> aValue) {
  // TODO(emilio): Expose this to JS in a nicer way than just the string.
  nsAutoString string;
  for (const auto& quad : aResult.mQuads) {
    string.Append(quad.mString);
    string.Append(u'\n');
  }
  return dom::ToJSValue(aCx, string, aValue);
}

NS_IMETHODIMP
TextRecognition::FindText(imgIContainer* aImage, JSContext* aCx,
                          dom::Promise** aResultPromise) {
  ErrorResult rv;
  RefPtr<dom::Promise> promise =
      dom::Promise::Create(xpc::CurrentNativeGlobal(aCx), rv);
  if (MOZ_UNLIKELY(rv.Failed())) {
    return rv.StealNSResult();
  }

  RefPtr<gfx::SourceSurface> surface = aImage->GetFrame(
      imgIContainer::FRAME_CURRENT,
      imgIContainer::FLAG_SYNC_DECODE | imgIContainer::FLAG_ASYNC_NOTIFY);

  if (NS_WARN_IF(!surface)) {
    promise->MaybeRejectWithInvalidStateError("Failed to get surface");
    return NS_OK;
  }

  auto promiseHolder = MakeRefPtr<nsMainThreadPtrHolder<dom::Promise>>(
      "nsTextRecognition::SpawnOSBackgroundThread", promise);
  FindText(*surface)->Then(
      GetCurrentSerialEventTarget(), __func__,
      [promiseHolder = std::move(promiseHolder)](
          NativePromise::ResolveOrRejectValue&& aValue) {
        dom::Promise* p = promiseHolder->get();
        if (aValue.IsReject()) {
          p->MaybeRejectWithNotSupportedError(aValue.RejectValue());
          return;
        }
        p->MaybeResolve(aValue.ResolveValue());
      });

  promise.forget(aResultPromise);
  return NS_OK;
}

auto TextRecognition::FindText(gfx::SourceSurface&) -> RefPtr<NativePromise> {
  return NativePromise::CreateAndReject("Text recognition not available"_ns,
                                        __func__);
}

}  // namespace mozilla::widget

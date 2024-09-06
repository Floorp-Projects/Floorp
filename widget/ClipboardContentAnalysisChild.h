/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_WIDGET_CLIPBOARDCONTENTANALYSISCHILD_H_
#define MOZILLA_WIDGET_CLIPBOARDCONTENTANALYSISCHILD_H_

#include "mozilla/ipc/Endpoint.h"
#include "mozilla/PClipboardContentAnalysisChild.h"

namespace mozilla {
class ClipboardContentAnalysisChild final
    : public PClipboardContentAnalysisChild {
 public:
  NS_INLINE_DECL_REFCOUNTING(ClipboardContentAnalysisChild, override)

  static ClipboardContentAnalysisChild* GetOrCreate();

  void ActorDestroy(ActorDestroyReason aReason) override final {
    // There's only one singleton, so remove our reference to it.
    sSingleton = nullptr;
  }

 private:
  friend PClipboardContentAnalysisChild;
  ClipboardContentAnalysisChild() { MOZ_ASSERT(XRE_IsContentProcess()); }
  ~ClipboardContentAnalysisChild() = default;
  static StaticRefPtr<ClipboardContentAnalysisChild> sSingleton;
};
}  // namespace mozilla

#endif  // MOZILLA_WIDGET_CLIPBOARDCONTENTANALYSISCHILD_H_

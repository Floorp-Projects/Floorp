/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __AboutThirdParty_h__
#define __AboutThirdParty_h__

#include "mozilla/MozPromise.h"
#include "nsIAboutThirdParty.h"

namespace mozilla {

using BackgroundThreadPromise =
    MozPromise<bool /* aIgnored */, nsresult, /* IsExclusive */ false>;

class AboutThirdParty final : public nsIAboutThirdParty {
  // Atomic only supports 32-bit or 64-bit types.
  enum class WorkerState : uint32_t {
    Init,
    Running,
    Done,
  };
  Atomic<WorkerState, SequentiallyConsistent> mWorkerState;
  RefPtr<BackgroundThreadPromise::Private> mPromise;

  ~AboutThirdParty() = default;
  void BackgroundThread();

 public:
  static already_AddRefed<AboutThirdParty> GetSingleton();

  AboutThirdParty();

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIABOUTTHIRDPARTY

  // Have a function separated from dom::Promise so that
  // both JS method and GTest can use.
  RefPtr<BackgroundThreadPromise> CollectSystemInfoAsync();
};

}  // namespace mozilla

#endif  // __AboutThirdParty_h__

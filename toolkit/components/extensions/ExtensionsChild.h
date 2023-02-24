/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_extensions_ExtensionsChild_h
#define mozilla_extensions_ExtensionsChild_h

#include "mozilla/extensions/PExtensionsChild.h"
#include "nsISupportsImpl.h"

namespace mozilla {
namespace extensions {

class ExtensionsChild final : public nsIObserver, public PExtensionsChild {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  static already_AddRefed<ExtensionsChild> GetSingleton();

  static ExtensionsChild& Get();

 private:
  ExtensionsChild() = default;
  ~ExtensionsChild() = default;

  void Init();

 protected:
  void ActorDestroy(ActorDestroyReason aWhy) override;
};

}  // namespace extensions
}  // namespace mozilla

#endif  // mozilla_extensions_ExtensionsChild_h

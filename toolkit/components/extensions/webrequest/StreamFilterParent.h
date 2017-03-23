/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_extensions_StreamFilterParent_h
#define mozilla_extensions_StreamFilterParent_h

#include "mozilla/extensions/PStreamFilterParent.h"

namespace mozilla {
namespace dom {
  class nsIContentParent;
}

namespace extensions {

using namespace mozilla::dom;
using mozilla::ipc::IPCResult;

class StreamFilterParent final : public PStreamFilterParent
                               , public nsISupports
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS

  explicit StreamFilterParent(uint64_t aChannelId, const nsAString& aAddonId);

  static already_AddRefed<StreamFilterParent>
  Create(uint64_t aChannelId, const nsAString& aAddonId)
  {
    RefPtr<StreamFilterParent> filter = new StreamFilterParent(aChannelId, aAddonId);
    return filter.forget();
  }

  void Init(already_AddRefed<nsIContentParent> aContentParent);

protected:
  virtual ~StreamFilterParent();

private:
  virtual void ActorDestroy(ActorDestroyReason aWhy) override;

  const uint64_t mChannelId;
  const nsCOMPtr<nsIAtom> mAddonId;
};

} // namespace extensions
} // namespace mozilla

#endif // mozilla_extensions_StreamFilterParent_h

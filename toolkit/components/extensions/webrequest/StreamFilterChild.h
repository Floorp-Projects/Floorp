/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_extensions_StreamFilterChild_h
#define mozilla_extensions_StreamFilterChild_h

#include "mozilla/extensions/PStreamFilterChild.h"
#include "nsISupportsImpl.h"

namespace mozilla {
namespace extensions {

using mozilla::ipc::IPCResult;

class StreamFilterChild final : public PStreamFilterChild
{
public:
  NS_INLINE_DECL_REFCOUNTING(StreamFilterChild)

  StreamFilterChild() {}

protected:
  virtual IPCResult Recv__delete__() override { return IPC_OK(); }

private:
  ~StreamFilterChild() {}

  virtual void ActorDestroy(ActorDestroyReason aWhy) override;
};

} // namespace extensions
} // namespace mozilla

#endif // mozilla_extensions_StreamFilterChild_h

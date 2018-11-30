/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_LoginReputationIPC_h
#define mozilla_dom_LoginReputationIPC_h

#include "mozilla/dom/PLoginReputationParent.h"
#include "nsILoginReputation.h"

namespace mozilla {
namespace dom {

class LoginReputationParent : public nsILoginReputationQueryCallback,
                              public PLoginReputationParent {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSILOGINREPUTATIONQUERYCALLBACK

  LoginReputationParent() = default;

  mozilla::ipc::IPCResult QueryReputation(nsIURI* aURI);

  void ActorDestroy(ActorDestroyReason aWhy) override;

 private:
  ~LoginReputationParent() = default;
  bool mIPCOpen = true;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_LoginReputationIPC_h

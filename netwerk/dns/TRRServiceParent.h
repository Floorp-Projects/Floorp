/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_TRRServiceParent_h
#define mozilla_net_TRRServiceParent_h

#include "mozilla/net/PTRRServiceParent.h"
#include "mozilla/net/TRRServiceBase.h"
#include "nsIObserver.h"
#include "nsWeakReference.h"

namespace mozilla {
namespace net {

class TRRServiceParent : public TRRServiceBase,
                         public nsIObserver,
                         public nsSupportsWeakReference,
                         public PTRRServiceParent {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIOBSERVER

  TRRServiceParent() = default;
  void Init();
  void UpdateParentalControlEnabled();
  static void PrefsChanged(const char* aName, void* aSelf);
  void SetDetectedTrrURI(const nsACString& aURI);
  bool MaybeSetPrivateURI(const nsACString& aURI) override;
  void GetTrrURI(nsACString& aURI);

 private:
  virtual ~TRRServiceParent() = default;
  virtual void ActorDestroy(ActorDestroyReason why) override;
  void prefsChanged(const char* aName);
};

}  // namespace net
}  // namespace mozilla

#endif  // mozilla_net_TRRServiceParent_h

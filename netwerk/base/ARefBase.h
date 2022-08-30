/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_ARefBase_h
#define mozilla_net_ARefBase_h

#include "nsISupportsImpl.h"

namespace mozilla {
namespace net {

// This is an abstract class that can be pointed to by either
// nsCOMPtr or nsRefPtr. nsHttpConnectionMgr uses it for generic
// objects that need to be reference counted - similiar to nsISupports
// but it may or may not be xpcom.

class ARefBase {
 public:
  ARefBase() = default;
  virtual ~ARefBase() = default;

  NS_INLINE_DECL_PURE_VIRTUAL_REFCOUNTING
};

}  // namespace net
}  // namespace mozilla

#endif

/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_ARefBase_h
#define mozilla_net_ARefBase_h

#include "nscore.h"

namespace mozilla { namespace net {

// This is an abstract class that can be pointed to by either
// nsCOMPtr or nsRefPtr. nsHttpConnectionMgr uses it for generic
// objects that need to be reference counted - similiar to nsISupports
// but it may or may not be xpcom.

class ARefBase
{
public:
  ARefBase() {}
  virtual ~ARefBase() {}

  NS_IMETHOD_ (MozExternalRefCountType) AddRef() = 0;
  NS_IMETHOD_ (MozExternalRefCountType) Release() = 0;
};

} // namespace net
} // namespace mozilla

#endif

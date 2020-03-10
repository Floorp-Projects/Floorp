/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef AltServiceChild_h__
#define AltServiceChild_h__

#include "mozilla/net/PAltServiceChild.h"

namespace mozilla {
namespace net {

class nsHttpConnectionInfo;

class AltServiceChild final : public PAltServiceChild {
 public:
  NS_INLINE_DECL_REFCOUNTING(AltServiceChild, override)

  static bool EnsureAltServiceChild();
  static void ClearHostMapping(nsHttpConnectionInfo* aCi);

 private:
  AltServiceChild();
  virtual ~AltServiceChild();
};

}  // namespace net
}  // namespace mozilla

#endif  // AltServiceChild_h__

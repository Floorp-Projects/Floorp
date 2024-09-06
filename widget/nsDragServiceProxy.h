/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef NSDRAGSERVICEPROXY_H
#define NSDRAGSERVICEPROXY_H

#include "nsBaseDragService.h"

// Temporary inheritance from nsBaseDragService instead of nsBaseDragSession
// (which nsBaseDragService temporarily inherits).
// This will be undone at the end of this patch series.
class nsDragSessionProxy : public nsBaseDragService {};

// Temporary inheritance from nsDragSessionProxy instead of nsBaseDragService
// (which nsDragSession temporarily inherits).
// This will be undone at the end of this patch series.
class nsDragServiceProxy final : public nsDragSessionProxy {
 public:
  nsDragServiceProxy();

  NS_INLINE_DECL_REFCOUNTING_INHERITED(nsDragServiceProxy, nsDragSessionProxy)

  // nsBaseDragService
  virtual nsresult InvokeDragSessionImpl(
      nsIArray* anArrayTransferables,
      const mozilla::Maybe<mozilla::CSSIntRegion>& aRegion,
      uint32_t aActionType) override;

 private:
  virtual ~nsDragServiceProxy();
};

#endif  // NSDRAGSERVICEPROXY_H

/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef NSDRAGSERVICEPROXY_H
#define NSDRAGSERVICEPROXY_H

#include "nsBaseDragService.h"

class nsDragServiceProxy : public nsBaseDragService {
 public:
  nsDragServiceProxy();

  NS_INLINE_DECL_REFCOUNTING_INHERITED(nsDragServiceProxy, nsBaseDragService)

  // nsBaseDragService
  virtual nsresult InvokeDragSessionImpl(
      nsIArray* anArrayTransferables,
      const mozilla::Maybe<mozilla::CSSIntRegion>& aRegion,
      uint32_t aActionType) override;

 private:
  virtual ~nsDragServiceProxy();
};

#endif  // NSDRAGSERVICEPROXY_H

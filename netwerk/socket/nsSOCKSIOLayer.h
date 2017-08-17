/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsSOCKSIOLayer_h__
#define nsSOCKSIOLayer_h__

#include "prio.h"
#include "nscore.h"
#include "nsIProxyInfo.h"

nsresult nsSOCKSIOLayerAddToSocket(int32_t       family,
                                   const char   *host,
                                   int32_t       port,
                                   nsIProxyInfo *proxyInfo,
                                   int32_t       socksVersion,
                                   uint32_t      flags,
                                   uint32_t      tlsFlags,
                                   PRFileDesc   *fd,
                                   nsISupports **info);

bool IsHostLocalTarget(const nsACString& aHost);

#endif /* nsSOCKSIOLayer_h__ */

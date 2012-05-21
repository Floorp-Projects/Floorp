/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsSOCKSIOLayer_h__
#define nsSOCKSIOLayer_h__

#include "prtypes.h"
#include "prio.h"
#include "nscore.h"

nsresult nsSOCKSIOLayerAddToSocket(PRInt32       family,
                                   const char   *host, 
                                   PRInt32       port,
                                   const char   *proxyHost,
                                   PRInt32       proxyPort,
                                   PRInt32       socksVersion,
                                   PRUint32      flags,
                                   PRFileDesc   *fd, 
                                   nsISupports **info);

#endif /* nsSOCKSIOLayer_h__ */

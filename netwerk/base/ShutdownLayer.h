/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SHUTDOWNLAYER_H___
#define SHUTDOWNLAYER_H___

#include "nscore.h"
#include "prio.h"

namespace mozilla { namespace net {

// This is only for windows. This layer will be attached jus before PR_CLose
// is call and it will only call shutdown(sock).
extern nsresult AttachShutdownLayer(PRFileDesc *fd);

} // namespace net
} // namespace mozilla

#endif /* SHUTDOWN_H___ */

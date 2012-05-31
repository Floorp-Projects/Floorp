/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef _SSLSERVERCERTVERIFICATION_H
#define _SSLSERVERCERTVERIFICATION_H

#include "seccomon.h"
#include "prio.h"

namespace mozilla { namespace psm {

SECStatus AuthCertificateHook(void *arg, PRFileDesc *fd, 
                              PRBool checkSig, PRBool isServer);

} } // namespace mozilla::psm

#endif

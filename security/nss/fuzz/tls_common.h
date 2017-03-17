/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef tls_common_h__
#define tls_common_h__

#include "prinit.h"

PRStatus EnableAllProtocolVersions();
void EnableAllCipherSuites(PRFileDesc* fd);
void DoHandshake(PRFileDesc* fd, bool isServer);

#endif  // tls_common_h__

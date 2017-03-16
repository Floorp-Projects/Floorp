/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef tls_server_certs_h__
#define tls_server_certs_h__

#include "prio.h"

void InstallServerCertificates(PRFileDesc* fd);

#endif  // tls_server_certs_h__

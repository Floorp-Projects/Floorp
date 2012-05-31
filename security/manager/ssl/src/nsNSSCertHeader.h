/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _NSNSSCERTHEADER_H_
#define _NSNSSCERTHEADER_H_

/* private NSS defines used by PSM */
/* (must be declated before cert.h) */
#define CERT_NewTempCertificate __CERT_NewTempCertificate
#define CERT_AddTempCertToPerm __CERT_AddTempCertToPerm

#include "prtypes.h"
#include "cert.h"
#include "secitem.h"
#include "nsString.h"

#endif

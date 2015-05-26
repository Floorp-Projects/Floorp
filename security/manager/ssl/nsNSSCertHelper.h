/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _NSNSSCERTHELPER_H_
#define _NSNSSCERTHELPER_H_

#ifndef INET6_ADDRSTRLEN
#define INET6_ADDRSTRLEN 46
#endif

#include "certt.h"
#include "nsString.h"

uint32_t
getCertType(CERTCertificate *cert);

CERTCertNicknames *
getNSSCertNicknamesFromCertList(CERTCertList *certList);

nsresult
GetCertFingerprintByOidTag(CERTCertificate* nsscert,
                           SECOidTag aOidTag, 
                           nsCString &fp);

#endif

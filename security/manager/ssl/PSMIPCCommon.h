/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef PSMIPCCommon_h__
#define PSMIPCCommon_h__

#include "mozilla/psm/PSMIPCTypes.h"
#include "seccomon.h"
#include "ScopedNSSTypes.h"

namespace mozilla {
namespace psm {

SECItem* WrapPrivateKeyInfoWithEmptyPassword(SECKEYPrivateKey* pk);
SECStatus UnwrapPrivateKeyInfoWithEmptyPassword(
    SECItem* derPKI, const UniqueCERTCertificate& aCert,
    SECKEYPrivateKey** privk);
void SerializeClientCertAndKey(const UniqueCERTCertificate& aCert,
                               const UniqueSECKEYPrivateKey& aKey,
                               ByteArray& aOutSerializedCert,
                               ByteArray& aOutSerializedKey);
void DeserializeClientCertAndKey(const ByteArray& aSerializedCert,
                                 const ByteArray& aSerializedKey,
                                 UniqueCERTCertificate& aOutCert,
                                 UniqueSECKEYPrivateKey& aOutKey);

}  // namespace psm
}  // namespace mozilla

#endif  // PSMIPCCommon_h__

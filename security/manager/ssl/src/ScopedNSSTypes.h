/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ScopedNSSTypes_h
#define mozilla_ScopedNSSTypes_h

#include "mozilla/Scoped.h"

extern "C" {
#include "prio.h"
#include "cert.h"
#include "cms.h"
#include "keyhi.h"
#include "pk11pub.h"
#include "sechash.h"
} // extern "C"


// Alphabetical order by NSS type
MOZ_TYPE_SPECIFIC_SCOPED_POINTER_TEMPLATE(ScopedPRFileDesc,
                                          PRFileDesc,
                                          PR_Close)
MOZ_TYPE_SPECIFIC_SCOPED_POINTER_TEMPLATE(ScopedCERTCertificate,
                                          CERTCertificate,
                                          CERT_DestroyCertificate)
MOZ_TYPE_SPECIFIC_SCOPED_POINTER_TEMPLATE(ScopedCERTCertificateList,
                                          CERTCertificateList,
                                          CERT_DestroyCertificateList)
MOZ_TYPE_SPECIFIC_SCOPED_POINTER_TEMPLATE(ScopedCERTCertificateRequest,
                                          CERTCertificateRequest,
                                          CERT_DestroyCertificateRequest)
MOZ_TYPE_SPECIFIC_SCOPED_POINTER_TEMPLATE(ScopedCERTCertList,
                                          CERTCertList,
                                          CERT_DestroyCertList)
MOZ_TYPE_SPECIFIC_SCOPED_POINTER_TEMPLATE(ScopedCERTName,
                                          CERTName,
                                          CERT_DestroyName)
MOZ_TYPE_SPECIFIC_SCOPED_POINTER_TEMPLATE(ScopedCERTCertNicknames,
                                          CERTCertNicknames,
                                          CERT_FreeNicknames)
MOZ_TYPE_SPECIFIC_SCOPED_POINTER_TEMPLATE(ScopedCERTSubjectPublicKeyInfo,
                                          CERTSubjectPublicKeyInfo,
                                          SECKEY_DestroySubjectPublicKeyInfo)
MOZ_TYPE_SPECIFIC_SCOPED_POINTER_TEMPLATE(ScopedCERTValidity,
                                          CERTValidity,
                                          CERT_DestroyValidity)

MOZ_TYPE_SPECIFIC_SCOPED_POINTER_TEMPLATE(ScopedHASHContext,
                                          HASHContext,
                                          HASH_Destroy)

MOZ_TYPE_SPECIFIC_SCOPED_POINTER_TEMPLATE(ScopedNSSCMSMessage,
                                          NSSCMSMessage,
                                          NSS_CMSMessage_Destroy)
MOZ_TYPE_SPECIFIC_SCOPED_POINTER_TEMPLATE(ScopedNSSCMSSignedData,
                                          NSSCMSSignedData,
                                          NSS_CMSSignedData_Destroy)


MOZ_TYPE_SPECIFIC_SCOPED_POINTER_TEMPLATE(ScopedPK11SlotInfo,
                                          PK11SlotInfo,
                                          PK11_FreeSlot)
MOZ_TYPE_SPECIFIC_SCOPED_POINTER_TEMPLATE(ScopedPK11SlotList,
                                          PK11SlotList,
                                          PK11_FreeSlotList)
MOZ_TYPE_SPECIFIC_SCOPED_POINTER_TEMPLATE(ScopedPK11SymKey,
                                          PK11SymKey,
                                          PK11_FreeSymKey)

MOZ_TYPE_SPECIFIC_SCOPED_POINTER_TEMPLATE(ScopedSECKEYPrivateKey,
                                          SECKEYPrivateKey,
                                          SECKEY_DestroyPrivateKey)
MOZ_TYPE_SPECIFIC_SCOPED_POINTER_TEMPLATE(ScopedSECKEYPublicKey,
                                          SECKEYPublicKey,
                                          SECKEY_DestroyPublicKey)


#endif // mozilla_ScopedNSSTypes_h

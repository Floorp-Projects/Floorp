/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "IPCClientCertsParent.h"

#include "mozilla/ipc/BackgroundParent.h"

namespace mozilla::psm {

IPCClientCertsParent::IPCClientCertsParent() = default;

// When the IPC client certs module needs to find certificate and key objects
// in the socket process, it will cause this function to be called in the
// parent process. The parent process needs to find all certificates with
// private keys (because these are potential client certificates).
mozilla::ipc::IPCResult IPCClientCertsParent::RecvFindObjects(
    nsTArray<IPCClientCertObject>* aObjects) {
  UniqueCERTCertList certList(psm::FindClientCertificatesWithPrivateKeys());
  if (!certList) {
    return IPC_OK();
  }
  CERTCertListNode* n = CERT_LIST_HEAD(certList);
  while (!CERT_LIST_END(n, certList)) {
    nsTArray<uint8_t> certDER(n->cert->derCert.data, n->cert->derCert.len);
    uint32_t slotType;
    UniqueSECKEYPublicKey pubkey(CERT_ExtractPublicKey(n->cert));
    if (!pubkey) {
      return IPC_OK();
    }
    switch (SECKEY_GetPublicKeyType(pubkey.get())) {
      case rsaKey:
      case rsaPssKey: {
        slotType = PK11_DoesMechanism(n->cert->slot, CKM_RSA_PKCS_PSS)
                       ? kIPCClientCertsSlotTypeModern
                       : kIPCClientCertsSlotTypeLegacy;
        nsTArray<uint8_t> modulus(pubkey->u.rsa.modulus.data,
                                  pubkey->u.rsa.modulus.len);
        RSAKey rsakey(modulus, certDER, slotType);
        aObjects->AppendElement(std::move(rsakey));
        break;
      }
      case ecKey: {
        slotType = kIPCClientCertsSlotTypeModern;
        nsTArray<uint8_t> params(pubkey->u.ec.DEREncodedParams.data,
                                 pubkey->u.ec.DEREncodedParams.len);
        ECKey eckey(params, certDER, slotType);
        aObjects->AppendElement(std::move(eckey));
        break;
      }
      default:
        n = CERT_LIST_NEXT(n);
        continue;
    }
    Certificate cert(certDER, slotType);
    aObjects->AppendElement(std::move(cert));

    n = CERT_LIST_NEXT(n);
  }
  return IPC_OK();
}

// When the IPC client certs module needs to sign data using a key managed by
// the parent process, it will cause this function to be called in the parent
// process. The parent process needs to find the key corresponding to the given
// certificate and sign the given data with the given parameters.
mozilla::ipc::IPCResult IPCClientCertsParent::RecvSign(ByteArray aCert,
                                                       ByteArray aData,
                                                       ByteArray aParams,
                                                       ByteArray* aSignature) {
  SECItem certItem = {siBuffer, const_cast<uint8_t*>(aCert.data().Elements()),
                      static_cast<unsigned int>(aCert.data().Length())};
  aSignature->data().Clear();

  UniqueCERTCertificate cert(CERT_NewTempCertificate(
      CERT_GetDefaultCertDB(), &certItem, nullptr, false, true));
  if (!cert) {
    return IPC_OK();
  }
  UniqueSECKEYPrivateKey key(PK11_FindKeyByAnyCert(cert.get(), nullptr));
  if (!key) {
    return IPC_OK();
  }
  SECItem params = {siBuffer, aParams.data().Elements(),
                    static_cast<unsigned int>(aParams.data().Length())};
  SECItem* paramsPtr = aParams.data().Length() > 0 ? &params : nullptr;
  CK_MECHANISM_TYPE mechanism;
  switch (key->keyType) {
    case ecKey:
      mechanism = CKM_ECDSA;
      break;
    case rsaKey:
      mechanism = aParams.data().Length() > 0 ? CKM_RSA_PKCS_PSS : CKM_RSA_PKCS;
      break;
    default:
      return IPC_OK();
  }
  uint32_t len = PK11_SignatureLen(key.get());
  UniqueSECItem sig(::SECITEM_AllocItem(nullptr, nullptr, len));
  SECItem hash = {siBuffer, aData.data().Elements(),
                  static_cast<unsigned int>(aData.data().Length())};
  SECStatus srv =
      PK11_SignWithMechanism(key.get(), mechanism, paramsPtr, sig.get(), &hash);
  if (srv != SECSuccess) {
    return IPC_OK();
  }
  aSignature->data().AppendElements(sig->data, sig->len);
  return IPC_OK();
}

}  // namespace mozilla::psm

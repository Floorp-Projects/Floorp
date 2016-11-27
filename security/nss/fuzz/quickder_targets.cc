/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdint.h>

#include "cert.h"

#include "registry.h"

void QuickDERDecode(void *dst, const SEC_ASN1Template *tpl, const uint8_t *buf,
                    size_t len) {
  PORTCheapArenaPool pool;
  SECItem data = {siBuffer, const_cast<unsigned char *>(buf),
                  static_cast<unsigned int>(len)};

  PORT_InitCheapArena(&pool, DER_DEFAULT_CHUNKSIZE);
  (void)SEC_QuickDERDecodeItem(&pool.arena, dst, tpl, &data);
  PORT_DestroyCheapArena(&pool);
}

extern "C" int cert_fuzzing_target(const uint8_t *Data, size_t Size) {
  CERTCertificate cert;
  QuickDERDecode(&cert, SEC_SignedCertificateTemplate, Data, Size);
  return 0;
}

REGISTER_FUZZING_TARGET("cert", cert_fuzzing_target, 3072, "Certificate Import")

extern "C" int spki_fuzzing_target(const uint8_t *Data, size_t Size) {
  CERTSubjectPublicKeyInfo spki;
  QuickDERDecode(&spki, CERT_SubjectPublicKeyInfoTemplate, Data, Size);
  return 0;
}

REGISTER_FUZZING_TARGET("spki", spki_fuzzing_target, 1024, "SPKI Import")

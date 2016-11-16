/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdint.h>

#include "cert.h"

#include "registry.h"

extern "C" int cert_fuzzing_target(const uint8_t *Data, size_t Size) {
  PORTCheapArenaPool pool;
  PORT_InitCheapArena(&pool, DER_DEFAULT_CHUNKSIZE);

  CERTCertificate cert;
  SECItem data = {siBuffer, (unsigned char *)Data, (unsigned int)Size};
  SEC_QuickDERDecodeItem(&pool.arena, &cert, SEC_SignedCertificateTemplate,
                         &data);

  PORT_DestroyCheapArena(&pool);
  return 0;
}

REGISTER_FUZZING_TARGET("cert", cert_fuzzing_target, 3072, "Certificate Import")

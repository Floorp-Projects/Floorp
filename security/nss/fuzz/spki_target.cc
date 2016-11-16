/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <assert.h>
#include <stdint.h>
#include <memory>

#include "keyhi.h"
#include "pk11pub.h"

#include "registry.h"
#include "shared.h"

extern "C" int spki_fuzzing_target(const uint8_t *Data, size_t Size) {
  SECItem data = {siBuffer, (unsigned char *)Data, (unsigned int)Size};

  static std::unique_ptr<NSSDatabase> db(new NSSDatabase());
  assert(db != nullptr);

  CERTSubjectPublicKeyInfo *spki = SECKEY_DecodeDERSubjectPublicKeyInfo(&data);

  if (spki) {
    SECKEYPublicKey *key = SECKEY_ExtractPublicKey(spki);
    SECKEY_DestroyPublicKey(key);
  }

  SECKEY_DestroySubjectPublicKeyInfo(spki);

  return 0;
}

REGISTER_FUZZING_TARGET("spki", spki_fuzzing_target, 1024, "SPKI Import")

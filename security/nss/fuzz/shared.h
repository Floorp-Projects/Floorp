/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef shared_h__
#define shared_h__

#include "cert.h"
#include "nss.h"

class NSSDatabase {
 public:
  NSSDatabase() { NSS_NoDB_Init(nullptr); }
  ~NSSDatabase() { NSS_Shutdown(); }
};

void QuickDERDecode(void *dst, const SEC_ASN1Template *tpl, const uint8_t *buf,
                    size_t len) {
  PORTCheapArenaPool pool;
  SECItem data = {siBuffer, const_cast<unsigned char *>(buf),
                  static_cast<unsigned int>(len)};

  PORT_InitCheapArena(&pool, DER_DEFAULT_CHUNKSIZE);
  (void)SEC_QuickDERDecodeItem(&pool.arena, dst, tpl, &data);
  PORT_DestroyCheapArena(&pool);
}

size_t CustomMutate(std::vector<decltype(LLVMFuzzerCustomMutator) *> mutators,
                    uint8_t *Data, size_t Size, size_t MaxSize,
                    unsigned int Seed) {
  fuzzer::Random R(Seed);

  if (R.RandBool()) {
    auto idx = R(mutators.size());
    return mutators.at(idx)(Data, Size, MaxSize, Seed);
  }

  return LLVMFuzzerMutate(Data, Size, MaxSize);
}

#define ADD_CUSTOM_MUTATORS(...)                                       \
  extern "C" size_t LLVMFuzzerCustomMutator(                           \
      uint8_t *Data, size_t Size, size_t MaxSize, unsigned int Seed) { \
    return CustomMutate(__VA_ARGS__, Data, Size, MaxSize, Seed);       \
  }

#endif  // shared_h__

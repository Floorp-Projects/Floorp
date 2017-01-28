/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef shared_h__
#define shared_h__

#include <assert.h>
#include <random>
#include "cert.h"
#include "nss.h"

extern "C" size_t LLVMFuzzerMutate(uint8_t *Data, size_t Size, size_t MaxSize);
extern "C" size_t LLVMFuzzerCustomMutator(uint8_t *Data, size_t Size,
                                          size_t MaxSize, unsigned int Seed);

class NSSDatabase {
 public:
  NSSDatabase() { assert(NSS_NoDB_Init(nullptr) == SECSuccess); }
  ~NSSDatabase() { assert(NSS_Shutdown() == SECSuccess); }
};

typedef std::vector<decltype(LLVMFuzzerCustomMutator) *> Mutators;

size_t CustomMutate(Mutators &mutators, uint8_t *Data, size_t Size,
                    size_t MaxSize, unsigned int Seed) {
  std::mt19937 rng(Seed);
  static std::bernoulli_distribution bdist;

  if (bdist(rng)) {
    std::uniform_int_distribution<size_t> idist(0, mutators.size() - 1);
    return mutators.at(idist(rng))(Data, Size, MaxSize, Seed);
  }

  return LLVMFuzzerMutate(Data, Size, MaxSize);
}

#endif  // shared_h__

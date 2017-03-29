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

size_t CustomMutate(Mutators mutators, uint8_t *data, size_t size,
                    size_t max_size, unsigned int seed);

#endif  // shared_h__

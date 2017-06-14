/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FuzzingMutate.h"
#include "FuzzingTraits.h"
#include "nsDebug.h"
#include "prenv.h"
#include "SharedMemoryFuzzer.h"

#define SHMEM_FUZZER_DEFAULT_MUTATION_PROBABILITY 2
#define SHMEM_FUZZER_DEFAULT_MUTATION_FACTOR 500
#define SHMEM_FUZZER_LOG(fmt, args...)                       \
 if (SharedMemoryFuzzer::IsLoggingEnabled()) {               \
   printf_stderr("[SharedMemoryFuzzer] " fmt "\n", ## args); \
 }

namespace mozilla {
namespace ipc {

using namespace fuzzing;

/* static */
bool
SharedMemoryFuzzer::IsLoggingEnabled()
{
  static bool sInitialized = false;
  static bool sIsLoggingEnabled = false;

  if (!sInitialized) {
    sIsLoggingEnabled = !!PR_GetEnv("SHMEM_FUZZER_ENABLE_LOGGING");
    sInitialized = true;
  }
  return sIsLoggingEnabled;
}

/* static */
bool
SharedMemoryFuzzer::IsEnabled()
{
  static bool sInitialized = false;
  static bool sIsFuzzerEnabled = false;

  if (!sInitialized) {
    sIsFuzzerEnabled = !!PR_GetEnv("SHMEM_FUZZER_ENABLE");
  }
  return sIsFuzzerEnabled;
}

/* static */
uint64_t
SharedMemoryFuzzer::MutationProbability()
{
  static uint64_t sPropValue = SHMEM_FUZZER_DEFAULT_MUTATION_PROBABILITY;
  static bool sInitialized = false;

  if (sInitialized) {
    return sPropValue;
  }
  sInitialized = true;

  const char* probability = PR_GetEnv("SHMEM_FUZZER_MUTATION_PROBABILITY");
  if (probability) {
    long n = std::strtol(probability, nullptr, 10);
    if (n != 0) {
      sPropValue = n;
      return sPropValue;
    }
  }
  return sPropValue;
}

/* static */
uint64_t
SharedMemoryFuzzer::MutationFactor()
{
  static uint64_t sPropValue = SHMEM_FUZZER_DEFAULT_MUTATION_FACTOR;
  static bool sInitialized = false;

  if (sInitialized) {
    return sPropValue;
  }
  sInitialized = true;

  const char* factor = PR_GetEnv("SHMEM_FUZZER_MUTATION_FACTOR");
  if (factor) {
    long n = strtol(factor, nullptr, 10);
    if (n != 0) {
      sPropValue = n;
      return sPropValue;
    }
  }
  return sPropValue;
}

/* static */
void*
SharedMemoryFuzzer::MutateSharedMemory(void* aMemory, size_t aSize)
{
  if (!IsEnabled()) {
    return aMemory;
  }

  if (aSize == 0) {
    /* Shmem opened from foreign handle. */
    SHMEM_FUZZER_LOG("shmem is of size 0.");
    return aMemory;
  }

  if (!aMemory) {
    /* Memory space is not mapped. */
    SHMEM_FUZZER_LOG("shmem memory space is not mapped.");
    return aMemory;
  }

  // The likelihood when a value gets fuzzed of this object.
  if (!FuzzingTraits::Sometimes(MutationProbability())) {
    return aMemory;
  }

  const size_t max = FuzzingTraits::Frequency(aSize, MutationFactor());
  SHMEM_FUZZER_LOG("shmem of size: %zu / mutations: %zu", aSize, max);
  for (size_t i = 0; i < max; i++) {
    FuzzingMutate::ChangeBit((uint8_t*)aMemory, aSize);
  }
  return aMemory;
}

} // namespace ipc
} // namespace mozilla

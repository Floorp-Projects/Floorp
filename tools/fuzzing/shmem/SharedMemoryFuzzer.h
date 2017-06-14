/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SharedMemoryFuzzer_h
#define mozilla_dom_SharedMemoryFuzzer_h

namespace mozilla {
namespace ipc {

/*
 * Exposed environment variables:
 * SHMEM_FUZZER_ENABLE=1
 * SHMEM_FUZZER_ENABLE_LOGGING=1       (optional)
 * SHMEM_FUZZER_MUTATION_PROBABILITY=2 (optional)
 * SHMEM_FUZZER_MUTATION_FACTOR=500    (optional)
*/

class SharedMemoryFuzzer
{
public:
  static void* MutateSharedMemory(void* aMemory, size_t aSize);

private:
  static uint64_t MutationProbability();
  static uint64_t MutationFactor();
  static bool IsEnabled();
  static bool IsLoggingEnabled();
};

} // namespace ipc
} // namespace mozilla

#endif

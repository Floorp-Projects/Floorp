/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Provides ASan (AddressSanitizer) specific functions that are normally
 * provided through the sanitizer/asan_interface.h header installed by ASan.
 */

#ifndef mozilla_ASan_h_
#define mozilla_ASan_h_

#ifdef MOZ_ASAN
extern "C" {
  void __asan_poison_memory_region(void const volatile *addr, size_t size)
    __attribute__((visibility("default")));
  void __asan_unpoison_memory_region(void const volatile *addr, size_t size)
    __attribute__((visibility("default")));
#define ASAN_POISON_MEMORY_REGION(addr, size)   \
  __asan_poison_memory_region((addr), (size))
#define ASAN_UNPOISON_MEMORY_REGION(addr, size) \
  __asan_unpoison_memory_region((addr), (size))
}
#endif

#endif  /* mozilla_ASan_h_ */

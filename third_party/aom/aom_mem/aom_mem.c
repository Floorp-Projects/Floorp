/*
 * Copyright (c) 2016, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include "aom_mem.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "include/aom_mem_intrnl.h"
#include "aom/aom_integer.h"

#if defined(AOM_MAX_ALLOCABLE_MEMORY)
// Returns 0 in case of overflow of nmemb * size.
static int check_size_argument_overflow(uint64_t nmemb, uint64_t size) {
  const uint64_t total_size = nmemb * size;
  if (nmemb == 0) return 1;
  if (size > AOM_MAX_ALLOCABLE_MEMORY / nmemb) return 0;
  if (total_size != (size_t)total_size) return 0;
  return 1;
}
#endif

static size_t GetAlignedMallocSize(size_t size, size_t align) {
  return size + align - 1 + ADDRESS_STORAGE_SIZE;
}

static size_t *GetMallocAddressLocation(void *const mem) {
  return ((size_t *)mem) - 1;
}

static void SetActualMallocAddress(void *const mem,
                                   const void *const malloc_addr) {
  size_t *const malloc_addr_location = GetMallocAddressLocation(mem);
  *malloc_addr_location = (size_t)malloc_addr;
}

static void *GetActualMallocAddress(void *const mem) {
  const size_t *const malloc_addr_location = GetMallocAddressLocation(mem);
  return (void *)(*malloc_addr_location);
}

void *aom_memalign(size_t align, size_t size) {
  void *x = NULL;
  const size_t aligned_size = GetAlignedMallocSize(size, align);
#if defined(AOM_MAX_ALLOCABLE_MEMORY)
  if (!check_size_argument_overflow(1, aligned_size)) return NULL;
#endif
  void *const addr = malloc(aligned_size);
  if (addr) {
    x = align_addr((unsigned char *)addr + ADDRESS_STORAGE_SIZE, align);
    SetActualMallocAddress(x, addr);
  }
  return x;
}

void *aom_malloc(size_t size) { return aom_memalign(DEFAULT_ALIGNMENT, size); }

void *aom_calloc(size_t num, size_t size) {
  const size_t total_size = num * size;
  void *const x = aom_malloc(total_size);
  if (x) memset(x, 0, total_size);
  return x;
}

void aom_free(void *memblk) {
  if (memblk) {
    void *addr = GetActualMallocAddress(memblk);
    free(addr);
  }
}

void *aom_memset16(void *dest, int val, size_t length) {
  size_t i;
  uint16_t *dest16 = (uint16_t *)dest;
  for (i = 0; i < length; i++) *dest16++ = val;
  return dest;
}

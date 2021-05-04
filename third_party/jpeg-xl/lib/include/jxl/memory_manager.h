/* Copyright (c) the JPEG XL Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/** @file memory_manager.h
 * @brief Abstraction functions used by JPEG XL to allocate memory.
 */

#ifndef JXL_MEMORY_MANAGER_H_
#define JXL_MEMORY_MANAGER_H_

#include <stddef.h>

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

/**
 * Allocating function for a memory region of a given size.
 *
 * Allocates a contiguous memory region of size @p size bytes. The returned
 * memory may not be aligned to a specific size or initialized at all.
 *
 * @param opaque custom memory manager handle provided by the caller.
 * @param size in bytes of the requested memory region.
 * @returns @c 0 if the memory can not be allocated,
 * @returns pointer to the memory otherwise.
 */
typedef void* (*jpegxl_alloc_func)(void* opaque, size_t size);

/**
 * Deallocating function pointer type.
 *
 * This function @b MUST do nothing if @p address is @c 0.
 *
 * @param opaque custom memory manager handle provided by the caller.
 * @param address memory region pointer returned by ::jpegxl_alloc_func, or @c 0
 */
typedef void (*jpegxl_free_func)(void* opaque, void* address);

/**
 * Memory Manager struct.
 * These functions, when provided by the caller, will be used to handle memory
 * allocations.
 */
typedef struct JxlMemoryManagerStruct {
  /** The opaque pointer that will be passed as the first parameter to all the
   * functions in this struct. */
  void* opaque;

  /** Memory allocation function. This can be NULL if and only if also the
   * free() member in this class is NULL. All dynamic memory will be allocated
   * and freed with these functions if they are not NULL. */
  jpegxl_alloc_func alloc;
  /** Free function matching the alloc() member. */
  jpegxl_free_func free;

  /* TODO(deymo): Add cache-aligned alloc/free functions here. */
} JxlMemoryManager;

#if defined(__cplusplus) || defined(c_plusplus)
}
#endif

#endif /* JXL_MEMORY_MANAGER_H_ */

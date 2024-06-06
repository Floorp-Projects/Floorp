// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#ifndef LIB_JXL_MEMORY_MANAGER_INTERNAL_H_
#define LIB_JXL_MEMORY_MANAGER_INTERNAL_H_

// Memory allocator with support for alignment + misalignment.

#include <jxl/memory_manager.h>

#include <cstddef>
#include <memory>

#include "lib/jxl/base/compiler_specific.h"
#include "lib/jxl/base/status.h"

namespace jxl {

namespace memory_manager_internal {

// To avoid RFOs, match L2 fill size (pairs of lines); 2 x cache line size.
static constexpr size_t kAlignment = 2 * 64;
static_assert((kAlignment & (kAlignment - 1)) == 0,
              "kAlignment must be a power of 2");

// Minimum multiple for which cache set conflicts and/or loads blocked by
// preceding stores can occur.
static constexpr size_t kNumAlignmentGroups = 16;
static constexpr size_t kAlias = kNumAlignmentGroups * kAlignment;
static_assert((kNumAlignmentGroups & (kNumAlignmentGroups - 1)) == 0,
              "kNumAlignmentGroups must be a power of 2");

}  // namespace memory_manager_internal

// Initializes the memory manager instance with the passed one. The
// MemoryManager passed in |memory_manager| may be NULL or contain NULL
// functions which will be initialized with the default ones. If either alloc
// or free are NULL, then both must be NULL, otherwise this function returns an
// error.
Status MemoryManagerInit(JxlMemoryManager* self,
                         const JxlMemoryManager* memory_manager);

void* MemoryManagerAlloc(const JxlMemoryManager* memory_manager, size_t size);
void MemoryManagerFree(const JxlMemoryManager* memory_manager, void* address);

// Helper class to be used as a deleter in a unique_ptr<T> call.
class MemoryManagerDeleteHelper {
 public:
  explicit MemoryManagerDeleteHelper(const JxlMemoryManager* memory_manager)
      : memory_manager_(memory_manager) {}

  // Delete and free the passed pointer using the memory_manager.
  template <typename T>
  void operator()(T* address) const {
    if (!address) {
      return;
    }
    address->~T();
    return memory_manager_->free(memory_manager_->opaque, address);
  }

 private:
  const JxlMemoryManager* memory_manager_;
};

template <typename T>
using MemoryManagerUniquePtr = std::unique_ptr<T, MemoryManagerDeleteHelper>;

// Creates a new object T allocating it with the memory allocator into a
// unique_ptr.
template <typename T, typename... Args>
JXL_INLINE MemoryManagerUniquePtr<T> MemoryManagerMakeUnique(
    const JxlMemoryManager* memory_manager, Args&&... args) {
  T* mem =
      static_cast<T*>(memory_manager->alloc(memory_manager->opaque, sizeof(T)));
  if (!mem) {
    // Allocation error case.
    return MemoryManagerUniquePtr<T>(nullptr,
                                     MemoryManagerDeleteHelper(memory_manager));
  }
  return MemoryManagerUniquePtr<T>(new (mem) T(std::forward<Args>(args)...),
                                   MemoryManagerDeleteHelper(memory_manager));
}

// Returns recommended distance in bytes between the start of two consecutive
// rows.
size_t BytesPerRow(size_t xsize, size_t sizeof_t);

class AlignedMemory {
 public:
  AlignedMemory()
      : allocation_(nullptr), memory_manager_(nullptr), address_(nullptr) {}

  // Copy disallowed.
  AlignedMemory(const AlignedMemory& other) = delete;
  AlignedMemory& operator=(const AlignedMemory& other) = delete;

  // Custom move.
  AlignedMemory(AlignedMemory&& other) noexcept;
  AlignedMemory& operator=(AlignedMemory&& other) noexcept;

  ~AlignedMemory();

  static StatusOr<AlignedMemory> Create(JxlMemoryManager* memory_manager,
                                        size_t size);

  template <typename T>
  T* address() const {
    return reinterpret_cast<T*>(address_);
  }
  JxlMemoryManager* memory_manager() const { return memory_manager_; }

  // TODO(eustas): we can offer "actually accessible" size; it is 0-2KiB bigger
  //               than requested size, due to generous alignment;
  //               might be useful for resizeable containers (e.g. PaddedBytes)

 private:
  AlignedMemory(JxlMemoryManager* memory_manager, void* allocation);

  void* allocation_;
  JxlMemoryManager* memory_manager_;
  void* address_;
};

}  // namespace jxl

#endif  // LIB_JXL_MEMORY_MANAGER_INTERNAL_H_

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MEMORY_UNSAFE_SHARED_MEMORY_REGION_H_
#define BASE_MEMORY_UNSAFE_SHARED_MEMORY_REGION_H_

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/platform_shared_memory_region.h"
#include "base/memory/shared_memory_mapping.h"

namespace base {

// Scoped move-only handle to a region of platform shared memory. The instance
// owns the platform handle it wraps. Mappings created by this region are
// writable. These mappings remain valid even after the region handle is moved
// or destroyed.
//
// NOTE: UnsafeSharedMemoryRegion cannot be converted to a read-only region. Use
// with caution as the region will be writable to any process with a handle to
// the region.
//
// Use this if and only if the following is true:
// - You do not need to share the region as read-only, and,
// - You need to have several instances of the region simultaneously, possibly
//   in different processes, that can produce writable mappings.

class BASE_EXPORT UnsafeSharedMemoryRegion {
 public:
  using MappingType = WritableSharedMemoryMapping;
  // Creates a new UnsafeSharedMemoryRegion instance of a given size that can be
  // used for mapping writable shared memory into the virtual address space.
  //
  // This call will fail if the process does not have sufficient permissions to
  // create a shared memory region itself. See
  // mojo::CreateUnsafeSharedMemoryRegion in
  // mojo/public/cpp/base/shared_memory_utils.h for creating a shared memory
  // region from a an unprivileged process where a broker must be used.
  static UnsafeSharedMemoryRegion Create(size_t size);
  using CreateFunction = decltype(Create);

  // Returns an UnsafeSharedMemoryRegion built from a platform-specific handle
  // that was taken from another UnsafeSharedMemoryRegion instance. Returns an
  // invalid region iff the |handle| is invalid. CHECK-fails if the |handle|
  // isn't unsafe.
  // This should be used only by the code passing a handle across
  // process boundaries.
  static UnsafeSharedMemoryRegion Deserialize(
      subtle::PlatformSharedMemoryRegion handle);

  // Extracts a platform handle from the region. Ownership is transferred to the
  // returned region object.
  // This should be used only for sending the handle from the current
  // process to another.
  static subtle::PlatformSharedMemoryRegion TakeHandleForSerialization(
      UnsafeSharedMemoryRegion region);

  // Default constructor initializes an invalid instance.
  UnsafeSharedMemoryRegion();

  // Move operations are allowed.
  UnsafeSharedMemoryRegion(UnsafeSharedMemoryRegion&&);
  UnsafeSharedMemoryRegion& operator=(UnsafeSharedMemoryRegion&&);

  // Destructor closes shared memory region if valid.
  // All created mappings will remain valid.
  ~UnsafeSharedMemoryRegion();

  // Duplicates the underlying platform handle and creates a new
  // UnsafeSharedMemoryRegion instance that owns the newly created handle.
  // Returns a valid UnsafeSharedMemoryRegion on success, invalid otherwise.
  // The current region instance remains valid in any case.
  UnsafeSharedMemoryRegion Duplicate() const;

  // Maps the shared memory region into the caller's address space with write
  // access. The mapped address is guaranteed to have an alignment of
  // at least |subtle::PlatformSharedMemoryRegion::kMapMinimumAlignment|.
  // Returns a valid WritableSharedMemoryMapping instance on success, invalid
  // otherwise.
  WritableSharedMemoryMapping Map() const;

  // Same as above, but maps only |size| bytes of the shared memory region
  // starting with the given |offset|. |offset| must be aligned to value of
  // |SysInfo::VMAllocationGranularity()|. Returns an invalid mapping if
  // requested bytes are out of the region limits.
  WritableSharedMemoryMapping MapAt(off_t offset, size_t size) const;

  // Whether the underlying platform handle is valid.
  bool IsValid() const;

  // Returns the maximum mapping size that can be created from this region.
  size_t GetSize() const {
    DCHECK(IsValid());
    return handle_.GetSize();
  }

  // Returns 128-bit GUID of the region.
  const UnguessableToken& GetGUID() const {
    DCHECK(IsValid());
    return handle_.GetGUID();
  }

  // Returns a platform shared memory handle. |this| remains the owner of the
  // handle.
  subtle::PlatformSharedMemoryRegion::PlatformHandle GetPlatformHandle() const {
    DCHECK(IsValid());
    return handle_.GetPlatformHandle();
  }

 private:
  friend class SharedMemoryHooks;

  explicit UnsafeSharedMemoryRegion(subtle::PlatformSharedMemoryRegion handle);

  static void set_create_hook(CreateFunction* hook) { create_hook_ = hook; }

  static CreateFunction* create_hook_;

  subtle::PlatformSharedMemoryRegion handle_;

  DISALLOW_COPY_AND_ASSIGN(UnsafeSharedMemoryRegion);
};

}  // namespace base

#endif  // BASE_MEMORY_UNSAFE_SHARED_MEMORY_REGION_H_

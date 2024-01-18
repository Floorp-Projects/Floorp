/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef Mappable_h
#define Mappable_h

#include "mozilla/RefCounted.h"
#include "Utils.h"

/**
 * Helper class to mmap plain files.
 */
class Mappable : public mozilla::RefCounted<Mappable> {
 public:
  MOZ_DECLARE_REFCOUNTED_TYPENAME(Mappable)
  ~Mappable() {}

  MemoryRange mmap(const void* addr, size_t length, int prot, int flags,
                   off_t offset);

 private:
  void munmap(void* addr, size_t length) { ::munmap(addr, length); }
  /* Limit use of Mappable::munmap to classes that keep track of the address
   * and size of the mapping. This allows to ignore ::munmap return value. */
  friend class Mappable1stPagePtr;
  friend class LibHandle;

 public:
  /**
   * Indicate to a Mappable instance that no further mmap is going to happen.
   */
  void finalize();

  /**
   * Returns the maximum length that can be mapped from this Mappable for
   * offset = 0.
   */
  size_t GetLength() const;

  /**
   * Create a Mappable instance for the given file path.
   */
  static Mappable* Create(const char* path);

 protected:
  explicit Mappable(int fd) : fd(fd) {}

 private:
  /* File descriptor */
  AutoCloseFD fd;
};

#endif /* Mappable_h */

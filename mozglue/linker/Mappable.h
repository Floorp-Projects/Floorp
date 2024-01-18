/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef Mappable_h
#define Mappable_h

#include "mozilla/RefCounted.h"
#include "Utils.h"

/**
 * Abstract class to handle mmap()ing from various kind of entities, such as
 * plain files or Zip entries. The virtual members are meant to act as the
 * equivalent system functions, except mapped memory is always MAP_PRIVATE,
 * even though a given implementation may use something different internally.
 */
class Mappable : public mozilla::RefCounted<Mappable> {
 public:
  MOZ_DECLARE_REFCOUNTED_TYPENAME(Mappable)
  virtual ~Mappable() {}

  virtual MemoryRange mmap(const void* addr, size_t length, int prot, int flags,
                           off_t offset) = 0;

  enum Kind {
    MAPPABLE_FILE,
    MAPPABLE_SEEKABLE_ZSTREAM
  };

  virtual Kind GetKind() const = 0;

 private:
  virtual void munmap(void* addr, size_t length) { ::munmap(addr, length); }
  /* Limit use of Mappable::munmap to classes that keep track of the address
   * and size of the mapping. This allows to ignore ::munmap return value. */
  friend class Mappable1stPagePtr;
  friend class LibHandle;

 public:
  /**
   * Indicate to a Mappable instance that no further mmap is going to happen.
   */
  virtual void finalize() = 0;

  /**
   * Returns the maximum length that can be mapped from this Mappable for
   * offset = 0.
   */
  virtual size_t GetLength() const = 0;
};

/**
 * Mappable implementation for plain files
 */
class MappableFile : public Mappable {
 public:
  ~MappableFile() {}

  /**
   * Create a MappableFile instance for the given file path.
   */
  static Mappable* Create(const char* path);

  /* Inherited from Mappable */
  virtual MemoryRange mmap(const void* addr, size_t length, int prot, int flags,
                           off_t offset);
  virtual void finalize();
  virtual size_t GetLength() const;

  virtual Kind GetKind() const { return MAPPABLE_FILE; };

 protected:
  explicit MappableFile(int fd) : fd(fd) {}

 private:
  /* File descriptor */
  AutoCloseFD fd;
};

#endif /* Mappable_h */

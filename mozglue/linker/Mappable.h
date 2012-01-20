/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef Mappable_h
#define Mappable_h

#include "Zip.h"
#include "mozilla/RefPtr.h"
#include "zlib.h"

/**
 * Abstract class to handle mmap()ing from various kind of entities, such as
 * plain files or Zip entries. The virtual members are meant to act as the
 * equivalent system functions, with a few differences:
 * - mapped memory is always MAP_PRIVATE, even though a given implementation
 *   may use something different internally.
 * - memory after length and up to the end of the corresponding page is nulled
 *   out.
 */
class Mappable
{
public:
  virtual ~Mappable() { }

  virtual void *mmap(const void *addr, size_t length, int prot, int flags,
                     off_t offset) = 0;
  virtual void munmap(void *addr, size_t length) {
    ::munmap(addr, length);
  }

  /**
   * Indicate to a Mappable instance that no further mmap is going to happen.
   */
  virtual void finalize() = 0;
};

/**
 * Mappable implementation for plain files
 */
class MappableFile: public Mappable
{
public:
  ~MappableFile() { }

  /**
   * Create a MappableFile instance for the given file path.
   */
  static MappableFile *Create(const char *path);

  /* Inherited from Mappable */
  virtual void *mmap(const void *addr, size_t length, int prot, int flags, off_t offset);
  virtual void finalize();

private:
  MappableFile(int fd): fd(fd) { }

  /* File descriptor */
  AutoCloseFD fd;
};

class _MappableBuffer;

/**
 * Mappable implementation for deflated stream in a Zip archive
 */
class MappableDeflate: public Mappable
{
public:
  ~MappableDeflate();

  /**
   * Create a MappableDeflate instance for the given Zip stream. The name
   * argument is used for an appropriately named temporary file, and the Zip
   * instance is given for the MappableDeflate to keep a reference of it.
   */
  static MappableDeflate *Create(const char *name, Zip *zip, Zip::Stream *stream);

  /* Inherited from Mappable */
  virtual void *mmap(const void *addr, size_t length, int prot, int flags, off_t offset);
  virtual void finalize();

private:
  MappableDeflate(_MappableBuffer *buf, Zip *zip, Zip::Stream *stream);

  /* Zip reference */
  mozilla::RefPtr<Zip> zip;

  /* Decompression buffer */
  _MappableBuffer *buffer;

  /* Zlib data */
  z_stream zStream;
};

#endif /* Mappable_h */

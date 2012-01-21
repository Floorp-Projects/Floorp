/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef Mappable_h
#define Mappable_h

#include <sys/types.h>
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

protected:
  MappableFile(int fd): fd(fd) { }

private:
  /* File descriptor */
  AutoCloseFD fd;
};

/**
 * Mappable implementation for deflated stream in a Zip archive
 * Inflates the complete stream into a cache file.
 */
class MappableExtractFile: public MappableFile
{
public:
  ~MappableExtractFile();

  /**
   * Create a MappableExtractFile instance for the given Zip stream. The name
   * argument is used to create the cache file in the cache directory.
   */
  static MappableExtractFile *Create(const char *name, Zip::Stream *stream);

  /**
   * Returns the path of the extracted file.
   */
  char *GetPath() {
    return path;
  }
private:
  MappableExtractFile(int fd, char *path)
  : MappableFile(fd), path(path), pid(getpid()) { }

  /**
   * AutoUnlinkFile keeps track or a file name and removes (unlinks) the file
   * when the instance is destroyed.
   */
  struct AutoUnlinkFileTraits: public AutoDeleteArrayTraits<char>
  {
    static void clean(char *value)
    {
      unlink(value);
      AutoDeleteArrayTraits<char>::clean(value);
    }
  };
  typedef AutoClean<AutoUnlinkFileTraits> AutoUnlinkFile;

  /* Extracted file */
  AutoUnlinkFile path;

  /* Id of the process that initialized the instance */
  pid_t pid;
};

class _MappableBuffer;

/**
 * Mappable implementation for deflated stream in a Zip archive.
 * Inflates the mapped bits in a temporary buffer. 
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
  AutoDeletePtr<_MappableBuffer> buffer;

  /* Zlib data */
  z_stream zStream;
};

#endif /* Mappable_h */

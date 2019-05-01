/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef Zip_h
#define Zip_h

#include <cstring>
#include <stdint.h>
#include <vector>
#include <zlib.h>
#include <pthread.h>
#include "Utils.h"
#include "mozilla/Assertions.h"
#include "mozilla/RefCounted.h"
#include "mozilla/RefPtr.h"

/**
 * Forward declaration
 */
class ZipCollection;

/**
 * Class to handle access to Zip archive streams. The Zip archive is mapped
 * in memory, and streams are direct references to that mapped memory.
 * Zip files are assumed to be correctly formed. No boundary checks are
 * performed, which means hand-crafted malicious Zip archives can make the
 * code fail in bad ways. However, since the only intended use is to load
 * libraries from Zip archives, there is no interest in making this code
 * safe, since the libraries could contain malicious code anyways.
 */
class Zip : public mozilla::external::AtomicRefCounted<Zip> {
 public:
  MOZ_DECLARE_REFCOUNTED_TYPENAME(Zip)
  /**
   * Create a Zip instance for the given file name. Returns nullptr in case
   * of failure.
   */
  static already_AddRefed<Zip> Create(const char* filename);

  /**
   * Create a Zip instance using the given buffer.
   */
  static already_AddRefed<Zip> Create(void* buffer, size_t size) {
    return Create(nullptr, buffer, size);
  }

 private:
  static already_AddRefed<Zip> Create(const char* filename, void* buffer,
                                      size_t size);

  /**
   * Private constructor
   */
  Zip(const char* filename, void* buffer, size_t size);

 public:
  /**
   * Destructor
   */
  ~Zip();

  /**
   * Class used to access Zip archive item streams
   */
  class Stream {
   public:
    /**
     * Stream types
     */
    enum Type { STORE = 0, DEFLATE = 8 };

    /**
     * Constructor
     */
    Stream()
        : compressedBuf(nullptr),
          compressedSize(0),
          uncompressedSize(0),
          CRC32(0),
          type(STORE) {}

    /**
     * Getters
     */
    const void* GetBuffer() { return compressedBuf; }
    size_t GetSize() { return compressedSize; }
    size_t GetUncompressedSize() { return uncompressedSize; }
    size_t GetCRC32() { return CRC32; }
    Type GetType() { return type; }

    /**
     * Returns a z_stream for use with inflate functions using the given
     * buffer as inflate output. The caller is expected to allocate enough
     * memory for the Stream uncompressed size.
     */
    z_stream GetZStream(void* buf) {
      z_stream zStream;
      zStream.avail_in = compressedSize;
      zStream.next_in =
          reinterpret_cast<Bytef*>(const_cast<void*>(compressedBuf));
      zStream.avail_out = uncompressedSize;
      zStream.next_out = static_cast<Bytef*>(buf);
      zStream.zalloc = nullptr;
      zStream.zfree = nullptr;
      zStream.opaque = nullptr;
      return zStream;
    }

   protected:
    friend class Zip;
    const void* compressedBuf;
    size_t compressedSize;
    size_t uncompressedSize;
    size_t CRC32;
    Type type;
  };

  /**
   * Returns a stream from the Zip archive.
   */
  bool GetStream(const char* path, Stream* out) const;

  /**
   * Returns the file name of the archive
   */
  const char* GetName() const { return name; }

  /**
   * Returns whether all files have correct CRC checksum.
   */
  bool VerifyCRCs() const;

 private:
  /* File name of the archive */
  char* name;
  /* Address where the Zip archive is mapped */
  void* mapped;
  /* Size of the archive */
  size_t size;

  /**
   * Strings (file names, comments, etc.) in the Zip headers are NOT zero
   * terminated. This class is a helper around them.
   */
  class StringBuf {
   public:
    /**
     * Constructor
     */
    StringBuf(const char* buf, size_t length) : buf(buf), length(length) {}

    /**
     * Returns whether the string has the same content as the given zero
     * terminated string.
     */
    bool Equals(const char* str) const {
      return (strncmp(str, buf, length) == 0 && str[length] == '\0');
    }

   private:
    const char* buf;
    size_t length;
  };

/* All the following types need to be packed */
#pragma pack(1)
 public:
  /**
   * A Zip archive is an aggregate of entities which all start with a
   * signature giving their type. This template is to be used as a base
   * class for these entities.
   */
  template <typename T>
  class SignedEntity {
   public:
    /**
     * Equivalent to reinterpret_cast<const T *>(buf), with an additional
     * check of the signature.
     */
    static const T* validate(const void* buf) {
      const T* ret = static_cast<const T*>(buf);
      if (ret->signature == T::magic) return ret;
      return nullptr;
    }

    explicit SignedEntity(uint32_t magic) : signature(magic) {}

   private:
    le_uint32 signature;
  };

 private:
  /**
   * Header used to describe a Local File entry. The header is followed by
   * the file name and an extra field, then by the data stream.
   */
  struct LocalFile : public SignedEntity<LocalFile> {
    /* Signature for a Local File header */
    static const uint32_t magic = 0x04034b50;

    /**
     * Returns the file name
     */
    StringBuf GetName() const {
      return StringBuf(reinterpret_cast<const char*>(this) + sizeof(*this),
                       filenameSize);
    }

    /**
     * Returns a pointer to the data associated with this header
     */
    const void* GetData() const {
      return reinterpret_cast<const char*>(this) + sizeof(*this) +
             filenameSize + extraFieldSize;
    }

    le_uint16 minVersion;
    le_uint16 generalFlag;
    le_uint16 compression;
    le_uint16 lastModifiedTime;
    le_uint16 lastModifiedDate;
    le_uint32 CRC32;
    le_uint32 compressedSize;
    le_uint32 uncompressedSize;
    le_uint16 filenameSize;
    le_uint16 extraFieldSize;
  };

  /**
   * In some cases, when a zip archive is created, compressed size and CRC
   * are not known when writing the Local File header. In these cases, the
   * 3rd bit of the general flag in the Local File header is set, and there
   * is an additional header following the compressed data.
   */
  struct DataDescriptor : public SignedEntity<DataDescriptor> {
    /* Signature for a Data Descriptor header */
    static const uint32_t magic = 0x08074b50;

    le_uint32 CRC32;
    le_uint32 compressedSize;
    le_uint32 uncompressedSize;
  };

  /**
   * Header used to describe a Central Directory Entry. The header is
   * followed by the file name, an extra field, and a comment.
   */
  struct DirectoryEntry : public SignedEntity<DirectoryEntry> {
    /* Signature for a Central Directory Entry header */
    static const uint32_t magic = 0x02014b50;

    /**
     * Returns the file name
     */
    StringBuf GetName() const {
      return StringBuf(reinterpret_cast<const char*>(this) + sizeof(*this),
                       filenameSize);
    }

    /**
     * Returns  the Central Directory Entry following this one.
     */
    const DirectoryEntry* GetNext() const {
      return validate(reinterpret_cast<const char*>(this) + sizeof(*this) +
                      filenameSize + extraFieldSize + fileCommentSize);
    }

    le_uint16 creatorVersion;
    le_uint16 minVersion;
    le_uint16 generalFlag;
    le_uint16 compression;
    le_uint16 lastModifiedTime;
    le_uint16 lastModifiedDate;
    le_uint32 CRC32;
    le_uint32 compressedSize;
    le_uint32 uncompressedSize;
    le_uint16 filenameSize;
    le_uint16 extraFieldSize;
    le_uint16 fileCommentSize;
    le_uint16 diskNum;
    le_uint16 internalAttributes;
    le_uint32 externalAttributes;
    le_uint32 offset;
  };

  /**
   * Header used to describe the End of Central Directory Record.
   */
  struct CentralDirectoryEnd : public SignedEntity<CentralDirectoryEnd> {
    /* Signature for the End of Central Directory Record */
    static const uint32_t magic = 0x06054b50;

    le_uint16 diskNum;
    le_uint16 startDisk;
    le_uint16 recordsOnDisk;
    le_uint16 records;
    le_uint32 size;
    le_uint32 offset;
    le_uint16 commentSize;
  };
#pragma pack()

  /**
   * Returns the first Directory entry
   */
  const DirectoryEntry* GetFirstEntry() const;

  /* Pointer to the Local File Entry following the last one GetStream() used.
   * This is used by GetStream to avoid scanning the Directory Entries when the
   * requested entry is that one. */
  mutable const LocalFile* nextFile;

  /* Likewise for the next Directory entry */
  mutable const DirectoryEntry* nextDir;

  /* Pointer to the Directory entries */
  mutable const DirectoryEntry* entries;

  mutable pthread_mutex_t mutex;
};

/**
 * Class for bookkeeping Zip instances
 */
class ZipCollection {
 public:
  static ZipCollection Singleton;

  /**
   * Get a Zip instance for the given path. If there is an existing one
   * already, return that one, otherwise create a new one.
   */
  static already_AddRefed<Zip> GetZip(const char* path);

 protected:
  friend class Zip;
  friend class mozilla::detail::RefCounted<Zip,
                                           mozilla::detail::AtomicRefCount>;

  /**
   * Register the given Zip instance. This method is meant to be called
   * by Zip::Create.
   */
  static void Register(Zip* zip);

  /**
   * Forget about the given Zip instance. This method is meant to be called
   * by the Zip destructor.
   */
  static void Forget(const Zip* zip);

 private:
  /* Zip instances bookkept in this collection */
  std::vector<RefPtr<Zip>> zips;
};

namespace mozilla {
namespace detail {

template <>
inline void RefCounted<Zip, AtomicRefCount>::Release() const {
  MOZ_ASSERT(static_cast<int32_t>(mRefCnt) > 0);
  const auto count = --mRefCnt;
  if (count == 1) {
    // No external references are left, attempt to remove it from the
    // collection. If it's successfully removed from the collection, Release()
    // will be called with mRefCnt = 1, which will finally delete this zip.
    ZipCollection::Forget(static_cast<const Zip*>(this));
  } else if (count == 0) {
#ifdef DEBUG
    mRefCnt = detail::DEAD;
#endif
    delete static_cast<const Zip*>(this);
  }
}

template <>
inline RefCounted<Zip, AtomicRefCount>::~RefCounted() {
  MOZ_ASSERT(mRefCnt == detail::DEAD);
}

}  // namespace detail
}  // namespace mozilla

#endif /* Zip_h */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <cstdlib>
#include <algorithm>
#include "Logging.h"
#include "Zip.h"

already_AddRefed<Zip>
Zip::Create(const char *filename)
{
  /* Open and map the file in memory */
  AutoCloseFD fd(open(filename, O_RDONLY));
  if (fd == -1) {
    ERROR("Error opening %s: %s", filename, strerror(errno));
    return nullptr;
  }
  struct stat st;
  if (fstat(fd, &st) == -1) {
    ERROR("Error stating %s: %s", filename, strerror(errno));
    return nullptr;
  }
  size_t size = st.st_size;
  if (size <= sizeof(CentralDirectoryEnd)) {
    ERROR("Error reading %s: too short", filename);
    return nullptr;
  }
  void *mapped = mmap(nullptr, size, PROT_READ, MAP_SHARED, fd, 0);
  if (mapped == MAP_FAILED) {
    ERROR("Error mmapping %s: %s", filename, strerror(errno));
    return nullptr;
  }
  DEBUG_LOG("Mapped %s @%p", filename, mapped);

  return Create(filename, mapped, size);
}

already_AddRefed<Zip>
Zip::Create(const char *filename, void *mapped, size_t size)
{
  RefPtr<Zip> zip = new Zip(filename, mapped, size);

  // If neither the first Local File entry nor central directory entries
  // have been found, the zip was invalid.
  if (!zip->nextFile && !zip->entries) {
    ERROR("%s - Invalid zip", filename);
    return nullptr;
  }

  ZipCollection::Singleton.Register(zip);
  return zip.forget();
}

Zip::Zip(const char *filename, void *mapped, size_t size)
: name(filename ? strdup(filename) : nullptr)
, mapped(mapped)
, size(size)
, nextFile(LocalFile::validate(mapped)) // first Local File entry
, nextDir(nullptr)
, entries(nullptr)
{
  pthread_mutex_init(&mutex, nullptr);
  // If the first local file entry couldn't be found (which can happen
  // with optimized jars), check the first central directory entry.
  if (!nextFile)
    GetFirstEntry();
}

Zip::~Zip()
{
  if (name) {
    munmap(mapped, size);
    DEBUG_LOG("Unmapped %s @%p", name, mapped);
    free(name);
  }
  pthread_mutex_destroy(&mutex);
}

bool
Zip::GetStream(const char *path, Zip::Stream *out) const
{
  AutoLock lock(&mutex);

  DEBUG_LOG("%s - GetFile %s", name, path);
  /* Fast path: if the Local File header on store matches, we can return the
   * corresponding stream right away.
   * However, the Local File header may not contain enough information, in
   * which case the 3rd bit on the generalFlag is set. Unfortunately, this
   * bit is also set in some archives even when we do have the data (most
   * notably the android packages as built by the Mozilla build system).
   * So instead of testing the generalFlag bit, only use the fast path when
   * we haven't read the central directory entries yet, and when the
   * compressed size as defined in the header is not filled (which is a
   * normal condition for the bit to be set). */
  if (nextFile && nextFile->GetName().Equals(path) &&
      !entries && (nextFile->compressedSize != 0)) {
    DEBUG_LOG("%s - %s was next file: fast path", name, path);
    /* Fill Stream info from Local File header content */
    const char *data = reinterpret_cast<const char *>(nextFile->GetData());
    out->compressedBuf = data;
    out->compressedSize = nextFile->compressedSize;
    out->uncompressedSize = nextFile->uncompressedSize;
    out->CRC32 = nextFile->CRC32;
    out->type = static_cast<Stream::Type>(uint16_t(nextFile->compression));

    /* Find the next Local File header. It is usually simply following the
     * compressed stream, but in cases where the 3rd bit of the generalFlag
     * is set, there is a Data Descriptor header before. */
    data += nextFile->compressedSize;
    if ((nextFile->generalFlag & 0x8) && DataDescriptor::validate(data)) {
      data += sizeof(DataDescriptor);
    }
    nextFile = LocalFile::validate(data);
    return true;
  }

  /* If the directory entry we have in store doesn't match, scan the Central
   * Directory for the entry corresponding to the given path */
  if (!nextDir || !nextDir->GetName().Equals(path)) {
    const DirectoryEntry *entry = GetFirstEntry();
    DEBUG_LOG("%s - Scan directory entries in search for %s", name, path);
    while (entry && !entry->GetName().Equals(path)) {
      entry = entry->GetNext();
    }
    nextDir = entry;
  }
  if (!nextDir) {
    DEBUG_LOG("%s - Couldn't find %s", name, path);
    return false;
  }

  /* Find the Local File header corresponding to the Directory entry that
   * was found. */
  nextFile = LocalFile::validate(static_cast<const char *>(mapped)
                             + nextDir->offset);
  if (!nextFile) {
    ERROR("%s - Couldn't find the Local File header for %s", name, path);
    return false;
  }

  /* Fill Stream info from Directory entry content */
  const char *data = reinterpret_cast<const char *>(nextFile->GetData());
  out->compressedBuf = data;
  out->compressedSize = nextDir->compressedSize;
  out->uncompressedSize = nextDir->uncompressedSize;
  out->CRC32 = nextDir->CRC32;
  out->type = static_cast<Stream::Type>(uint16_t(nextDir->compression));

  /* Store the next directory entry */
  nextDir = nextDir->GetNext();
  nextFile = nullptr;
  return true;
}

const Zip::DirectoryEntry *
Zip::GetFirstEntry() const
{
  if (entries)
    return entries;

  const CentralDirectoryEnd *end = nullptr;
  const char *_end = static_cast<const char *>(mapped) + size
                     - sizeof(CentralDirectoryEnd);

  /* Scan for the Central Directory End */
  for (; _end > mapped && !end; _end--)
    end = CentralDirectoryEnd::validate(_end);
  if (!end) {
    ERROR("%s - Couldn't find end of central directory record", name);
    return nullptr;
  }

  entries = DirectoryEntry::validate(static_cast<const char *>(mapped)
                                 + end->offset);
  if (!entries) {
    ERROR("%s - Couldn't find central directory record", name);
  }
  return entries;
}

bool
Zip::VerifyCRCs() const
{
  AutoLock lock(&mutex);

  for (const DirectoryEntry *entry = GetFirstEntry();
       entry; entry = entry->GetNext()) {
    const LocalFile *file = LocalFile::validate(
        static_cast<const char *>(mapped) + entry->offset);
    uint32_t crc = crc32(0, nullptr, 0);

    DEBUG_LOG("%.*s: crc=%08x", int(entry->filenameSize),
              reinterpret_cast<const char *>(entry) + sizeof(*entry),
              uint32_t(entry->CRC32));

    if (entry->compression == Stream::Type::STORE) {
      crc = crc32(crc, static_cast<const uint8_t*>(file->GetData()),
                  entry->compressedSize);
      DEBUG_LOG(" STORE size=%d crc=%08x", int(entry->compressedSize), crc);

    } else if (entry->compression == Stream::Type::DEFLATE) {
      z_stream zstream;
      Bytef buffer[1024];
      zstream.avail_in = entry->compressedSize;
      zstream.next_in = reinterpret_cast<Bytef *>(
                        const_cast<void *>(file->GetData()));
      zstream.zalloc = nullptr;
      zstream.zfree = nullptr;
      zstream.opaque = nullptr;

      if (inflateInit2(&zstream, -MAX_WBITS) != Z_OK) {
        return false;
      }

      for (;;) {
        zstream.avail_out = sizeof(buffer);
        zstream.next_out = buffer;

        int ret = inflate(&zstream, Z_SYNC_FLUSH);
        crc = crc32(crc, buffer, sizeof(buffer) - zstream.avail_out);

        if (ret == Z_STREAM_END) {
          break;
        } else if (ret != Z_OK) {
          return false;
        }
      }

      inflateEnd(&zstream);
      DEBUG_LOG(" DEFLATE size=%d crc=%08x", int(zstream.total_out), crc);

    } else {
      MOZ_ASSERT_UNREACHABLE("Unexpected stream type");
      continue;
    }

    if (entry->CRC32 != crc) {
      return false;
    }
  }

  return true;
}

ZipCollection ZipCollection::Singleton;

static pthread_mutex_t sZipCollectionMutex = PTHREAD_MUTEX_INITIALIZER;

already_AddRefed<Zip>
ZipCollection::GetZip(const char *path)
{
  {
    AutoLock lock(&sZipCollectionMutex);
    /* Search the list of Zips we already have for a match */
    for (const auto& zip: Singleton.zips) {
      if (zip->GetName() && (strcmp(zip->GetName(), path) == 0)) {
        return RefPtr<Zip>(zip).forget();
      }
    }
  }
  return Zip::Create(path);
}

void
ZipCollection::Register(Zip *zip)
{
  AutoLock lock(&sZipCollectionMutex);
  DEBUG_LOG("ZipCollection::Register(\"%s\")", zip->GetName());
  Singleton.zips.push_back(zip);
}

void
ZipCollection::Forget(const Zip *zip)
{
  AutoLock lock(&sZipCollectionMutex);
  if (zip->refCount() > 1) {
    // Someone has acquired a reference before we had acquired the lock,
    // ignore this request.
    return;
  }
  DEBUG_LOG("ZipCollection::Forget(\"%s\")", zip->GetName());
  const auto it = std::find(Singleton.zips.begin(), Singleton.zips.end(), zip);
  if (*it == zip) {
    Singleton.zips.erase(it);
  } else {
    DEBUG_LOG("ZipCollection::Forget: didn't find \"%s\" in bookkeeping", zip->GetName());
  }
}

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <sys/mman.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "mozilla/Assertions.h"
#include "mozilla/Sprintf.h"

#include "PlatformMacros.h"
#include "AutoObjectMapper.h"

#if defined(MOZ_LINKER)
#  include <dlfcn.h>
#  include "mozilla/Types.h"
#  if defined(ANDROID)
#    include <sys/system_properties.h>
#  endif

// FIXME move these out of mozglue/linker/ElfLoader.h into their
// own header, so as to avoid conflicts arising from two definitions
// of Array
extern "C" {
MFBT_API size_t __dl_get_mappable_length(void* handle);
MFBT_API void* __dl_mmap(void* handle, void* addr, size_t length, off_t offset);
MFBT_API void __dl_munmap(void* handle, void* addr, size_t length);
}
// The following are for get_installation_lib_dir()
#  include "nsString.h"
#  include "nsDirectoryServiceUtils.h"
#  include "nsDirectoryServiceDefs.h"
#endif

// A helper function for creating failure error messages in
// AutoObjectMapper*::Map.
static void failedToMessage(void (*aLog)(const char*), const char* aHowFailed,
                            std::string aFileName) {
  char buf[300];
  SprintfLiteral(buf, "AutoObjectMapper::Map: Failed to %s \'%s\'", aHowFailed,
                 aFileName.c_str());
  buf[sizeof(buf) - 1] = 0;
  aLog(buf);
}

AutoObjectMapperPOSIX::AutoObjectMapperPOSIX(void (*aLog)(const char*))
    : mImage(nullptr), mSize(0), mLog(aLog), mIsMapped(false) {}

AutoObjectMapperPOSIX::~AutoObjectMapperPOSIX() {
  if (!mIsMapped) {
    // There's nothing to do.
    MOZ_ASSERT(!mImage);
    MOZ_ASSERT(mSize == 0);
    return;
  }
  MOZ_ASSERT(mSize > 0);
  // The following assertion doesn't necessarily have to be true,
  // but we assume (reasonably enough) that no mmap facility would
  // be crazy enough to map anything at page zero.
  MOZ_ASSERT(mImage);
  munmap(mImage, mSize);
}

bool AutoObjectMapperPOSIX::Map(/*OUT*/ void** start, /*OUT*/ size_t* length,
                                std::string fileName) {
  MOZ_ASSERT(!mIsMapped);

  int fd = open(fileName.c_str(), O_RDONLY);
  if (fd == -1) {
    failedToMessage(mLog, "open", fileName);
    return false;
  }

  struct stat st;
  int err = fstat(fd, &st);
  size_t sz = (err == 0) ? st.st_size : 0;
  if (err != 0 || sz == 0) {
    failedToMessage(mLog, "fstat", fileName);
    close(fd);
    return false;
  }

  void* image = mmap(nullptr, sz, PROT_READ, MAP_SHARED, fd, 0);
  if (image == MAP_FAILED) {
    failedToMessage(mLog, "mmap", fileName);
    close(fd);
    return false;
  }

  close(fd);
  mIsMapped = true;
  mImage = *start = image;
  mSize = *length = sz;
  return true;
}

#if defined(MOZ_LINKER)
// A helper function for AutoObjectMapperFaultyLib::Map.  Finds out
// where the installation's lib directory is, since we'll have to look
// in there to get hold of libmozglue.so.  Returned C string is heap
// allocated and the caller must deallocate it.
static char* get_installation_lib_dir() {
  nsCOMPtr<nsIProperties> directoryService(
      do_GetService(NS_DIRECTORY_SERVICE_CONTRACTID));
  if (!directoryService) {
    return nullptr;
  }
  nsCOMPtr<nsIFile> greDir;
  nsresult rv = directoryService->Get(NS_GRE_DIR, NS_GET_IID(nsIFile),
                                      getter_AddRefs(greDir));
  if (NS_FAILED(rv)) return nullptr;
  nsCString path;
  rv = greDir->GetNativePath(path);
  if (NS_FAILED(rv)) {
    return nullptr;
  }
  return strdup(path.get());
}

AutoObjectMapperFaultyLib::AutoObjectMapperFaultyLib(void (*aLog)(const char*))
    : AutoObjectMapperPOSIX(aLog), mHdl(nullptr) {}

AutoObjectMapperFaultyLib::~AutoObjectMapperFaultyLib() {
  if (mHdl) {
    // We've got an object mapped by faulty.lib.  Unmap it via faulty.lib.
    MOZ_ASSERT(mSize > 0);
    // Assert on the basis that no valid mapping would start at page zero.
    MOZ_ASSERT(mImage);
    __dl_munmap(mHdl, mImage, mSize);
    dlclose(mHdl);
    // Stop assertions in ~AutoObjectMapperPOSIX from failing.
    mImage = nullptr;
    mSize = 0;
  }
  // At this point the parent class destructor, ~AutoObjectMapperPOSIX,
  // gets called.  If that has something mapped in the normal way, it
  // will unmap it in the normal way.  Unfortunately there's no
  // obvious way to enforce the requirement that the object is mapped
  // either by faulty.lib or by the parent class, but not by both.
}

#  if defined(ANDROID)
static int GetAndroidSDKVersion() {
  static int version = 0;
  if (version) {
    return version;
  }

  char version_string[PROP_VALUE_MAX] = {'\0'};
  int len = __system_property_get("ro.build.version.sdk", version_string);
  if (len) {
    version = static_cast<int>(strtol(version_string, nullptr, 10));
  }
  return version;
}
#  endif

bool AutoObjectMapperFaultyLib::Map(/*OUT*/ void** start,
                                    /*OUT*/ size_t* length,
                                    std::string fileName) {
  MOZ_ASSERT(!mHdl);

#  if defined(ANDROID)
  if (GetAndroidSDKVersion() >= 23) {
    return AutoObjectMapperPOSIX::Map(start, length, fileName);
  }
#  endif

  if (fileName == "libmozglue.so") {
    // Do (2) in the comment above.
    char* libdir = get_installation_lib_dir();
    if (libdir) {
      fileName = std::string(libdir) + "/lib/" + fileName;
      free(libdir);
    }
    // Hand the problem off to the standard mapper.
    return AutoObjectMapperPOSIX::Map(start, length, fileName);

  } else {
    // Do cases (1) and (3) in the comment above.  We have to
    // grapple with faulty.lib directly.
    void* hdl = dlopen(fileName.c_str(), RTLD_GLOBAL | RTLD_LAZY);
    if (!hdl) {
      failedToMessage(mLog, "get handle for ELF file", fileName);
      return false;
    }

    size_t sz = __dl_get_mappable_length(hdl);
    if (sz == 0) {
      dlclose(hdl);
      failedToMessage(mLog, "get size for ELF file", fileName);
      return false;
    }

    void* image = __dl_mmap(hdl, nullptr, sz, 0);
    if (image == MAP_FAILED) {
      dlclose(hdl);
      failedToMessage(mLog, "mmap ELF file", fileName);
      return false;
    }

    mHdl = hdl;
    mImage = *start = image;
    mSize = *length = sz;
    return true;
  }
}

#endif  // defined(MOZ_LINKER)

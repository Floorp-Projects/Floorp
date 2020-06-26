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

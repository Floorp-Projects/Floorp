/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SandboxOpenedFiles.h"

#include "mozilla/Move.h"
#include "SandboxLogging.h"

#include <fcntl.h>
#include <unistd.h>

namespace mozilla {

// The default move constructor almost works, but Atomic isn't
// move-constructable and the fd needs some special handling.
SandboxOpenedFile::SandboxOpenedFile(SandboxOpenedFile&& aMoved)
: mPath(Move(aMoved.mPath))
, mMaybeFd(aMoved.TakeDesc())
, mDup(aMoved.mDup)
, mExpectError(aMoved.mExpectError)
{ }

SandboxOpenedFile::SandboxOpenedFile(const char* aPath, bool aDup)
  : mPath(aPath), mDup(aDup), mExpectError(false)
{
  MOZ_ASSERT(aPath[0] == '/', "path should be absolute");

  int fd = open(aPath, O_RDONLY | O_CLOEXEC);
  if (fd < 0) {
    mExpectError = true;
  }
  mMaybeFd = fd;
}

int
SandboxOpenedFile::GetDesc() const
{
  int fd = -1;
  if (mDup) {
    fd = mMaybeFd;
    if (fd >= 0) {
      fd = dup(fd);
      if (fd < 0) {
        SANDBOX_LOG_ERROR("dup: %s", strerror(errno));
      }
    }
  } else {
    fd = TakeDesc();
  }
  if (fd < 0 && !mExpectError) {
    SANDBOX_LOG_ERROR("unexpected multiple open of file %s", Path());
  }
  return fd;
}

SandboxOpenedFile::~SandboxOpenedFile()
{
  int fd = TakeDesc();
  if (fd >= 0) {
    close(fd);
  }
}

int
SandboxOpenedFiles::GetDesc(const char* aPath) const
{
  for (const auto& file : mFiles) {
    if (strcmp(file.Path(), aPath) == 0) {
      return file.GetDesc();
    }
  }
  SANDBOX_LOG_ERROR("attempt to open unexpected file %s", aPath);
  return -1;
}

} // namespace mozilla

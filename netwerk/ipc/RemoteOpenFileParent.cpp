/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/net/RemoteOpenFileParent.h"
#include "mozilla/unused.h"
#include "nsEscape.h"

#if !defined(XP_WIN) && !defined(MOZ_WIDGET_COCOA)
#include <fcntl.h>
#endif

namespace mozilla {
namespace net {

RemoteOpenFileParent::RemoteOpenFileParent(nsIFileURL *aURI)
  : mURI(aURI)
#if !defined(XP_WIN) && !defined(MOZ_WIDGET_COCOA)
  , mFd(-1)
#endif
{}

RemoteOpenFileParent::~RemoteOpenFileParent()
{
#if !defined(XP_WIN) && !defined(MOZ_WIDGET_COCOA)
  if (mFd != -1) {
    // close file handle now that other process has it open, else we'll leak
    // file handles in parent process
    close(mFd);
  }
#endif
}

bool
RemoteOpenFileParent::RecvAsyncOpenFile()
{
#if defined(XP_WIN) || defined(MOZ_WIDGET_COCOA)
  NS_NOTREACHED("osX and Windows shouldn't be doing IPDL here");
#else

  // TODO: make this async!

  nsAutoCString path;
  nsresult rv = mURI->GetFilePath(path);
  NS_UnescapeURL(path);
  if (NS_SUCCEEDED(rv)) {
    int fd = open(path.get(), O_RDONLY);
    if (fd != -1) {
      unused << SendFileOpened(FileDescriptor(fd), NS_OK);
      // file handle needs to stay open until it's shared with child (and IPDL
      // is async, so hasn't happened yet). Close in destructor.
      mFd = fd;
      return true;
    }
  }

  // Note: sending an invalid file descriptor currently kills the child process:
  // but that's ok for our use case (failing to open application.jar).
  unused << SendFileOpened(FileDescriptor(mFd), NS_ERROR_NOT_AVAILABLE);
#endif // OS_TYPE

  return true;
}

} // namespace net
} // namespace mozilla

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
#include <unistd.h>
#endif

namespace mozilla {
namespace net {

bool
RemoteOpenFileParent::OpenSendCloseDelete()
{
#if defined(XP_WIN) || defined(MOZ_WIDGET_COCOA)
  MOZ_CRASH("OS X and Windows shouldn't be doing IPDL here");
#else

  // TODO: make this async!

  FileDescriptor fileDescriptor;

  nsAutoCString path;
  nsresult rv = mURI->GetFilePath(path);
  NS_WARN_IF_FALSE(NS_SUCCEEDED(rv), "GetFilePath failed!");

  NS_UnescapeURL(path);

  if (NS_SUCCEEDED(rv)) {
    int fd = open(path.get(), O_RDONLY);
    if (fd == -1) {
      printf_stderr("RemoteOpenFileParent: file '%s' was not found!\n",
                    path.get());
    } else {
      fileDescriptor = FileDescriptor(fd);
    }
  }

  // Sending a potentially invalid file descriptor is just fine.
  unused << Send__delete__(this, fileDescriptor);

  if (fileDescriptor.IsValid()) {
    // close file now that other process has it open, else we'll leak fds in the
    // parent process.
    close(fileDescriptor.PlatformHandle());
  }

#endif // OS_TYPE

  return true;
}

} // namespace net
} // namespace mozilla

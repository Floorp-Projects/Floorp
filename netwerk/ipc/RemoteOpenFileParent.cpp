/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/net/RemoteOpenFileParent.h"
#include "mozilla/Unused.h"
#include "nsEscape.h"

#if !defined(XP_WIN) && !defined(MOZ_WIDGET_COCOA)
#include <fcntl.h>
#include <unistd.h>
#endif

namespace mozilla {
namespace net {

void
RemoteOpenFileParent::ActorDestroy(ActorDestroyReason aWhy)
{
  // Nothing needed here. Called right before destructor since this is a
  // non-refcounted class.
}

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
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "GetFilePath failed!");

  NS_UnescapeURL(path);

  if (NS_SUCCEEDED(rv)) {
    int fd = open(path.get(), O_RDONLY);
    if (fd == -1) {
      printf_stderr("RemoteOpenFileParent: file '%s' was not found!\n",
                    path.get());
    } else {
      fileDescriptor = FileDescriptor(fd);
      // FileDescriptor does a dup() internally, so we need to close our fd
      close(fd);
    }
  }

  // Sending a potentially invalid file descriptor is just fine.
  Unused << Send__delete__(this, fileDescriptor);

  // Current process's file descriptor is closed by FileDescriptor destructor.
#endif // OS_TYPE

  return true;
}

} // namespace net
} // namespace mozilla

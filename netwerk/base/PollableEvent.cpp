/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsSocketTransportService2.h"
#include "PollableEvent.h"
#include "mozilla/Assertions.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/Logging.h"
#include "prerror.h"
#include "prio.h"
#include "private/pprio.h"

#ifdef XP_WIN
#include "ShutdownLayer.h"
#else
#include <fcntl.h>
#define USEPIPE 1
#endif

namespace mozilla {
namespace net {

PollableEvent::PollableEvent()
  : mWriteFD(nullptr)
  , mReadFD(nullptr)
  , mSignaled(false)
{
  // create pair of prfiledesc that can be used as a poll()ble
  // signal. on windows use a localhost socket pair, and on
  // unix use a pipe.
#ifdef USEPIPE
  if (PR_CreatePipe(&mReadFD, &mWriteFD) == PR_SUCCESS) {
    // make the pipe non blocking. NSPR asserts at
    // trying to use SockOpt here
    PROsfd fd = PR_FileDesc2NativeHandle(mReadFD);
    int flags = fcntl(fd, F_GETFL, 0);
    (void)fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    fd = PR_FileDesc2NativeHandle(mWriteFD);
    flags = fcntl(fd, F_GETFL, 0);
    (void)fcntl(fd, F_SETFL, flags | O_NONBLOCK);
  } else {
    mReadFD = nullptr;
    mWriteFD = nullptr;
    SOCKET_LOG(("PollableEvent() pipe failed\n"));
  }
#else
  PRFileDesc *fd[2];
  if (PR_NewTCPSocketPair(fd) == PR_SUCCESS) {
    mReadFD = fd[0];
    mWriteFD = fd[1];

    PRSocketOptionData opt;
    DebugOnly<PRStatus> status;
    opt.option = PR_SockOpt_NoDelay;
    opt.value.no_delay = true;
    PR_SetSocketOption(mWriteFD, &opt);
    PR_SetSocketOption(mReadFD, &opt);
    opt.option = PR_SockOpt_Nonblocking;
    opt.value.non_blocking = true;
    status = PR_SetSocketOption(mWriteFD, &opt);
    MOZ_ASSERT(status == PR_SUCCESS);
    status = PR_SetSocketOption(mReadFD, &opt);
    MOZ_ASSERT(status == PR_SUCCESS);
  } else {
    SOCKET_LOG(("PollableEvent() socketpair failed\n"));
  }
#endif

  if (mReadFD && mWriteFD) {
    // prime the system to deal with races invovled in [dc]tor cycle
    SOCKET_LOG(("PollableEvent() ctor ok\n"));
    mSignaled = true;
    PR_Write(mWriteFD, "I", 1);
  }
}

PollableEvent::~PollableEvent()
{
  if (mWriteFD) {
#if defined(XP_WIN)
    mozilla::net::AttachShutdownLayer(mWriteFD);
#endif
    PR_Close(mWriteFD);
  }
  if (mReadFD) {
#if defined(XP_WIN)
    mozilla::net::AttachShutdownLayer(mReadFD);
#endif
    PR_Close(mReadFD);
  }
}

// we do not record signals on the socket thread
// because the socket thread can reliably look at its
// own runnable queue before selecting a poll time
// this is the "service the network without blocking" comment in
// nsSocketTransportService2.cpp
bool
PollableEvent::Signal()
{
  SOCKET_LOG(("PollableEvent::Signal\n"));

  if (!mWriteFD) {
    SOCKET_LOG(("PollableEvent::Signal Failed on no FD\n"));
    return false;
  }
  if (PR_GetCurrentThread() == gSocketThread) {
    SOCKET_LOG(("PollableEvent::Signal OnSocketThread nop\n"));
    return true;
  }
  if (mSignaled) {
    return true;
  }
  mSignaled = true;
  int32_t status = PR_Write(mWriteFD, "M", 1);
  if (status != 1) {
    NS_WARNING("PollableEvent::Signal Failed\n");
    SOCKET_LOG(("PollableEvent::Signal Failed\n"));
  }
  return (status == 1);
}

bool
PollableEvent::Clear()
{
  // necessary because of the "dont signal on socket thread" optimization
  MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);

  SOCKET_LOG(("PollableEvent::Clear\n"));
  mSignaled = false;
  if (!mReadFD) {
    SOCKET_LOG(("PollableEvent::Clear mReadFD is null\n"));
    return false;
  }
  char buf[2048];
  int32_t status = PR_Read(mReadFD, buf, 2048);

  if (status == 1) {
    return true;
  }
  if (status == 0) {
    SOCKET_LOG(("PollableEvent::Clear EOF!\n"));
    return false;
  }
  if (status > 1) {
    MOZ_ASSERT(false);
    SOCKET_LOG(("PollableEvent::Clear Unexpected events\n"));
    Clear();
    return true;
  }
  PRErrorCode code = PR_GetError();
  if (code == PR_WOULD_BLOCK_ERROR) {
    return true;
  }
  SOCKET_LOG(("PollableEvent::Clear unexpected error %d\n", code));
  return false;
}

} // namespace net
} // namespace mozilla

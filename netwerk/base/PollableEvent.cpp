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
#include "prnetdb.h"

#ifdef XP_WIN
#include "ShutdownLayer.h"
#else
#include <fcntl.h>
#define USEPIPE 1
#endif

namespace mozilla {
namespace net {

#ifndef USEPIPE
static PRDescIdentity sPollableEventLayerIdentity;
static PRIOMethods    sPollableEventLayerMethods;
static PRIOMethods   *sPollableEventLayerMethodsPtr = nullptr;

static void LazyInitSocket()
{
  MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);
  if (sPollableEventLayerMethodsPtr) {
    return;
  }
  sPollableEventLayerIdentity = PR_GetUniqueIdentity("PollableEvent Layer");
  sPollableEventLayerMethods = *PR_GetDefaultIOMethods();
  sPollableEventLayerMethodsPtr = &sPollableEventLayerMethods;
}

static bool NewTCPSocketPair(PRFileDesc *fd[])
{
  // this is a replacement for PR_NewTCPSocketPair that manually
  // sets the recv buffer to 64K. A windows bug (1248358)
  // can result in using an incompatible rwin and window
  // scale option on localhost pipes if not set before connect.

  PRFileDesc *listener = nullptr;
  PRFileDesc *writer = nullptr;
  PRFileDesc *reader = nullptr;
  PRSocketOptionData recvBufferOpt;
  recvBufferOpt.option = PR_SockOpt_RecvBufferSize;
  recvBufferOpt.value.recv_buffer_size = 65535;

  PRSocketOptionData nodelayOpt;
  nodelayOpt.option = PR_SockOpt_NoDelay;
  nodelayOpt.value.no_delay = true;

  PRSocketOptionData noblockOpt;
  noblockOpt.option = PR_SockOpt_Nonblocking;
  noblockOpt.value.non_blocking = true;

  listener = PR_OpenTCPSocket(PR_AF_INET);
  if (!listener) {
    goto failed;
  }
  PR_SetSocketOption(listener, &recvBufferOpt);
  PR_SetSocketOption(listener, &nodelayOpt);

  PRNetAddr listenAddr;
  memset(&listenAddr, 0, sizeof(listenAddr));
  if ((PR_InitializeNetAddr(PR_IpAddrLoopback, 0, &listenAddr) == PR_FAILURE) ||
      (PR_Bind(listener, &listenAddr) == PR_FAILURE) ||
      (PR_GetSockName(listener, &listenAddr) == PR_FAILURE) || // learn the dynamic port
      (PR_Listen(listener, 5) == PR_FAILURE)) {
    goto failed;
  }

  writer = PR_OpenTCPSocket(PR_AF_INET);
  if (!writer) {
    goto failed;
  }
  PR_SetSocketOption(writer, &recvBufferOpt);
  PR_SetSocketOption(writer, &nodelayOpt);
  PR_SetSocketOption(writer, &noblockOpt);
  PRNetAddr writerAddr;
  if (PR_InitializeNetAddr(PR_IpAddrLoopback, ntohs(listenAddr.inet.port), &writerAddr) == PR_FAILURE) {
    goto failed;
  }

  if (PR_Connect(writer, &writerAddr, PR_INTERVAL_NO_TIMEOUT) == PR_FAILURE) {
    if ((PR_GetError() != PR_IN_PROGRESS_ERROR) ||
        (PR_ConnectContinue(writer, PR_POLL_WRITE) == PR_FAILURE)) {
      goto failed;
    }
  }

  reader = PR_Accept(listener, &listenAddr, PR_INTERVAL_NO_TIMEOUT);
  if (!reader) {
    goto failed;
  }
  PR_SetSocketOption(reader, &recvBufferOpt);
  PR_SetSocketOption(reader, &nodelayOpt);
  PR_SetSocketOption(reader, &noblockOpt);
  PR_Close(listener);

  fd[0] = reader;
  fd[1] = writer;
  return true;

failed:
  if (listener) {
    PR_Close(listener);
  }
  if (reader) {
    PR_Close(reader);
  }
  if (writer) {
    PR_Close(writer);
  }
  return false;
}

#endif

PollableEvent::PollableEvent()
  : mWriteFD(nullptr)
  , mReadFD(nullptr)
  , mSignaled(false)
{
  MOZ_COUNT_CTOR(PollableEvent);
  MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);
  // create pair of prfiledesc that can be used as a poll()ble
  // signal. on windows use a localhost socket pair, and on
  // unix use a pipe.
#ifdef USEPIPE
  SOCKET_LOG(("PollableEvent() using pipe\n"));
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
  SOCKET_LOG(("PollableEvent() using socket pair\n"));
  PRFileDesc *fd[2];
  LazyInitSocket();
  if (NewTCPSocketPair(fd)) {
    mReadFD = fd[0];
    mWriteFD = fd[1];

    // compatibility with LSPs such as McAfee that assume a NSPR
    // layer for read ala the nspr Pollable Event - Bug 698882. This layer is a nop.
    PRFileDesc *topLayer =
      PR_CreateIOLayerStub(sPollableEventLayerIdentity,
                           sPollableEventLayerMethodsPtr);
    if (topLayer) {
      if (PR_PushIOLayer(fd[0], PR_TOP_IO_LAYER, topLayer) == PR_FAILURE) {
        topLayer->dtor(topLayer);
      } else {
        SOCKET_LOG(("PollableEvent() nspr layer ok\n"));
        mReadFD = topLayer;
      }
    }

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
  MOZ_COUNT_DTOR(PollableEvent);
  if (mWriteFD) {
#if defined(XP_WIN)
    AttachShutdownLayer(mWriteFD);
#endif
    PR_Close(mWriteFD);
  }
  if (mReadFD) {
#if defined(XP_WIN)
    AttachShutdownLayer(mReadFD);
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
  SOCKET_LOG(("PollableEvent::Signal PR_Write %d\n", status));
  if (status != 1) {
    NS_WARNING("PollableEvent::Signal Failed\n");
    SOCKET_LOG(("PollableEvent::Signal Failed\n"));
    mSignaled = false;
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
  SOCKET_LOG(("PollableEvent::Signal PR_Read %d\n", status));

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

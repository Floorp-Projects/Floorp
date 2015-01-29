/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_BASE_PHYSICALSOCKETSERVER_H__
#define WEBRTC_BASE_PHYSICALSOCKETSERVER_H__

#include <vector>

#include "webrtc/base/asyncfile.h"
#include "webrtc/base/scoped_ptr.h"
#include "webrtc/base/socketserver.h"
#include "webrtc/base/criticalsection.h"

#if defined(WEBRTC_POSIX)
typedef int SOCKET;
#endif // WEBRTC_POSIX

namespace rtc {

// Event constants for the Dispatcher class.
enum DispatcherEvent {
  DE_READ    = 0x0001,
  DE_WRITE   = 0x0002,
  DE_CONNECT = 0x0004,
  DE_CLOSE   = 0x0008,
  DE_ACCEPT  = 0x0010,
};

class Signaler;
#if defined(WEBRTC_POSIX)
class PosixSignalDispatcher;
#endif

class Dispatcher {
 public:
  virtual ~Dispatcher() {}
  virtual uint32 GetRequestedEvents() = 0;
  virtual void OnPreEvent(uint32 ff) = 0;
  virtual void OnEvent(uint32 ff, int err) = 0;
#if defined(WEBRTC_WIN)
  virtual WSAEVENT GetWSAEvent() = 0;
  virtual SOCKET GetSocket() = 0;
  virtual bool CheckSignalClose() = 0;
#elif defined(WEBRTC_POSIX)
  virtual int GetDescriptor() = 0;
  virtual bool IsDescriptorClosed() = 0;
#endif
};

// A socket server that provides the real sockets of the underlying OS.
class PhysicalSocketServer : public SocketServer {
 public:
  PhysicalSocketServer();
  virtual ~PhysicalSocketServer();

  // SocketFactory:
  virtual Socket* CreateSocket(int type);
  virtual Socket* CreateSocket(int family, int type);

  virtual AsyncSocket* CreateAsyncSocket(int type);
  virtual AsyncSocket* CreateAsyncSocket(int family, int type);

  // Internal Factory for Accept
  AsyncSocket* WrapSocket(SOCKET s);

  // SocketServer:
  virtual bool Wait(int cms, bool process_io);
  virtual void WakeUp();

  void Add(Dispatcher* dispatcher);
  void Remove(Dispatcher* dispatcher);

#if defined(WEBRTC_POSIX)
  AsyncFile* CreateFile(int fd);

  // Sets the function to be executed in response to the specified POSIX signal.
  // The function is executed from inside Wait() using the "self-pipe trick"--
  // regardless of which thread receives the signal--and hence can safely
  // manipulate user-level data structures.
  // "handler" may be SIG_IGN, SIG_DFL, or a user-specified function, just like
  // with signal(2).
  // Only one PhysicalSocketServer should have user-level signal handlers.
  // Dispatching signals on multiple PhysicalSocketServers is not reliable.
  // The signal mask is not modified. It is the caller's responsibily to
  // maintain it as desired.
  virtual bool SetPosixSignalHandler(int signum, void (*handler)(int));

 protected:
  Dispatcher* signal_dispatcher();
#endif

 private:
  typedef std::vector<Dispatcher*> DispatcherList;
  typedef std::vector<size_t*> IteratorList;

#if defined(WEBRTC_POSIX)
  static bool InstallSignal(int signum, void (*handler)(int));

  scoped_ptr<PosixSignalDispatcher> signal_dispatcher_;
#endif
  DispatcherList dispatchers_;
  IteratorList iterators_;
  Signaler* signal_wakeup_;
  CriticalSection crit_;
  bool fWait_;
#if defined(WEBRTC_WIN)
  WSAEVENT socket_ev_;
#endif
};

} // namespace rtc

#endif // WEBRTC_BASE_PHYSICALSOCKETSERVER_H__

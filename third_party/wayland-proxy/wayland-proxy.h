/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _wayland_proxy_h_
#define _wayland_proxy_h_

#include <poll.h>
#include <vector>
#include <fcntl.h>
#include <atomic>
#include <memory>

class ProxiedConnection;

class WaylandProxy {
 public:
  static std::unique_ptr<WaylandProxy> Create();

  // Launch an application with Wayland proxy set
  bool RunChildApplication(char* argv[]);

  // Run proxy as part of already running application
  // and set Wayland proxy display for it.
  bool RunThread();

  // Set original Wayland display env variable and clear
  // proxy display file.
  void RestoreWaylandDisplay();

  static void SetVerbose(bool aVerbose);

  ~WaylandProxy();

 private:
  bool Init();
  void Run();

  void SetWaylandProxyDisplay();
  static void* RunProxyThread(WaylandProxy* aProxy);

  bool SetupWaylandDisplays();
  bool StartProxyServer();
  bool IsChildAppTerminated();

  bool PollConnections();
  bool ProcessConnections();

  void Info(const char* aFormat, ...);
  void Warning(const char* aOperation, bool aPrintErrno = true);
  void Error(const char* aOperation, bool aPrintErrno = true);
  void ErrorPlain(const char* aFormat, ...);

 private:
  // List of all Compositor <-> Application connections
  std::vector<std::unique_ptr<ProxiedConnection>> mConnections;
  int mProxyServerSocket = -1;
  pid_t mApplicationPID = 0;
  std::atomic<bool> mThreadRunning = false;
  pthread_t mThread;

  // sockaddr_un has hardcoded max len of sun_path
  static constexpr int sMaxDisplayNameLen = 108;
  // Name of Wayland display provided by compositor
  char mWaylandDisplay[sMaxDisplayNameLen];
  // Name of Wayland display provided by us
  char mWaylandProxy[sMaxDisplayNameLen];
};

#endif  // _wayland_proxy_h_

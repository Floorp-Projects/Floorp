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

  // Set original Wayland display env variable.
  void SetWaylandDisplay();

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

 private:
  // List of all Compositor <-> Application connections
  std::vector<std::unique_ptr<ProxiedConnection>> mConnections;
  int mProxyServerSocket = -1;
  pid_t mApplicationPID = 0;
  std::atomic<bool> mThreadRunning = false;
  pthread_t mThread;
};

#endif  // _wayland_proxy_h_

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_SandboxBrokerClient_h
#define mozilla_SandboxBrokerClient_h

#include "broker/SandboxBrokerCommon.h"

#include "mozilla/Attributes.h"

// This is the client for the sandbox broker described in
// broker/SandboxBroker.h; its constructor takes the file descriptor
// returned by SandboxBroker::Create, passed to the child over IPC.
//
// The operations exposed here can be called from any thread and in
// async signal handlers, like the corresponding system calls.  The
// intended use is from a seccomp-bpf SIGSYS handler, to transparently
// replace those syscalls, but they could also be used directly.

struct stat;

namespace mozilla {

class SandboxBrokerClient final : private SandboxBrokerCommon {
 public:
  explicit SandboxBrokerClient(int aFd);
  ~SandboxBrokerClient();

  int Open(const char* aPath, int aFlags);
  int Access(const char* aPath, int aMode);
  int Stat(const char* aPath, struct stat* aStat);
  int LStat(const char* aPath, struct stat* aStat);

 private:
  int mFileDesc;

  int DoCall(const Request* aReq, const char* aPath, struct stat* aStat,
             bool expectFd);
};

} // namespace mozilla

#endif // mozilla_SandboxBrokerClient_h

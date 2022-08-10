/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_SocketProcessImpl_h
#define mozilla_net_SocketProcessImpl_h

#include "mozilla/ipc/ProcessChild.h"
#include "SocketProcessChild.h"

namespace mozilla {
namespace net {

// This class owns the subprocess instance of socket child process.
// It is instantiated as a singleton in XRE_InitChildProcess.
class SocketProcessImpl final : public mozilla::ipc::ProcessChild {
 protected:
  using ProcessChild = mozilla::ipc::ProcessChild;

 public:
  using ProcessChild::ProcessChild;
  ~SocketProcessImpl();

  bool Init(int aArgc, char* aArgv[]) override;
  void CleanUp() override;

 private:
  SocketProcessChild mSocketProcessChild;
};

}  // namespace net
}  // namespace mozilla

#endif  // mozilla_net_SocketProcessImpl_h

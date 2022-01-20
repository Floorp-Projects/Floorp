/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_psm_IPCClientCertsChild_h__
#define mozilla_psm_IPCClientCertsChild_h__

#include "mozilla/psm/PIPCClientCertsChild.h"

namespace mozilla {

namespace ipc {
class BackgroundChildImpl;
}  // namespace ipc

namespace psm {

class IPCClientCertsChild final : public PIPCClientCertsChild {
  friend class mozilla::ipc::BackgroundChildImpl;

 public:
  IPCClientCertsChild();

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(IPCClientCertsChild);

 private:
  ~IPCClientCertsChild() = default;
};

}  // namespace psm
}  // namespace mozilla

#endif

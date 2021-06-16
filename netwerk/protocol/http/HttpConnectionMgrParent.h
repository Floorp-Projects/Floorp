/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef HttpConnectionMgrParent_h__
#define HttpConnectionMgrParent_h__

#include "HttpConnectionMgrShell.h"
#include "mozilla/net/PHttpConnectionMgrParent.h"

namespace mozilla {
namespace net {

// HttpConnectionMgrParent plays the role of nsHttpConnectionMgr and delegates
// the work to the nsHttpConnectionMgr in socket process.
class HttpConnectionMgrParent final : public PHttpConnectionMgrParent,
                                      public HttpConnectionMgrShell {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_HTTPCONNECTIONMGRSHELL

  explicit HttpConnectionMgrParent() = default;

 private:
  virtual ~HttpConnectionMgrParent() = default;

  bool mShutDown{false};
};

}  // namespace net
}  // namespace mozilla

#endif  // HttpConnectionMgrParent_h__

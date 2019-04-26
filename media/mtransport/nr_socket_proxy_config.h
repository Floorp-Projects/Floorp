/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nr_socket_proxy_config__
#define nr_socket_proxy_config__

#include <memory>
#include "nsString.h"

class nsIPrincipal;

namespace mozilla {
namespace net {
class LoadInfoArgs;
}

class NrSocketProxyConfig {
 public:
  NrSocketProxyConfig(uint64_t aTabId, const nsCString& aAlpn,
                      const net::LoadInfoArgs& aArgs);
  // We need to actually write the default impl ourselves, because the compiler
  // needs to know how to destroy mPrivate in case an exception is thrown, even
  // though we disable exceptions in our build.
  NrSocketProxyConfig(NrSocketProxyConfig&& aOrig);

  ~NrSocketProxyConfig();

  uint64_t GetTabId() const;
  const nsCString& GetAlpn() const;
  const net::LoadInfoArgs& GetLoadInfoArgs() const;

 private:
  // LoadInfoArgs includes stuff that conflicts with nICEr includes.
  // Make it possible to include this header file without tripping over this
  // problem.
  class Private;
  std::unique_ptr<Private> mPrivate;
};

}  // namespace mozilla

#endif  // nr_socket_proxy_config__

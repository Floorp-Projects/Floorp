/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nr_socket_proxy_config.h"

#include "mozilla/dom/ipc/IdType.h"
#include "mozilla/net/NeckoChannelParams.h"

namespace mozilla {

class NrSocketProxyConfig::Private {
 public:
  uint64_t mTabId;
  nsCString mAlpn;
  net::LoadInfoArgs mLoadInfoArgs;
};

NrSocketProxyConfig::NrSocketProxyConfig(uint64_t aTabId,
                                         const nsCString& aAlpn,
                                         const net::LoadInfoArgs& aArgs)
    : mPrivate(new Private({aTabId, aAlpn, aArgs})) {}

NrSocketProxyConfig::NrSocketProxyConfig(NrSocketProxyConfig&& aOrig)
    : mPrivate(std::move(aOrig.mPrivate)) {}

NrSocketProxyConfig::~NrSocketProxyConfig() {}

uint64_t NrSocketProxyConfig::GetTabId() const { return mPrivate->mTabId; }

const nsCString& NrSocketProxyConfig::GetAlpn() const {
  return mPrivate->mAlpn;
}

const net::LoadInfoArgs& NrSocketProxyConfig::GetLoadInfoArgs() const {
  return mPrivate->mLoadInfoArgs;
}

}  // namespace mozilla

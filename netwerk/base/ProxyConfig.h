/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_netwerk_base_proxy_config_h
#define mozilla_netwerk_base_proxy_config_h

#include <map>

#include "nsCRT.h"
#include "nsString.h"
#include "nsTArray.h"

// NOTE: This file is inspired by Chromium's code.
// https://source.chromium.org/chromium/chromium/src/+/main:net/proxy_resolution/proxy_config.h.

namespace mozilla {
namespace net {

// ProxyServer stores the {type, host, port} of a proxy server.
// ProxyServer is immutable.
class ProxyServer final {
 public:
  enum class ProxyType {
    DIRECT = 0,
    HTTP,
    HTTPS,
    SOCKS,
    SOCKS4,
    SOCKS5,
    FTP,
    // DEFAULT is a special type used on windows only.
    DEFAULT
  };

  ProxyServer() = default;

  ProxyServer(ProxyType aType, const nsACString& aHost, int32_t aPort)
      : mType(aType), mHost(aHost), mPort(aPort) {}

  const nsCString& Host() const { return mHost; }

  int32_t Port() const { return mPort; }

  ProxyType Type() const { return mType; }

  void ToHostAndPortStr(nsACString& aOutput) {
    aOutput.Truncate();
    if (mType == ProxyType::DIRECT) {
      return;
    }

    aOutput.Assign(mHost);
    if (mPort != -1) {
      aOutput.Append(':');
      aOutput.AppendInt(mPort);
    }
  }

  bool operator==(const ProxyServer& aOther) const {
    return mType == aOther.mType && mHost == aOther.mHost &&
           mPort == aOther.mPort;
  }

  bool operator!=(const ProxyServer& aOther) const {
    return !(*this == aOther);
  }

 private:
  ProxyType mType{ProxyType::DIRECT};
  nsCString mHost;
  int32_t mPort{-1};
};

// This class includes the information about proxy configuration.
// It contains enabled proxy servers, exception list, and the url of PAC
// script.
class ProxyConfig {
 public:
  struct ProxyRules {
    ProxyRules() = default;
    ~ProxyRules() = default;

    std::map<ProxyServer::ProxyType, ProxyServer> mProxyServers;
  };

  struct ProxyBypassRules {
    ProxyBypassRules() = default;
    ~ProxyBypassRules() = default;

    CopyableTArray<nsCString> mExceptions;
  };

  ProxyConfig() = default;
  ProxyConfig(const ProxyConfig& config);
  ~ProxyConfig() = default;

  ProxyRules& Rules() { return mRules; }

  const ProxyRules& Rules() const { return mRules; }

  ProxyBypassRules& ByPassRules() { return mBypassRules; }

  const ProxyBypassRules& ByPassRules() const { return mBypassRules; }

  void SetPACUrl(const nsACString& aUrl) { mPACUrl = aUrl; }

  const nsCString& PACUrl() const { return mPACUrl; }

  static ProxyServer::ProxyType ToProxyType(const char* aType) {
    if (!aType) {
      return ProxyServer::ProxyType::DIRECT;
    }

    if (nsCRT::strcasecmp(aType, "http") == 0) {
      return ProxyServer::ProxyType::HTTP;
    }
    if (nsCRT::strcasecmp(aType, "https") == 0) {
      return ProxyServer::ProxyType::HTTPS;
    }
    if (nsCRT::strcasecmp(aType, "socks") == 0) {
      return ProxyServer::ProxyType::SOCKS;
    }
    if (nsCRT::strcasecmp(aType, "socks4") == 0) {
      return ProxyServer::ProxyType::SOCKS4;
    }
    if (nsCRT::strcasecmp(aType, "socks5") == 0) {
      return ProxyServer::ProxyType::SOCKS5;
    }
    if (nsCRT::strcasecmp(aType, "ftp") == 0) {
      return ProxyServer::ProxyType::FTP;
    }

    return ProxyServer::ProxyType::DIRECT;
  }

  static void SetProxyResult(const char* aType, const nsACString& aHostPort,
                             nsACString& aResult) {
    aResult.AssignASCII(aType);
    aResult.Append(' ');
    aResult.Append(aHostPort);
  }

  static void SetProxyResultDirect(nsACString& aResult) {
    // For whatever reason, a proxy is not to be used.
    aResult.AssignLiteral("DIRECT");
  }

  static void ProxyStrToResult(const nsACString& aSpecificProxy,
                               const nsACString& aDefaultProxy,
                               const nsACString& aSocksProxy,
                               nsACString& aResult) {
    if (!aSpecificProxy.IsEmpty()) {
      SetProxyResult("PROXY", aSpecificProxy,
                     aResult);  // Protocol-specific proxy.
    } else if (!aDefaultProxy.IsEmpty()) {
      SetProxyResult("PROXY", aDefaultProxy, aResult);  // Default proxy.
    } else if (!aSocksProxy.IsEmpty()) {
      SetProxyResult("SOCKS", aSocksProxy, aResult);  // SOCKS proxy.
    } else {
      SetProxyResultDirect(aResult);  // Direct connection.
    }
  }

  void GetProxyString(const nsACString& aScheme, nsACString& aResult) {
    SetProxyResultDirect(aResult);

    nsAutoCString specificProxy;
    nsAutoCString defaultProxy;
    nsAutoCString socksProxy;
    nsAutoCString prefix;
    ToLowerCase(aScheme, prefix);
    ProxyServer::ProxyType type = ProxyConfig::ToProxyType(prefix.get());
    for (auto& [key, value] : mRules.mProxyServers) {
      // Break the loop if we found a specific proxy.
      if (key == type) {
        value.ToHostAndPortStr(specificProxy);
        break;
      } else if (key == ProxyServer::ProxyType::DEFAULT) {
        value.ToHostAndPortStr(defaultProxy);
      } else if (key == ProxyServer::ProxyType::SOCKS) {
        value.ToHostAndPortStr(socksProxy);
      }
    }

    ProxyStrToResult(specificProxy, defaultProxy, socksProxy, aResult);
  }

 private:
  nsCString mPACUrl;
  ProxyRules mRules;
  ProxyBypassRules mBypassRules;
};

}  // namespace net
}  // namespace mozilla

#endif  // mozilla_netwerk_base_proxy_config_h

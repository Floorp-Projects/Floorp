/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsIServiceManager.h"
#include "nsNamedPipeIOLayer.h"
#include "nsSOCKSSocketProvider.h"
#include "nsSOCKSIOLayer.h"
#include "nsCOMPtr.h"
#include "nsError.h"

using mozilla::OriginAttributes;

//////////////////////////////////////////////////////////////////////////

NS_IMPL_ISUPPORTS(nsSOCKSSocketProvider, nsISocketProvider)

nsresult nsSOCKSSocketProvider::CreateV4(nsISupports* aOuter, REFNSIID aIID,
                                         void** aResult) {
  nsresult rv;
  nsCOMPtr<nsISocketProvider> inst =
      new nsSOCKSSocketProvider(NS_SOCKS_VERSION_4);
  if (!inst)
    rv = NS_ERROR_OUT_OF_MEMORY;
  else
    rv = inst->QueryInterface(aIID, aResult);
  return rv;
}

nsresult nsSOCKSSocketProvider::CreateV5(nsISupports* aOuter, REFNSIID aIID,
                                         void** aResult) {
  nsresult rv;
  nsCOMPtr<nsISocketProvider> inst =
      new nsSOCKSSocketProvider(NS_SOCKS_VERSION_5);
  if (!inst)
    rv = NS_ERROR_OUT_OF_MEMORY;
  else
    rv = inst->QueryInterface(aIID, aResult);
  return rv;
}

// Per-platform implemenation of OpenTCPSocket helper function
// Different platforms have special cases to handle

#if defined(XP_WIN)
// The proxy host on Windows may be a named pipe uri, in which
// case a named-pipe (rather than a socket) should be returned
static PRFileDesc* OpenTCPSocket(int32_t family, nsIProxyInfo* proxy) {
  PRFileDesc* sock = nullptr;

  nsAutoCString proxyHost;
  proxy->GetHost(proxyHost);
  if (IsNamedPipePath(proxyHost)) {
    sock = CreateNamedPipeLayer();
  } else {
    sock = PR_OpenTCPSocket(family);
  }

  return sock;
}
#elif defined(XP_UNIX)
// The proxy host on UNIX systems may point to a local file uri
// in which case we should create an AF_LOCAL (UNIX Domain) socket
// instead of the requested AF_INET or AF_INET6 socket.

// Normally,this socket would get thrown out and recreated later on
// with the proper family, but we want to do it early here so that
// we can enforce seccomp policy to blacklist socket(AF_INET) calls
// to prevent the content sandbox from creating network requests
static PRFileDesc* OpenTCPSocket(int32_t family, nsIProxyInfo* proxy) {
  nsAutoCString proxyHost;
  proxy->GetHost(proxyHost);
  if (StringBeginsWith(proxyHost, NS_LITERAL_CSTRING("file://"))) {
    family = AF_LOCAL;
  }

  return PR_OpenTCPSocket(family);
}
#else
// Default, pass-through to PR_OpenTCPSocket
static PRFileDesc* OpenTCPSocket(int32_t family, nsIProxyInfo*) {
  return PR_OpenTCPSocket(family);
}
#endif

NS_IMETHODIMP
nsSOCKSSocketProvider::NewSocket(int32_t family, const char* host, int32_t port,
                                 nsIProxyInfo* proxy,
                                 const OriginAttributes& originAttributes,
                                 uint32_t flags, uint32_t tlsFlags,
                                 PRFileDesc** result, nsISupports** socksInfo) {
  PRFileDesc* sock = OpenTCPSocket(family, proxy);
  if (!sock) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  nsresult rv = nsSOCKSIOLayerAddToSocket(family, host, port, proxy, mVersion,
                                          flags, tlsFlags, sock, socksInfo);
  if (NS_SUCCEEDED(rv)) {
    *result = sock;
    return NS_OK;
  }

  return NS_ERROR_SOCKET_CREATE_FAILED;
}

NS_IMETHODIMP
nsSOCKSSocketProvider::AddToSocket(int32_t family, const char* host,
                                   int32_t port, nsIProxyInfo* proxy,
                                   const OriginAttributes& originAttributes,
                                   uint32_t flags, uint32_t tlsFlags,
                                   PRFileDesc* sock, nsISupports** socksInfo) {
  nsresult rv = nsSOCKSIOLayerAddToSocket(family, host, port, proxy, mVersion,
                                          flags, tlsFlags, sock, socksInfo);

  if (NS_FAILED(rv)) rv = NS_ERROR_SOCKET_CREATE_FAILED;
  return rv;
}

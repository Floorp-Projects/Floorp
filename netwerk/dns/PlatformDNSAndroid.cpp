/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=4 sw=2 sts=2 et cin: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GetAddrInfo.h"
#include "mozilla/net/DNSPacket.h"
#include "nsIDNSService.h"
#include "mozilla/Maybe.h"
#include "mozilla/Atomics.h"
#include "mozilla/StaticPrefs_network.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <resolv.h>
#include <poll.h>
#include <dlfcn.h>
#include <android/multinetwork.h>

namespace mozilla::net {

// The first call to ResolveHTTPSRecordImpl will load the library
// and function pointers.
static Atomic<bool> sLibLoading{false};

// https://developer.android.com/ndk/reference/group/networking#android_res_nquery
// The function android_res_nquery is defined in <android/multinetwork.h>
typedef int (*android_res_nquery_ptr)(net_handle_t network, const char* dname,
                                      int ns_class, int ns_type,
                                      uint32_t flags);
static Atomic<android_res_nquery_ptr> sAndroidResNQuery;

#define LOG(msg, ...) \
  MOZ_LOG(gGetAddrInfoLog, LogLevel::Debug, ("[DNS]: " msg, ##__VA_ARGS__))

nsresult ResolveHTTPSRecordImpl(const nsACString& aHost, uint16_t aFlags,
                                TypeRecordResultType& aResult, uint32_t& aTTL) {
  DNSPacket packet;
  nsAutoCString host(aHost);
  nsAutoCString cname;
  nsresult rv;

  if (!sLibLoading.exchange(true)) {
    // We're the first call here, load the library and symbols.
    void* handle = dlopen("libandroid.so", RTLD_LAZY | RTLD_LOCAL);
    if (!handle) {
      LOG("Error loading libandroid_net %s", dlerror());
      return NS_ERROR_UNKNOWN_HOST;
    }

    auto x = dlsym(handle, "android_res_nquery");
    if (!x) {
      LOG("No android_res_nquery symbol");
    }

    sAndroidResNQuery = (android_res_nquery_ptr)x;
  }

  if (!sAndroidResNQuery) {
    LOG("nquery not loaded");
    // The library hasn't been loaded yet.
    return NS_ERROR_UNKNOWN_HOST;
  }

  LOG("resolving %s\n", host.get());
  // Perform the query
  rv = packet.FillBuffer(
      [&](unsigned char response[DNSPacket::MAX_SIZE]) -> int {
        int fd = 0;
        auto closeSocket = MakeScopeExit([&] {
          if (fd > 0) {
            close(fd);
          }
        });
        uint32_t flags = 0;
        if (aFlags & nsIDNSService::RESOLVE_BYPASS_CACHE) {
          flags = ANDROID_RESOLV_NO_CACHE_LOOKUP;
        }
        fd = sAndroidResNQuery(0, host.get(), ns_c_in,
                               nsIDNSService::RESOLVE_TYPE_HTTPSSVC, flags);

        if (fd < 0) {
          LOG("DNS query failed");
          return fd;
        }

        struct pollfd fds;
        fds.fd = fd;
        fds.events = POLLIN;  // Wait for read events

        // Wait for an event on the file descriptor
        int ret = poll(&fds, 1,
                       StaticPrefs::network_dns_native_https_timeout_android());
        if (ret <= 0) {
          LOG("poll failed %d", ret);
          return -1;
        }

        ssize_t len = recv(fd, response, DNSPacket::MAX_SIZE - 1, 0);
        if (len <= 8) {
          LOG("size too small %zd", len);
          return len < 0 ? len : -1;
        }

        // The first 8 bytes are UDP header.
        // XXX: we should consider avoiding this move somehow.
        for (int i = 0; i < len - 8; i++) {
          response[i] = response[i + 8];
        }

        return len - 8;
      });
  if (NS_FAILED(rv)) {
    LOG("failed rv");
    return rv;
  }
  packet.SetNativePacket(true);

  int32_t loopCount = 64;
  while (loopCount > 0 && aResult.is<Nothing>()) {
    loopCount--;
    DOHresp resp;
    nsClassHashtable<nsCStringHashKey, DOHresp> additionalRecords;
    rv = packet.Decode(host, TRRTYPE_HTTPSSVC, cname, true, resp, aResult,
                       additionalRecords, aTTL);
    if (NS_FAILED(rv)) {
      LOG("Decode failed %x", static_cast<uint32_t>(rv));
      return rv;
    }
    if (!cname.IsEmpty() && aResult.is<Nothing>()) {
      host = cname;
      cname.Truncate();
      continue;
    }
  }

  if (aResult.is<Nothing>()) {
    LOG("Result is nothing");
    // The call succeeded, but no HTTPS records were found.
    return NS_ERROR_UNKNOWN_HOST;
  }

  return NS_OK;
}

}  // namespace mozilla::net

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef netwerk_dns_GetAddrInfo_h
#define netwerk_dns_GetAddrInfo_h

#include "nsError.h"

#define DNS_API_PORTABLE (0)          // Portable: PR_GetAddrInfoByName()
#define DNS_API_WINDOWS_DNS_QUERY (1) // Windows: DnsQuery()

#ifdef XP_WIN
#define DNS_API DNS_API_WINDOWS_DNS_QUERY
#else
#define DNS_API DNS_API_PORTABLE
#endif

namespace mozilla {
namespace net {

class AddrInfo;

/**
 * Look up a host by name. Mostly equivalent to getaddrinfo(host, NULL, ...) of
 * RFC 3493.
 *
 * @param hostname[in] Character string defining the host name of interest
 * @param aAf[in] May be AF_INET, AF_INET6, or AF_UNSPEC. Note that AF_UNSPEC
 *     is not supported if DNS_API is DNS_API_WINDOWS_DNS_QUERY.
 * @param aFlags[in] May be either PR_AI_ADDRCONFIG or
 *     PR_AI_ADDRCONFIG | PR_AI_NOCANONNAME. Include PR_AI_NOCANONNAME to
 *     suppress the determination of the canonical name corresponding to
 *     hostname.
 * @param aAddrInfo[out] Will point to the results of the host lookup, or be
 *     null if the lookup failed.
 */
nsresult
GetAddrInfo(const char* aHost, uint16_t aAf, uint16_t aFlags, AddrInfo** aAddrInfo);

/**
 * Initialize the GetAddrInfo module.
 *
 * GetAddrInfoShutdown() should be called for every time this function is
 * called.
 */
nsresult
GetAddrInfoInit();

/**
 * Shutdown the GetAddrInfo module.
 *
 * This function should be called for every time GetAddrInfoInit() is called.
 * An assertion may throw (but is not guarenteed) if this function is called
 * too many times.
 */
nsresult
GetAddrInfoShutdown();

} // namespace net
} // namespace mozilla

#endif // netwerk_dns_GetAddrInfo_h

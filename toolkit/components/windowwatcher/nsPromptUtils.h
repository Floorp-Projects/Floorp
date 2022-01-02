/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NSPROMPTUTILS_H_
#define NSPROMPTUTILS_H_

#include "nsIHttpChannel.h"

/**
 * @file
 * This file defines some helper functions that simplify interaction
 * with authentication prompts.
 */

/**
 * Given a username (possibly in DOMAIN\user form) and password, parses the
 * domain out of the username if necessary and sets domain, username and
 * password on the auth information object.
 */
inline void NS_SetAuthInfo(nsIAuthInformation* aAuthInfo, const nsString& aUser,
                           const nsString& aPassword) {
  uint32_t flags;
  aAuthInfo->GetFlags(&flags);
  if (flags & nsIAuthInformation::NEED_DOMAIN) {
    // Domain is separated from username by a backslash
    int32_t idx = aUser.FindChar(char16_t('\\'));
    if (idx == kNotFound) {
      aAuthInfo->SetUsername(aUser);
    } else {
      aAuthInfo->SetDomain(Substring(aUser, 0, idx));
      aAuthInfo->SetUsername(Substring(aUser, idx + 1));
    }
  } else {
    aAuthInfo->SetUsername(aUser);
  }
  aAuthInfo->SetPassword(aPassword);
}

/**
 * Gets the host and port from a channel and authentication info. This is the
 * "logical" host and port for this authentication, i.e. for a proxy
 * authentication it refers to the proxy, while for a host authentication it
 * is the actual host.
 *
 * @param machineProcessing
 *        When this parameter is true, the host will be returned in ASCII
 *        (instead of UTF-8; this is relevant when IDN is used). In addition,
 *        the port will be returned as the real port even when it was not
 *        explicitly specified (when false, the port will be returned as -1 in
 *        this case)
 */
inline void NS_GetAuthHostPort(nsIChannel* aChannel,
                               nsIAuthInformation* aAuthInfo,
                               bool aMachineProcessing, nsCString& aHost,
                               int32_t* aPort) {
  nsCOMPtr<nsIURI> uri;
  nsresult rv = aChannel->GetURI(getter_AddRefs(uri));
  if (NS_FAILED(rv)) {
    return;
  }

  // Have to distinguish proxy auth and host auth here...
  uint32_t flags;
  aAuthInfo->GetFlags(&flags);
  if (flags & nsIAuthInformation::AUTH_PROXY) {
    nsCOMPtr<nsIProxiedChannel> proxied(do_QueryInterface(aChannel));
    NS_ASSERTION(proxied, "proxy auth needs nsIProxiedChannel");

    nsCOMPtr<nsIProxyInfo> info;
    proxied->GetProxyInfo(getter_AddRefs(info));
    NS_ASSERTION(info, "proxy auth needs nsIProxyInfo");

    nsAutoCString idnhost;
    info->GetHost(idnhost);
    info->GetPort(aPort);

    if (aMachineProcessing) {
      nsCOMPtr<nsIIDNService> idnService =
          do_GetService(NS_IDNSERVICE_CONTRACTID);
      if (idnService) {
        idnService->ConvertUTF8toACE(idnhost, aHost);
      } else {
        // Not much we can do here...
        aHost = idnhost;
      }
    } else {
      aHost = idnhost;
    }
  } else {
    if (aMachineProcessing) {
      uri->GetAsciiHost(aHost);
      *aPort = NS_GetRealPort(uri);
    } else {
      uri->GetHost(aHost);
      uri->GetPort(aPort);
    }
  }
}

/**
 * Creates the key for looking up passwords in the password manager. This
 * function uses the same format that Gecko functions have always used, thus
 * ensuring backwards compatibility.
 */
inline void NS_GetAuthKey(nsIChannel* aChannel, nsIAuthInformation* aAuthInfo,
                          nsCString& aKey) {
  // HTTP does this differently from other protocols
  nsCOMPtr<nsIHttpChannel> http(do_QueryInterface(aChannel));
  if (!http) {
    nsCOMPtr<nsIURI> uri;
    aChannel->GetURI(getter_AddRefs(uri));
    uri->GetPrePath(aKey);
    return;
  }

  // NOTE: For backwards-compatibility reasons, this must be the ASCII host.
  nsCString host;
  int32_t port = -1;

  NS_GetAuthHostPort(aChannel, aAuthInfo, true, host, &port);

  nsAutoString realm;
  aAuthInfo->GetRealm(realm);

  // Now assemble the key: host:port (realm)
  aKey.Append(host);
  aKey.Append(':');
  aKey.AppendInt(port);
  aKey.AppendLiteral(" (");
  AppendUTF16toUTF8(realm, aKey);
  aKey.Append(')');
}

#endif

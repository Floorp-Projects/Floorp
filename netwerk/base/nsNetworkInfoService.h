/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_nsNetworkInfoService_h
#define mozilla_net_nsNetworkInfoService_h

#include "nsISupportsImpl.h"
#include "mozilla/ErrorResult.h"

#include "nsINetworkInfoService.h"

#define NETWORKINFOSERVICE_CID \
{ 0x296d0900, 0xf8ef, 0x4df0, \
  { 0x9c, 0x35, 0xdb, 0x58, 0x62, 0xab, 0xc5, 0x8d } }

namespace mozilla {
namespace net {

class nsNetworkInfoService final
  : public nsINetworkInfoService
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSINETWORKINFOSERVICE

  nsresult Init();

  explicit nsNetworkInfoService();

private:
  virtual ~nsNetworkInfoService() = default;
};

} // namespace net
} // namespace mozilla

#endif // mozilla_dom_nsNetworkInfoService_h

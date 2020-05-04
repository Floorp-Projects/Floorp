/* vim: et ts=2 sw=2 tw=80
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsNetAddr_h__
#define nsNetAddr_h__

#include "nsINetAddr.h"
#include "mozilla/net/DNS.h"
#include "mozilla/Attributes.h"

class nsNetAddr final : public nsINetAddr {
  ~nsNetAddr() = default;

 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSINETADDR

  explicit nsNetAddr(const mozilla::net::NetAddr* addr) : mAddr(*addr) {}

 private:
  mozilla::net::NetAddr mAddr;

 protected:
  /* additional members */
};

#endif  // !nsNetAddr_h__

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef stun_socket_filter_h__
#define stun_socket_filter_h__

#include "nsISocketFilter.h"

#define NS_STUN_UDP_SOCKET_FILTER_HANDLER_CID { 0x3e43ee93, 0x829e, 0x4ea6, \
      { 0xa3, 0x4e, 0x62, 0xd9, 0xe4, 0xc9, 0xf9, 0x93 } };

class nsStunUDPSocketFilterHandler : public nsISocketFilterHandler {
public:
  // Threadsafe because we create off-main-thread, but destroy on MainThread
  // via FreeFactoryEntries()
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSISOCKETFILTERHANDLER
private:
  virtual ~nsStunUDPSocketFilterHandler() {}
};

#define NS_STUN_TCP_SOCKET_FILTER_HANDLER_CID { 0x9fea635a, 0x2fc2, 0x4d08, \
  { 0x97, 0x21, 0xd2, 0x38, 0xd3, 0xf5, 0x2f, 0x92 } };

class nsStunTCPSocketFilterHandler : public nsISocketFilterHandler {
public:
  // Threadsafe because we create off-main-thread, but destroy on MainThread
  // via FreeFactoryEntries()
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSISOCKETFILTERHANDLER
private:
  virtual ~nsStunTCPSocketFilterHandler() {}
};


#endif // stun_socket_filter_h__

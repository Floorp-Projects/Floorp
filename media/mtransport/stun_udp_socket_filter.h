/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef stun_udp_socket_filter_h__
#define stun_udp_socket_filter_h__

#include "nsIUDPSocketFilter.h"

#define NS_STUN_UDP_SOCKET_FILTER_HANDLER_CONTRACTID NS_NETWORK_UDP_SOCKET_FILTER_HANDLER_PREFIX "stun"
#define NS_STUN_UDP_SOCKET_FILTER_HANDLER_CID { 0x3e43ee93, 0x829e, 0x4ea6, \
      { 0xa3, 0x4e, 0x62, 0xd9, 0xe4, 0xc9, 0xf9, 0x93 } };

class nsStunUDPSocketFilterHandler : public nsIUDPSocketFilterHandler {
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIUDPSOCKETFILTERHANDLER
private:
  virtual ~nsStunUDPSocketFilterHandler() {}
};


#endif // stun_udp_socket_filter_h__

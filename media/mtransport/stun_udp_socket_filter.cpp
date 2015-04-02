/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <string>
#include <set>

extern "C" {
#include "nr_api.h"
#include "transport_addr.h"
#include "stun.h"
}

#include "mozilla/Attributes.h"
#include "mozilla/net/DNS.h"
#include "stun_udp_socket_filter.h"
#include "nr_socket_prsock.h"

namespace {

class NetAddressAdapter {
 public:
  MOZ_IMPLICIT NetAddressAdapter(const mozilla::net::NetAddr& netaddr)
    : addr_(ntohl(netaddr.inet.ip)),
      port_(ntohs(netaddr.inet.port)) {
    MOZ_ASSERT(netaddr.raw.family == AF_INET);
  }

  bool operator<(const NetAddressAdapter& rhs) const {
    return addr_ != rhs.addr_ ? (addr_ < rhs.addr_) : (port_ < rhs.port_);
  }

  bool operator!=(const NetAddressAdapter& rhs) const {
    return (*this < rhs) || (rhs < *this);
  }

 private:
  const uint32_t addr_;
  const uint16_t port_;
};

class PendingSTUNRequest {
 public:
  PendingSTUNRequest(const NetAddressAdapter& netaddr, const UINT12 &id)
    : id_(id),
      net_addr_(netaddr),
      is_id_set_(true) {}

  MOZ_IMPLICIT PendingSTUNRequest(const NetAddressAdapter& netaddr)
    : id_(),
      net_addr_(netaddr),
      is_id_set_(false) {}

  bool operator<(const PendingSTUNRequest& rhs) const {
    if (net_addr_ != rhs.net_addr_) {
      return net_addr_ < rhs.net_addr_;
    }

    if (!is_id_set_ && !rhs.is_id_set_) {
      // PendingSTUNRequest can be stored to set only when it has id,
      // so comparing two PendingSTUNRequst without id is not going
      // to happen.
      MOZ_CRASH();
    }

    if (!(is_id_set_ && rhs.is_id_set_)) {
      // one of operands doesn't have id, ignore the difference.
      return false;
    }

    return memcmp(id_.octet, rhs.id_.octet, sizeof(id_.octet)) < 0;
  }

 private:
  const UINT12 id_;
  const NetAddressAdapter net_addr_;
  const bool is_id_set_;
};

class STUNUDPSocketFilter : public nsIUDPSocketFilter {
 public:
  STUNUDPSocketFilter()
    : white_list_(),
      pending_requests_() {}

  NS_DECL_ISUPPORTS
  NS_DECL_NSIUDPSOCKETFILTER

 private:
  virtual ~STUNUDPSocketFilter() {}

  bool filter_incoming_packet(const mozilla::net::NetAddr *remote_addr,
                              const uint8_t *data,
                              uint32_t len);

  bool filter_outgoing_packet(const mozilla::net::NetAddr *remote_addr,
                              const uint8_t *data,
                              uint32_t len);

  std::set<NetAddressAdapter> white_list_;
  std::set<PendingSTUNRequest> pending_requests_;
  std::set<PendingSTUNRequest> response_allowed_;
};

NS_IMPL_ISUPPORTS(STUNUDPSocketFilter, nsIUDPSocketFilter)

NS_IMETHODIMP
STUNUDPSocketFilter::FilterPacket(const mozilla::net::NetAddr *remote_addr,
                                  const uint8_t *data,
                                  uint32_t len,
                                  int32_t direction,
                                  bool *result) {
  // Allowing IPv4 address only.
  if (remote_addr->raw.family != AF_INET) {
    *result = false;
    return NS_OK;
  }

  switch (direction) {
    case nsIUDPSocketFilter::SF_INCOMING:
      *result = filter_incoming_packet(remote_addr, data, len);
      break;
    case nsIUDPSocketFilter::SF_OUTGOING:
      *result = filter_outgoing_packet(remote_addr, data, len);
      break;
    default:
      MOZ_CRASH("Unknown packet direction");
  }
  return NS_OK;
}

bool STUNUDPSocketFilter::filter_incoming_packet(const mozilla::net::NetAddr *remote_addr,
                                                 const uint8_t *data, uint32_t len) {
  // Check white list
  if (white_list_.find(*remote_addr) != white_list_.end()) {
    return true;
  }

  // Check if we had sent any stun request to this destination. If we had sent a request
  // to this host, we check the transaction id, and we can add this address to whitelist.
  std::set<PendingSTUNRequest>::iterator it =
    pending_requests_.find(PendingSTUNRequest(*remote_addr));
  if (it != pending_requests_.end()) {
    if (nr_is_stun_message(reinterpret_cast<UCHAR*>(const_cast<uint8_t*>(data)), len)) {
      const nr_stun_message_header *msg = reinterpret_cast<const nr_stun_message_header*>(data);
      // If it is a STUN response message and we can match its id with one of the pending
      // requests, we can add this address into whitelist.
      if (nr_is_stun_response_message(reinterpret_cast<UCHAR*>(const_cast<uint8_t*>(data)), len)) {
        PendingSTUNRequest pending_req(*remote_addr, msg->id);
        std::set<PendingSTUNRequest>::iterator it = pending_requests_.find(pending_req);
        if (it != pending_requests_.end()) {
          pending_requests_.erase(it);
          response_allowed_.erase(pending_req);
          white_list_.insert(*remote_addr);
        }
      } else {
        // If it is a STUN message, but not a response message, we add it into response
        // allowed list and allow outgoing filter to send a response back.
        response_allowed_.insert(PendingSTUNRequest(*remote_addr, msg->id));
      }
    }
    return true;
  }

  return false;
}

bool STUNUDPSocketFilter::filter_outgoing_packet(const mozilla::net::NetAddr *remote_addr,
                                                 const uint8_t *data, uint32_t len) {
  // Check white list
  if (white_list_.find(*remote_addr) != white_list_.end()) {
    return true;
  }

  // Check if it is a stun packet. If yes, we put it into a pending list and wait for
  // response packet.
  if (nr_is_stun_request_message(reinterpret_cast<UCHAR*>(const_cast<uint8_t*>(data)), len)) {
    const nr_stun_message_header *msg = reinterpret_cast<const nr_stun_message_header*>(data);
    pending_requests_.insert(PendingSTUNRequest(*remote_addr, msg->id));
    return true;
  }

  // If it is a stun response packet, and we had received the request before, we can
  // allow it packet to pass filter.
  if (nr_is_stun_response_message(reinterpret_cast<UCHAR*>(const_cast<uint8_t*>(data)), len)) {
    const nr_stun_message_header *msg = reinterpret_cast<const nr_stun_message_header*>(data);
    std::set<PendingSTUNRequest>::iterator it =
      response_allowed_.find(PendingSTUNRequest(*remote_addr, msg->id));
    if (it != response_allowed_.end()) {
      return true;
    }
  }

  return false;
}

} // anonymous namespace

NS_IMPL_ISUPPORTS(nsStunUDPSocketFilterHandler, nsIUDPSocketFilterHandler)

NS_IMETHODIMP nsStunUDPSocketFilterHandler::NewFilter(nsIUDPSocketFilter **result)
{
  nsIUDPSocketFilter *ret = new STUNUDPSocketFilter();
  if (!ret) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  NS_ADDREF(*result = ret);
  return NS_OK;
}

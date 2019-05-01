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
#include "stun_socket_filter.h"
#include "nr_socket_prsock.h"

namespace {

class NetAddrCompare {
 public:
  bool operator()(const mozilla::net::NetAddr& lhs,
                  const mozilla::net::NetAddr& rhs) const {
    if (lhs.raw.family != rhs.raw.family) {
      return lhs.raw.family < rhs.raw.family;
    }

    switch (lhs.raw.family) {
      case AF_INET:
        if (lhs.inet.port != rhs.inet.port) {
          return lhs.inet.port < rhs.inet.port;
        }
        return lhs.inet.ip < rhs.inet.ip;
      case AF_INET6:
        if (lhs.inet6.port != rhs.inet6.port) {
          return lhs.inet6.port < rhs.inet6.port;
        }
        return memcmp(&lhs.inet6.ip, &rhs.inet6.ip, sizeof(lhs.inet6.ip)) < 0;
      default:
        MOZ_ASSERT(false);
    }
    return false;
  }
};

class PendingSTUNRequest {
 public:
  PendingSTUNRequest(const mozilla::net::NetAddr& netaddr, const UINT12& id)
      : id_(id), net_addr_(netaddr), is_id_set_(true) {}

  MOZ_IMPLICIT PendingSTUNRequest(const mozilla::net::NetAddr& netaddr)
      : id_(), net_addr_(netaddr), is_id_set_(false) {}

  bool operator<(const PendingSTUNRequest& rhs) const {
    if (NetAddrCompare()(net_addr_, rhs.net_addr_)) {
      return true;
    }

    if (NetAddrCompare()(rhs.net_addr_, net_addr_)) {
      return false;
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
  const mozilla::net::NetAddr net_addr_;
  const bool is_id_set_;
};

class STUNUDPSocketFilter : public nsISocketFilter {
 public:
  STUNUDPSocketFilter() : white_list_(), pending_requests_() {}

  // Allocated/freed and used on the PBackground IPC thread
  NS_DECL_ISUPPORTS
  NS_DECL_NSISOCKETFILTER

 private:
  virtual ~STUNUDPSocketFilter() {}

  bool filter_incoming_packet(const mozilla::net::NetAddr* remote_addr,
                              const uint8_t* data, uint32_t len);

  bool filter_outgoing_packet(const mozilla::net::NetAddr* remote_addr,
                              const uint8_t* data, uint32_t len);

  std::set<mozilla::net::NetAddr, NetAddrCompare> white_list_;
  std::set<PendingSTUNRequest> pending_requests_;
  std::set<PendingSTUNRequest> response_allowed_;
};

NS_IMPL_ISUPPORTS(STUNUDPSocketFilter, nsISocketFilter)

NS_IMETHODIMP
STUNUDPSocketFilter::FilterPacket(const mozilla::net::NetAddr* remote_addr,
                                  const uint8_t* data, uint32_t len,
                                  int32_t direction, bool* result) {
  switch (direction) {
    case nsISocketFilter::SF_INCOMING:
      *result = filter_incoming_packet(remote_addr, data, len);
      break;
    case nsISocketFilter::SF_OUTGOING:
      *result = filter_outgoing_packet(remote_addr, data, len);
      break;
    default:
      MOZ_CRASH("Unknown packet direction");
  }
  return NS_OK;
}

bool STUNUDPSocketFilter::filter_incoming_packet(
    const mozilla::net::NetAddr* remote_addr, const uint8_t* data,
    uint32_t len) {
  // Check white list
  if (white_list_.find(*remote_addr) != white_list_.end()) {
    return true;
  }

  // If it is a STUN response message and we can match its id with one of the
  // pending requests, we can add this address into whitelist.
  if (nr_is_stun_response_message(
          reinterpret_cast<UCHAR*>(const_cast<uint8_t*>(data)), len)) {
    const nr_stun_message_header* msg =
        reinterpret_cast<const nr_stun_message_header*>(data);
    PendingSTUNRequest pending_req(*remote_addr, msg->id);
    std::set<PendingSTUNRequest>::iterator it =
        pending_requests_.find(pending_req);
    if (it != pending_requests_.end()) {
      pending_requests_.erase(it);
      response_allowed_.erase(pending_req);
      white_list_.insert(*remote_addr);
      return true;
    }
  }
  // If it's an incoming STUN request we let it pass and add it to the list of
  // pending response for white listing once we answer.
  if (nr_is_stun_request_message(
          reinterpret_cast<UCHAR*>(const_cast<uint8_t*>(data)), len)) {
    const nr_stun_message_header* msg =
        reinterpret_cast<const nr_stun_message_header*>(data);
    response_allowed_.insert(PendingSTUNRequest(*remote_addr, msg->id));
    return true;
  }
  // Lastly if we have send a STUN request to the destination of this
  // packet we allow it to send us anything back in case it's for example a
  // DTLS message (but we don't white list).
  std::set<PendingSTUNRequest>::iterator it =
      pending_requests_.find(PendingSTUNRequest(*remote_addr));
  if (it != pending_requests_.end()) {
    return true;
  }

  return false;
}

bool STUNUDPSocketFilter::filter_outgoing_packet(
    const mozilla::net::NetAddr* remote_addr, const uint8_t* data,
    uint32_t len) {
  // Check white list
  if (white_list_.find(*remote_addr) != white_list_.end()) {
    return true;
  }

  // Check if it is a stun packet. If yes, we put it into a pending list and
  // wait for response packet.
  if (nr_is_stun_request_message(
          reinterpret_cast<UCHAR*>(const_cast<uint8_t*>(data)), len)) {
    const nr_stun_message_header* msg =
        reinterpret_cast<const nr_stun_message_header*>(data);
    pending_requests_.insert(PendingSTUNRequest(*remote_addr, msg->id));
    return true;
  }

  // If it is a stun response packet, and we had received the request before, we
  // can allow it packet to pass filter.
  if (nr_is_stun_response_message(
          reinterpret_cast<UCHAR*>(const_cast<uint8_t*>(data)), len)) {
    const nr_stun_message_header* msg =
        reinterpret_cast<const nr_stun_message_header*>(data);
    std::set<PendingSTUNRequest>::iterator it =
        response_allowed_.find(PendingSTUNRequest(*remote_addr, msg->id));
    if (it != response_allowed_.end()) {
      white_list_.insert(*remote_addr);
      response_allowed_.erase(it);
      return true;
    }
  }

  return false;
}

class PendingSTUNId {
 public:
  explicit PendingSTUNId(const UINT12& id) : id_(id) {}

  bool operator<(const PendingSTUNId& rhs) const {
    return memcmp(id_.octet, rhs.id_.octet, sizeof(id_.octet)) < 0;
  }

 private:
  const UINT12 id_;
};

class STUNTCPSocketFilter : public nsISocketFilter {
 public:
  STUNTCPSocketFilter()
      : white_listed_(false), pending_request_ids_(), response_allowed_ids_() {}

  // Allocated/freed and used on the PBackground IPC thread
  NS_DECL_ISUPPORTS
  NS_DECL_NSISOCKETFILTER

 private:
  virtual ~STUNTCPSocketFilter() {}

  bool filter_incoming_packet(const uint8_t* data, uint32_t len);

  bool filter_outgoing_packet(const uint8_t* data, uint32_t len);

  bool white_listed_;
  std::set<PendingSTUNId> pending_request_ids_;
  std::set<PendingSTUNId> response_allowed_ids_;
};

NS_IMPL_ISUPPORTS(STUNTCPSocketFilter, nsISocketFilter)

NS_IMETHODIMP
STUNTCPSocketFilter::FilterPacket(const mozilla::net::NetAddr* remote_addr,
                                  const uint8_t* data, uint32_t len,
                                  int32_t direction, bool* result) {
  switch (direction) {
    case nsISocketFilter::SF_INCOMING:
      *result = filter_incoming_packet(data, len);
      break;
    case nsISocketFilter::SF_OUTGOING:
      *result = filter_outgoing_packet(data, len);
      break;
    default:
      MOZ_CRASH("Unknown packet direction");
  }
  return NS_OK;
}

bool STUNTCPSocketFilter::filter_incoming_packet(const uint8_t* data,
                                                 uint32_t len) {
  // check if white listed already
  if (white_listed_) {
    return true;
  }

  UCHAR* stun = const_cast<uint8_t*>(data);
  uint32_t length = len;
  if (!nr_is_stun_message(stun, length)) {
    stun += 2;
    length -= 2;
    if (!nr_is_stun_message(stun, length)) {
      // Note: the UDP filter lets incoming packets pass, because order of
      // packets is not guaranteed and the next packet is likely an important
      // packet for DTLS (which is costly in terms of timing to wait for a
      // retransmit). This does not apply to TCP with its guaranteed order. But
      // we still let it pass, because otherwise we would have to buffer bytes
      // here until the minimum STUN request size of bytes has been received.
      return true;
    }
  }

  const nr_stun_message_header* msg =
      reinterpret_cast<const nr_stun_message_header*>(stun);

  // If it is a STUN response message and we can match its id with one of the
  // pending requests, we can add this address into whitelist.
  if (nr_is_stun_response_message(stun, length)) {
    std::set<PendingSTUNId>::iterator it =
        pending_request_ids_.find(PendingSTUNId(msg->id));
    if (it != pending_request_ids_.end()) {
      pending_request_ids_.erase(it);
      white_listed_ = true;
    }
  } else {
    // If it is a STUN message, but not a response message, we add it into
    // response allowed list and allow outgoing filter to send a response back.
    response_allowed_ids_.insert(PendingSTUNId(msg->id));
  }

  return true;
}

bool STUNTCPSocketFilter::filter_outgoing_packet(const uint8_t* data,
                                                 uint32_t len) {
  // check if white listed already
  if (white_listed_) {
    return true;
  }

  UCHAR* stun = const_cast<uint8_t*>(data);
  uint32_t length = len;
  if (!nr_is_stun_message(stun, length)) {
    stun += 2;
    length -= 2;
    if (!nr_is_stun_message(stun, length)) {
      return false;
    }
  }

  const nr_stun_message_header* msg =
      reinterpret_cast<const nr_stun_message_header*>(stun);

  // Check if it is a stun request. If yes, we put it into a pending list and
  // wait for response packet.
  if (nr_is_stun_request_message(stun, length)) {
    pending_request_ids_.insert(PendingSTUNId(msg->id));
    return true;
  }

  // If it is a stun response packet, and we had received the request before, we
  // can allow it packet to pass filter.
  if (nr_is_stun_response_message(stun, length)) {
    std::set<PendingSTUNId>::iterator it =
        response_allowed_ids_.find(PendingSTUNId(msg->id));
    if (it != response_allowed_ids_.end()) {
      response_allowed_ids_.erase(it);
      white_listed_ = true;
      return true;
    }
  }

  return false;
}

}  // anonymous namespace

NS_IMPL_ISUPPORTS(nsStunUDPSocketFilterHandler, nsISocketFilterHandler)

NS_IMETHODIMP nsStunUDPSocketFilterHandler::NewFilter(
    nsISocketFilter** result) {
  nsISocketFilter* ret = new STUNUDPSocketFilter();
  NS_ADDREF(*result = ret);
  return NS_OK;
}

NS_IMPL_ISUPPORTS(nsStunTCPSocketFilterHandler, nsISocketFilterHandler)

NS_IMETHODIMP nsStunTCPSocketFilterHandler::NewFilter(
    nsISocketFilter** result) {
  nsISocketFilter* ret = new STUNTCPSocketFilter();
  NS_ADDREF(*result = ret);
  return NS_OK;
}

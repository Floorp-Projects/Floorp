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
#if defined(MOZILLA_XPCOMRT_API)
#include "mozilla/Module.h"
#include "mozilla/ModuleUtils.h"
#endif

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
  PendingSTUNRequest(const mozilla::net::NetAddr& netaddr, const UINT12 &id)
    : id_(id),
      net_addr_(netaddr),
      is_id_set_(true) {}

  MOZ_IMPLICIT PendingSTUNRequest(const mozilla::net::NetAddr& netaddr)
    : id_(),
      net_addr_(netaddr),
      is_id_set_(false) {}

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

class STUNUDPSocketFilter : public nsIUDPSocketFilter {
 public:
  STUNUDPSocketFilter()
    : white_list_(),
      pending_requests_() {}

  // Allocated/freed and used on the PBackground IPC thread
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

  std::set<mozilla::net::NetAddr, NetAddrCompare> white_list_;
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

#if defined(MOZILLA_XPCOMRT_API)
NS_DEFINE_NAMED_CID(NS_STUN_UDP_SOCKET_FILTER_HANDLER_CID)

NS_GENERIC_FACTORY_CONSTRUCTOR(nsStunUDPSocketFilterHandler)

static const mozilla::Module::CIDEntry kCIDs[] = {
  { &kNS_STUN_UDP_SOCKET_FILTER_HANDLER_CID, false, nullptr, nsStunUDPSocketFilterHandlerConstructor },
  { nullptr }
};

static const mozilla::Module::ContractIDEntry kContracts[] = {
  { NS_STUN_UDP_SOCKET_FILTER_HANDLER_CONTRACTID, &kNS_STUN_UDP_SOCKET_FILTER_HANDLER_CID },
  { nullptr }
};

extern const mozilla::Module kStunUDPSocketFilterHandlerModule;
const mozilla::Module kStunUDPSocketFilterHandlerModule = {
  mozilla::Module::kVersion,
  kCIDs,
  kContracts
};
#endif

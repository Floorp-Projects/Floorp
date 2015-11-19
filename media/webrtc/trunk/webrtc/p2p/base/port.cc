/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/p2p/base/port.h"

#include <algorithm>
#include <vector>

#include "webrtc/p2p/base/common.h"
#include "webrtc/p2p/base/portallocator.h"
#include "webrtc/base/base64.h"
#include "webrtc/base/crc32.h"
#include "webrtc/base/helpers.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/messagedigest.h"
#include "webrtc/base/scoped_ptr.h"
#include "webrtc/base/stringencode.h"
#include "webrtc/base/stringutils.h"

namespace {

// Determines whether we have seen at least the given maximum number of
// pings fail to have a response.
inline bool TooManyFailures(
    const std::vector<uint32>& pings_since_last_response,
    uint32 maximum_failures,
    uint32 rtt_estimate,
    uint32 now) {

  // If we haven't sent that many pings, then we can't have failed that many.
  if (pings_since_last_response.size() < maximum_failures)
    return false;

  // Check if the window in which we would expect a response to the ping has
  // already elapsed.
  return pings_since_last_response[maximum_failures - 1] + rtt_estimate < now;
}

// Determines whether we have gone too long without seeing any response.
inline bool TooLongWithoutResponse(
    const std::vector<uint32>& pings_since_last_response,
    uint32 maximum_time,
    uint32 now) {

  if (pings_since_last_response.size() == 0)
    return false;

  return pings_since_last_response[0] + maximum_time < now;
}

// GICE(ICEPROTO_GOOGLE) requires different username for RTP and RTCP.
// This function generates a different username by +1 on the last character of
// the given username (|rtp_ufrag|).
std::string GetRtcpUfragFromRtpUfrag(const std::string& rtp_ufrag) {
  ASSERT(!rtp_ufrag.empty());
  if (rtp_ufrag.empty()) {
    return rtp_ufrag;
  }
  // Change the last character to the one next to it in the base64 table.
  char new_last_char;
  if (!rtc::Base64::GetNextBase64Char(rtp_ufrag[rtp_ufrag.size() - 1],
                                            &new_last_char)) {
    // Should not be here.
    ASSERT(false);
  }
  std::string rtcp_ufrag = rtp_ufrag;
  rtcp_ufrag[rtcp_ufrag.size() - 1] = new_last_char;
  ASSERT(rtcp_ufrag != rtp_ufrag);
  return rtcp_ufrag;
}

// We will restrict RTT estimates (when used for determining state) to be
// within a reasonable range.
const uint32 MINIMUM_RTT = 100;   // 0.1 seconds
const uint32 MAXIMUM_RTT = 3000;  // 3 seconds

// When we don't have any RTT data, we have to pick something reasonable.  We
// use a large value just in case the connection is really slow.
const uint32 DEFAULT_RTT = MAXIMUM_RTT;

// Computes our estimate of the RTT given the current estimate.
inline uint32 ConservativeRTTEstimate(uint32 rtt) {
  return std::max(MINIMUM_RTT, std::min(MAXIMUM_RTT, 2 * rtt));
}

// Weighting of the old rtt value to new data.
const int RTT_RATIO = 3;  // 3 : 1

// The delay before we begin checking if this port is useless.
const int kPortTimeoutDelay = 30 * 1000;  // 30 seconds

// Used by the Connection.
const uint32 MSG_DELETE = 1;
}

namespace cricket {

// TODO(ronghuawu): Use "host", "srflx", "prflx" and "relay". But this requires
// the signaling part be updated correspondingly as well.
const char LOCAL_PORT_TYPE[] = "local";
const char STUN_PORT_TYPE[] = "stun";
const char PRFLX_PORT_TYPE[] = "prflx";
const char RELAY_PORT_TYPE[] = "relay";

const char UDP_PROTOCOL_NAME[] = "udp";
const char TCP_PROTOCOL_NAME[] = "tcp";
const char SSLTCP_PROTOCOL_NAME[] = "ssltcp";

static const char* const PROTO_NAMES[] = { UDP_PROTOCOL_NAME,
                                           TCP_PROTOCOL_NAME,
                                           SSLTCP_PROTOCOL_NAME };

const char* ProtoToString(ProtocolType proto) {
  return PROTO_NAMES[proto];
}

bool StringToProto(const char* value, ProtocolType* proto) {
  for (size_t i = 0; i <= PROTO_LAST; ++i) {
    if (_stricmp(PROTO_NAMES[i], value) == 0) {
      *proto = static_cast<ProtocolType>(i);
      return true;
    }
  }
  return false;
}

// RFC 6544, TCP candidate encoding rules.
const int DISCARD_PORT = 9;
const char TCPTYPE_ACTIVE_STR[] = "active";
const char TCPTYPE_PASSIVE_STR[] = "passive";
const char TCPTYPE_SIMOPEN_STR[] = "so";

// Foundation:  An arbitrary string that is the same for two candidates
//   that have the same type, base IP address, protocol (UDP, TCP,
//   etc.), and STUN or TURN server.  If any of these are different,
//   then the foundation will be different.  Two candidate pairs with
//   the same foundation pairs are likely to have similar network
//   characteristics.  Foundations are used in the frozen algorithm.
static std::string ComputeFoundation(
    const std::string& type,
    const std::string& protocol,
    const rtc::SocketAddress& base_address) {
  std::ostringstream ost;
  ost << type << base_address.ipaddr().ToString() << protocol;
  return rtc::ToString<uint32>(rtc::ComputeCrc32(ost.str()));
}

Port::Port(rtc::Thread* thread,
           rtc::PacketSocketFactory* factory,
           rtc::Network* network,
           const rtc::IPAddress& ip,
           const std::string& username_fragment,
           const std::string& password)
    : thread_(thread),
      factory_(factory),
      send_retransmit_count_attribute_(false),
      network_(network),
      ip_(ip),
      min_port_(0),
      max_port_(0),
      component_(ICE_CANDIDATE_COMPONENT_DEFAULT),
      generation_(0),
      ice_username_fragment_(username_fragment),
      password_(password),
      timeout_delay_(kPortTimeoutDelay),
      enable_port_packets_(false),
      ice_protocol_(ICEPROTO_HYBRID),
      ice_role_(ICEROLE_UNKNOWN),
      tiebreaker_(0),
      shared_socket_(true),
      candidate_filter_(CF_ALL) {
  Construct();
}

Port::Port(rtc::Thread* thread,
           const std::string& type,
           rtc::PacketSocketFactory* factory,
           rtc::Network* network,
           const rtc::IPAddress& ip,
           uint16 min_port,
           uint16 max_port,
           const std::string& username_fragment,
           const std::string& password)
    : thread_(thread),
      factory_(factory),
      type_(type),
      send_retransmit_count_attribute_(false),
      network_(network),
      ip_(ip),
      min_port_(min_port),
      max_port_(max_port),
      component_(ICE_CANDIDATE_COMPONENT_DEFAULT),
      generation_(0),
      ice_username_fragment_(username_fragment),
      password_(password),
      timeout_delay_(kPortTimeoutDelay),
      enable_port_packets_(false),
      ice_protocol_(ICEPROTO_HYBRID),
      ice_role_(ICEROLE_UNKNOWN),
      tiebreaker_(0),
      shared_socket_(false),
      candidate_filter_(CF_ALL) {
  ASSERT(factory_ != NULL);
  Construct();
}

void Port::Construct() {
  // If the username_fragment and password are empty, we should just create one.
  if (ice_username_fragment_.empty()) {
    ASSERT(password_.empty());
    ice_username_fragment_ = rtc::CreateRandomString(ICE_UFRAG_LENGTH);
    password_ = rtc::CreateRandomString(ICE_PWD_LENGTH);
  }
  LOG_J(LS_INFO, this) << "Port created";
}

Port::~Port() {
  // Delete all of the remaining connections.  We copy the list up front
  // because each deletion will cause it to be modified.

  std::vector<Connection*> list;

  AddressMap::iterator iter = connections_.begin();
  while (iter != connections_.end()) {
    list.push_back(iter->second);
    ++iter;
  }

  for (uint32 i = 0; i < list.size(); i++)
    delete list[i];
}

Connection* Port::GetConnection(const rtc::SocketAddress& remote_addr) {
  AddressMap::const_iterator iter = connections_.find(remote_addr);
  if (iter != connections_.end())
    return iter->second;
  else
    return NULL;
}

void Port::AddAddress(const rtc::SocketAddress& address,
                      const rtc::SocketAddress& base_address,
                      const rtc::SocketAddress& related_address,
                      const std::string& protocol,
                      const std::string& tcptype,
                      const std::string& type,
                      uint32 type_preference,
                      uint32 relay_preference,
                      bool final) {
  if (protocol == TCP_PROTOCOL_NAME && type == LOCAL_PORT_TYPE) {
    ASSERT(!tcptype.empty());
  }

  Candidate c;
  c.set_id(rtc::CreateRandomString(8));
  c.set_component(component_);
  c.set_type(type);
  c.set_protocol(protocol);
  c.set_tcptype(tcptype);
  c.set_address(address);
  c.set_priority(c.GetPriority(type_preference, network_->preference(),
                               relay_preference));
  c.set_username(username_fragment());
  c.set_password(password_);
  c.set_network_name(network_->name());
  c.set_network_type(network_->type());
  c.set_generation(generation_);
  c.set_related_address(related_address);
  c.set_foundation(ComputeFoundation(type, protocol, base_address));
  candidates_.push_back(c);
  SignalCandidateReady(this, c);

  if (final) {
    SignalPortComplete(this);
  }
}

void Port::AddConnection(Connection* conn) {
  connections_[conn->remote_candidate().address()] = conn;
  conn->SignalDestroyed.connect(this, &Port::OnConnectionDestroyed);
  SignalConnectionCreated(this, conn);
}

void Port::OnReadPacket(
    const char* data, size_t size, const rtc::SocketAddress& addr,
    ProtocolType proto) {
  // If the user has enabled port packets, just hand this over.
  if (enable_port_packets_) {
    SignalReadPacket(this, data, size, addr);
    return;
  }

  // If this is an authenticated STUN request, then signal unknown address and
  // send back a proper binding response.
  rtc::scoped_ptr<IceMessage> msg;
  std::string remote_username;
  if (!GetStunMessage(data, size, addr, msg.accept(), &remote_username)) {
    LOG_J(LS_ERROR, this) << "Received non-STUN packet from unknown address ("
                          << addr.ToSensitiveString() << ")";
  } else if (!msg) {
    // STUN message handled already
  } else if (msg->type() == STUN_BINDING_REQUEST) {
    // Check for role conflicts.
    if (IsStandardIce() &&
        !MaybeIceRoleConflict(addr, msg.get(), remote_username)) {
      LOG(LS_INFO) << "Received conflicting role from the peer.";
      return;
    }

    SignalUnknownAddress(this, addr, proto, msg.get(), remote_username, false);
  } else {
    // NOTE(tschmelcher): STUN_BINDING_RESPONSE is benign. It occurs if we
    // pruned a connection for this port while it had STUN requests in flight,
    // because we then get back responses for them, which this code correctly
    // does not handle.
    if (msg->type() != STUN_BINDING_RESPONSE) {
      LOG_J(LS_ERROR, this) << "Received unexpected STUN message type ("
                            << msg->type() << ") from unknown address ("
                            << addr.ToSensitiveString() << ")";
    }
  }
}

void Port::OnReadyToSend() {
  AddressMap::iterator iter = connections_.begin();
  for (; iter != connections_.end(); ++iter) {
    iter->second->OnReadyToSend();
  }
}

size_t Port::AddPrflxCandidate(const Candidate& local) {
  candidates_.push_back(local);
  return (candidates_.size() - 1);
}

bool Port::IsStandardIce() const {
  return (ice_protocol_ == ICEPROTO_RFC5245);
}

bool Port::IsGoogleIce() const {
  return (ice_protocol_ == ICEPROTO_GOOGLE);
}

bool Port::IsHybridIce() const {
  return (ice_protocol_ == ICEPROTO_HYBRID);
}

bool Port::GetStunMessage(const char* data, size_t size,
                          const rtc::SocketAddress& addr,
                          IceMessage** out_msg, std::string* out_username) {
  // NOTE: This could clearly be optimized to avoid allocating any memory.
  //       However, at the data rates we'll be looking at on the client side,
  //       this probably isn't worth worrying about.
  ASSERT(out_msg != NULL);
  ASSERT(out_username != NULL);
  *out_msg = NULL;
  out_username->clear();

  // Don't bother parsing the packet if we can tell it's not STUN.
  // In ICE mode, all STUN packets will have a valid fingerprint.
  if (IsStandardIce() && !StunMessage::ValidateFingerprint(data, size)) {
    return false;
  }

  // Parse the request message.  If the packet is not a complete and correct
  // STUN message, then ignore it.
  rtc::scoped_ptr<IceMessage> stun_msg(new IceMessage());
  rtc::ByteBuffer buf(data, size);
  if (!stun_msg->Read(&buf) || (buf.Length() > 0)) {
    return false;
  }

  if (stun_msg->type() == STUN_BINDING_REQUEST) {
    // Check for the presence of USERNAME and MESSAGE-INTEGRITY (if ICE) first.
    // If not present, fail with a 400 Bad Request.
    if (!stun_msg->GetByteString(STUN_ATTR_USERNAME) ||
        (IsStandardIce() &&
         !stun_msg->GetByteString(STUN_ATTR_MESSAGE_INTEGRITY))) {
      LOG_J(LS_ERROR, this) << "Received STUN request without username/M-I "
                            << "from " << addr.ToSensitiveString();
      SendBindingErrorResponse(stun_msg.get(), addr, STUN_ERROR_BAD_REQUEST,
                               STUN_ERROR_REASON_BAD_REQUEST);
      return true;
    }

    // If the username is bad or unknown, fail with a 401 Unauthorized.
    std::string local_ufrag;
    std::string remote_ufrag;
    IceProtocolType remote_protocol_type;
    if (!ParseStunUsername(stun_msg.get(), &local_ufrag, &remote_ufrag,
                           &remote_protocol_type) ||
        local_ufrag != username_fragment()) {
      LOG_J(LS_ERROR, this) << "Received STUN request with bad local username "
                            << local_ufrag << " from "
                            << addr.ToSensitiveString();
      SendBindingErrorResponse(stun_msg.get(), addr, STUN_ERROR_UNAUTHORIZED,
                               STUN_ERROR_REASON_UNAUTHORIZED);
      return true;
    }

    // Port is initialized to GOOGLE-ICE protocol type. If pings from remote
    // are received before the signal message, protocol type may be different.
    // Based on the STUN username, we can determine what's the remote protocol.
    // This also enables us to send the response back using the same protocol
    // as the request.
    if (IsHybridIce()) {
      SetIceProtocolType(remote_protocol_type);
    }

    // If ICE, and the MESSAGE-INTEGRITY is bad, fail with a 401 Unauthorized
    if (IsStandardIce() &&
        !stun_msg->ValidateMessageIntegrity(data, size, password_)) {
      LOG_J(LS_ERROR, this) << "Received STUN request with bad M-I "
                            << "from " << addr.ToSensitiveString()
                            << ", password_=" << password_;
      SendBindingErrorResponse(stun_msg.get(), addr, STUN_ERROR_UNAUTHORIZED,
                               STUN_ERROR_REASON_UNAUTHORIZED);
      return true;
    }
    out_username->assign(remote_ufrag);
  } else if ((stun_msg->type() == STUN_BINDING_RESPONSE) ||
             (stun_msg->type() == STUN_BINDING_ERROR_RESPONSE)) {
    if (stun_msg->type() == STUN_BINDING_ERROR_RESPONSE) {
      if (const StunErrorCodeAttribute* error_code = stun_msg->GetErrorCode()) {
        LOG_J(LS_ERROR, this) << "Received STUN binding error:"
                              << " class=" << error_code->eclass()
                              << " number=" << error_code->number()
                              << " reason='" << error_code->reason() << "'"
                              << " from " << addr.ToSensitiveString();
        // Return message to allow error-specific processing
      } else {
        LOG_J(LS_ERROR, this) << "Received STUN binding error without a error "
                              << "code from " << addr.ToSensitiveString();
        return true;
      }
    }
    // NOTE: Username should not be used in verifying response messages.
    out_username->clear();
  } else if (stun_msg->type() == STUN_BINDING_INDICATION) {
    LOG_J(LS_VERBOSE, this) << "Received STUN binding indication:"
                            << " from " << addr.ToSensitiveString();
    out_username->clear();
    // No stun attributes will be verified, if it's stun indication message.
    // Returning from end of the this method.
  } else {
    LOG_J(LS_ERROR, this) << "Received STUN packet with invalid type ("
                          << stun_msg->type() << ") from "
                          << addr.ToSensitiveString();
    return true;
  }

  // Return the STUN message found.
  *out_msg = stun_msg.release();
  return true;
}

bool Port::IsCompatibleAddress(const rtc::SocketAddress& addr) {
  int family = ip().family();
  // We use single-stack sockets, so families must match.
  if (addr.family() != family) {
    return false;
  }
  // Link-local IPv6 ports can only connect to other link-local IPv6 ports.
  if (family == AF_INET6 && (IPIsPrivate(ip()) != IPIsPrivate(addr.ipaddr()))) {
    return false;
  }
  return true;
}

bool Port::ParseStunUsername(const StunMessage* stun_msg,
                             std::string* local_ufrag,
                             std::string* remote_ufrag,
                             IceProtocolType* remote_protocol_type) const {
  // The packet must include a username that either begins or ends with our
  // fragment.  It should begin with our fragment if it is a request and it
  // should end with our fragment if it is a response.
  local_ufrag->clear();
  remote_ufrag->clear();
  const StunByteStringAttribute* username_attr =
        stun_msg->GetByteString(STUN_ATTR_USERNAME);
  if (username_attr == NULL)
    return false;

  const std::string username_attr_str = username_attr->GetString();
  size_t colon_pos = username_attr_str.find(":");
  // If we are in hybrid mode set the appropriate ice protocol type based on
  // the username argument style.
  if (IsHybridIce()) {
    *remote_protocol_type = (colon_pos != std::string::npos) ?
        ICEPROTO_RFC5245 : ICEPROTO_GOOGLE;
  } else {
    *remote_protocol_type = ice_protocol_;
  }
  if (*remote_protocol_type == ICEPROTO_RFC5245) {
    if (colon_pos != std::string::npos) {  // RFRAG:LFRAG
      *local_ufrag = username_attr_str.substr(0, colon_pos);
      *remote_ufrag = username_attr_str.substr(
          colon_pos + 1, username_attr_str.size());
    } else {
      return false;
    }
  } else if (*remote_protocol_type == ICEPROTO_GOOGLE) {
    int remote_frag_len = static_cast<int>(username_attr_str.size());
    remote_frag_len -= static_cast<int>(username_fragment().size());
    if (remote_frag_len < 0)
      return false;

    *local_ufrag = username_attr_str.substr(0, username_fragment().size());
    *remote_ufrag = username_attr_str.substr(
        username_fragment().size(), username_attr_str.size());
  }
  return true;
}

bool Port::MaybeIceRoleConflict(
    const rtc::SocketAddress& addr, IceMessage* stun_msg,
    const std::string& remote_ufrag) {
  // Validate ICE_CONTROLLING or ICE_CONTROLLED attributes.
  bool ret = true;
  IceRole remote_ice_role = ICEROLE_UNKNOWN;
  uint64 remote_tiebreaker = 0;
  const StunUInt64Attribute* stun_attr =
      stun_msg->GetUInt64(STUN_ATTR_ICE_CONTROLLING);
  if (stun_attr) {
    remote_ice_role = ICEROLE_CONTROLLING;
    remote_tiebreaker = stun_attr->value();
  }

  // If |remote_ufrag| is same as port local username fragment and
  // tie breaker value received in the ping message matches port
  // tiebreaker value this must be a loopback call.
  // We will treat this as valid scenario.
  if (remote_ice_role == ICEROLE_CONTROLLING &&
      username_fragment() == remote_ufrag &&
      remote_tiebreaker == IceTiebreaker()) {
    return true;
  }

  stun_attr = stun_msg->GetUInt64(STUN_ATTR_ICE_CONTROLLED);
  if (stun_attr) {
    remote_ice_role = ICEROLE_CONTROLLED;
    remote_tiebreaker = stun_attr->value();
  }

  switch (ice_role_) {
    case ICEROLE_CONTROLLING:
      if (ICEROLE_CONTROLLING == remote_ice_role) {
        if (remote_tiebreaker >= tiebreaker_) {
          SignalRoleConflict(this);
        } else {
          // Send Role Conflict (487) error response.
          SendBindingErrorResponse(stun_msg, addr,
              STUN_ERROR_ROLE_CONFLICT, STUN_ERROR_REASON_ROLE_CONFLICT);
          ret = false;
        }
      }
      break;
    case ICEROLE_CONTROLLED:
      if (ICEROLE_CONTROLLED == remote_ice_role) {
        if (remote_tiebreaker < tiebreaker_) {
          SignalRoleConflict(this);
        } else {
          // Send Role Conflict (487) error response.
          SendBindingErrorResponse(stun_msg, addr,
              STUN_ERROR_ROLE_CONFLICT, STUN_ERROR_REASON_ROLE_CONFLICT);
          ret = false;
        }
      }
      break;
    default:
      ASSERT(false);
  }
  return ret;
}

void Port::CreateStunUsername(const std::string& remote_username,
                              std::string* stun_username_attr_str) const {
  stun_username_attr_str->clear();
  *stun_username_attr_str = remote_username;
  if (IsStandardIce()) {
    // Connectivity checks from L->R will have username RFRAG:LFRAG.
    stun_username_attr_str->append(":");
  }
  stun_username_attr_str->append(username_fragment());
}

void Port::SendBindingResponse(StunMessage* request,
                               const rtc::SocketAddress& addr) {
  ASSERT(request->type() == STUN_BINDING_REQUEST);

  // Retrieve the username from the request.
  const StunByteStringAttribute* username_attr =
      request->GetByteString(STUN_ATTR_USERNAME);
  ASSERT(username_attr != NULL);
  if (username_attr == NULL) {
    // No valid username, skip the response.
    return;
  }

  // Fill in the response message.
  StunMessage response;
  response.SetType(STUN_BINDING_RESPONSE);
  response.SetTransactionID(request->transaction_id());
  const StunUInt32Attribute* retransmit_attr =
      request->GetUInt32(STUN_ATTR_RETRANSMIT_COUNT);
  if (retransmit_attr) {
    // Inherit the incoming retransmit value in the response so the other side
    // can see our view of lost pings.
    response.AddAttribute(new StunUInt32Attribute(
        STUN_ATTR_RETRANSMIT_COUNT, retransmit_attr->value()));

    if (retransmit_attr->value() > CONNECTION_WRITE_CONNECT_FAILURES) {
      LOG_J(LS_INFO, this)
          << "Received a remote ping with high retransmit count: "
          << retransmit_attr->value();
    }
  }

  // Only GICE messages have USERNAME and MAPPED-ADDRESS in the response.
  // ICE messages use XOR-MAPPED-ADDRESS, and add MESSAGE-INTEGRITY.
  if (IsStandardIce()) {
    response.AddAttribute(
        new StunXorAddressAttribute(STUN_ATTR_XOR_MAPPED_ADDRESS, addr));
    response.AddMessageIntegrity(password_);
    response.AddFingerprint();
  } else if (IsGoogleIce()) {
    response.AddAttribute(
        new StunAddressAttribute(STUN_ATTR_MAPPED_ADDRESS, addr));
    response.AddAttribute(new StunByteStringAttribute(
        STUN_ATTR_USERNAME, username_attr->GetString()));
  }

  // Send the response message.
  rtc::ByteBuffer buf;
  response.Write(&buf);
  rtc::PacketOptions options(DefaultDscpValue());
  if (SendTo(buf.Data(), buf.Length(), addr, options, false) < 0) {
    LOG_J(LS_ERROR, this) << "Failed to send STUN ping response to "
                          << addr.ToSensitiveString();
  }

  // The fact that we received a successful request means that this connection
  // (if one exists) should now be readable.
  Connection* conn = GetConnection(addr);
  ASSERT(conn != NULL);
  if (conn)
    conn->ReceivedPing();
}

void Port::SendBindingErrorResponse(StunMessage* request,
                                    const rtc::SocketAddress& addr,
                                    int error_code, const std::string& reason) {
  ASSERT(request->type() == STUN_BINDING_REQUEST);

  // Fill in the response message.
  StunMessage response;
  response.SetType(STUN_BINDING_ERROR_RESPONSE);
  response.SetTransactionID(request->transaction_id());

  // When doing GICE, we need to write out the error code incorrectly to
  // maintain backwards compatiblility.
  StunErrorCodeAttribute* error_attr = StunAttribute::CreateErrorCode();
  if (IsStandardIce()) {
    error_attr->SetCode(error_code);
  } else if (IsGoogleIce()) {
    error_attr->SetClass(error_code / 256);
    error_attr->SetNumber(error_code % 256);
  }
  error_attr->SetReason(reason);
  response.AddAttribute(error_attr);

  if (IsStandardIce()) {
    // Per Section 10.1.2, certain error cases don't get a MESSAGE-INTEGRITY,
    // because we don't have enough information to determine the shared secret.
    if (error_code != STUN_ERROR_BAD_REQUEST &&
        error_code != STUN_ERROR_UNAUTHORIZED)
      response.AddMessageIntegrity(password_);
    response.AddFingerprint();
  } else if (IsGoogleIce()) {
    // GICE responses include a username, if one exists.
    const StunByteStringAttribute* username_attr =
        request->GetByteString(STUN_ATTR_USERNAME);
    if (username_attr)
      response.AddAttribute(new StunByteStringAttribute(
          STUN_ATTR_USERNAME, username_attr->GetString()));
  }

  // Send the response message.
  rtc::ByteBuffer buf;
  response.Write(&buf);
  rtc::PacketOptions options(DefaultDscpValue());
  SendTo(buf.Data(), buf.Length(), addr, options, false);
  LOG_J(LS_INFO, this) << "Sending STUN binding error: reason=" << reason
                       << " to " << addr.ToSensitiveString();
}

void Port::OnMessage(rtc::Message *pmsg) {
  ASSERT(pmsg->message_id == MSG_CHECKTIMEOUT);
  CheckTimeout();
}

std::string Port::ToString() const {
  std::stringstream ss;
  ss << "Port[" << content_name_ << ":" << component_
     << ":" << generation_ << ":" << type_
     << ":" << network_->ToString() << "]";
  return ss.str();
}

void Port::EnablePortPackets() {
  enable_port_packets_ = true;
}

void Port::OnConnectionDestroyed(Connection* conn) {
  AddressMap::iterator iter =
      connections_.find(conn->remote_candidate().address());
  ASSERT(iter != connections_.end());
  connections_.erase(iter);

  // On the controlled side, ports time out, but only after all connections
  // fail.  Note: If a new connection is added after this message is posted,
  // but it fails and is removed before kPortTimeoutDelay, then this message
  //  will still cause the Port to be destroyed.
  if (ice_role_ == ICEROLE_CONTROLLED)
    thread_->PostDelayed(timeout_delay_, this, MSG_CHECKTIMEOUT);
}

void Port::Destroy() {
  ASSERT(connections_.empty());
  LOG_J(LS_INFO, this) << "Port deleted";
  SignalDestroyed(this);
  delete this;
}

void Port::CheckTimeout() {
  ASSERT(ice_role_ == ICEROLE_CONTROLLED);
  // If this port has no connections, then there's no reason to keep it around.
  // When the connections time out (both read and write), they will delete
  // themselves, so if we have any connections, they are either readable or
  // writable (or still connecting).
  if (connections_.empty())
    Destroy();
}

const std::string Port::username_fragment() const {
  if (!IsStandardIce() &&
      component_ == ICE_CANDIDATE_COMPONENT_RTCP) {
    // In GICE mode, we should adjust username fragment for rtcp component.
    return GetRtcpUfragFromRtpUfrag(ice_username_fragment_);
  } else {
    return ice_username_fragment_;
  }
}

// A ConnectionRequest is a simple STUN ping used to determine writability.
class ConnectionRequest : public StunRequest {
 public:
  explicit ConnectionRequest(Connection* connection)
      : StunRequest(new IceMessage()),
        connection_(connection) {
  }

  virtual ~ConnectionRequest() {
  }

  virtual void Prepare(StunMessage* request) {
    request->SetType(STUN_BINDING_REQUEST);
    std::string username;
    connection_->port()->CreateStunUsername(
        connection_->remote_candidate().username(), &username);
    request->AddAttribute(
        new StunByteStringAttribute(STUN_ATTR_USERNAME, username));

    // connection_ already holds this ping, so subtract one from count.
    if (connection_->port()->send_retransmit_count_attribute()) {
      request->AddAttribute(new StunUInt32Attribute(
          STUN_ATTR_RETRANSMIT_COUNT,
          static_cast<uint32>(
              connection_->pings_since_last_response_.size() - 1)));
    }

    // Adding ICE-specific attributes to the STUN request message.
    if (connection_->port()->IsStandardIce()) {
      // Adding ICE_CONTROLLED or ICE_CONTROLLING attribute based on the role.
      if (connection_->port()->GetIceRole() == ICEROLE_CONTROLLING) {
        request->AddAttribute(new StunUInt64Attribute(
            STUN_ATTR_ICE_CONTROLLING, connection_->port()->IceTiebreaker()));
        // Since we are trying aggressive nomination, sending USE-CANDIDATE
        // attribute in every ping.
        // If we are dealing with a ice-lite end point, nomination flag
        // in Connection will be set to false by default. Once the connection
        // becomes "best connection", nomination flag will be turned on.
        if (connection_->use_candidate_attr()) {
          request->AddAttribute(new StunByteStringAttribute(
              STUN_ATTR_USE_CANDIDATE));
        }
      } else if (connection_->port()->GetIceRole() == ICEROLE_CONTROLLED) {
        request->AddAttribute(new StunUInt64Attribute(
            STUN_ATTR_ICE_CONTROLLED, connection_->port()->IceTiebreaker()));
      } else {
        ASSERT(false);
      }

      // Adding PRIORITY Attribute.
      // Changing the type preference to Peer Reflexive and local preference
      // and component id information is unchanged from the original priority.
      // priority = (2^24)*(type preference) +
      //           (2^8)*(local preference) +
      //           (2^0)*(256 - component ID)
      uint32 prflx_priority = ICE_TYPE_PREFERENCE_PRFLX << 24 |
          (connection_->local_candidate().priority() & 0x00FFFFFF);
      request->AddAttribute(
          new StunUInt32Attribute(STUN_ATTR_PRIORITY, prflx_priority));

      // Adding Message Integrity attribute.
      request->AddMessageIntegrity(connection_->remote_candidate().password());
      // Adding Fingerprint.
      request->AddFingerprint();
    }
  }

  virtual void OnResponse(StunMessage* response) {
    connection_->OnConnectionRequestResponse(this, response);
  }

  virtual void OnErrorResponse(StunMessage* response) {
    connection_->OnConnectionRequestErrorResponse(this, response);
  }

  virtual void OnTimeout() {
    connection_->OnConnectionRequestTimeout(this);
  }

  virtual int GetNextDelay() {
    // Each request is sent only once.  After a single delay , the request will
    // time out.
    timeout_ = true;
    return CONNECTION_RESPONSE_TIMEOUT;
  }

 private:
  Connection* connection_;
};

//
// Connection
//

Connection::Connection(Port* port,
                       size_t index,
                       const Candidate& remote_candidate)
    : port_(port),
      local_candidate_index_(index),
      remote_candidate_(remote_candidate),
      read_state_(STATE_READ_INIT),
      write_state_(STATE_WRITE_INIT),
      connected_(true),
      pruned_(false),
      use_candidate_attr_(false),
      remote_ice_mode_(ICEMODE_FULL),
      requests_(port->thread()),
      rtt_(DEFAULT_RTT),
      last_ping_sent_(0),
      last_ping_received_(0),
      last_data_received_(0),
      last_ping_response_received_(0),
      sent_packets_discarded_(0),
      sent_packets_total_(0),
      reported_(false),
      state_(STATE_WAITING) {
  // All of our connections start in WAITING state.
  // TODO(mallinath) - Start connections from STATE_FROZEN.
  // Wire up to send stun packets
  requests_.SignalSendPacket.connect(this, &Connection::OnSendStunPacket);
  LOG_J(LS_INFO, this) << "Connection created";
}

Connection::~Connection() {
}

const Candidate& Connection::local_candidate() const {
  ASSERT(local_candidate_index_ < port_->Candidates().size());
  return port_->Candidates()[local_candidate_index_];
}

uint64 Connection::priority() const {
  uint64 priority = 0;
  // RFC 5245 - 5.7.2.  Computing Pair Priority and Ordering Pairs
  // Let G be the priority for the candidate provided by the controlling
  // agent.  Let D be the priority for the candidate provided by the
  // controlled agent.
  // pair priority = 2^32*MIN(G,D) + 2*MAX(G,D) + (G>D?1:0)
  IceRole role = port_->GetIceRole();
  if (role != ICEROLE_UNKNOWN) {
    uint32 g = 0;
    uint32 d = 0;
    if (role == ICEROLE_CONTROLLING) {
      g = local_candidate().priority();
      d = remote_candidate_.priority();
    } else {
      g = remote_candidate_.priority();
      d = local_candidate().priority();
    }
    priority = std::min(g, d);
    priority = priority << 32;
    priority += 2 * std::max(g, d) + (g > d ? 1 : 0);
  }
  return priority;
}

void Connection::set_read_state(ReadState value) {
  ReadState old_value = read_state_;
  read_state_ = value;
  if (value != old_value) {
    LOG_J(LS_VERBOSE, this) << "set_read_state";
    SignalStateChange(this);
    CheckTimeout();
  }
}

void Connection::set_write_state(WriteState value) {
  WriteState old_value = write_state_;
  write_state_ = value;
  if (value != old_value) {
    LOG_J(LS_VERBOSE, this) << "set_write_state from: " << old_value << " to "
                            << value;
    SignalStateChange(this);
    CheckTimeout();
  }
}

void Connection::set_state(State state) {
  State old_state = state_;
  state_ = state;
  if (state != old_state) {
    LOG_J(LS_VERBOSE, this) << "set_state";
  }
}

void Connection::set_connected(bool value) {
  bool old_value = connected_;
  connected_ = value;
  if (value != old_value) {
    LOG_J(LS_VERBOSE, this) << "set_connected";
  }
}

void Connection::set_use_candidate_attr(bool enable) {
  use_candidate_attr_ = enable;
}

void Connection::OnSendStunPacket(const void* data, size_t size,
                                  StunRequest* req) {
  rtc::PacketOptions options(port_->DefaultDscpValue());
  if (port_->SendTo(data, size, remote_candidate_.address(),
                    options, false) < 0) {
    LOG_J(LS_WARNING, this) << "Failed to send STUN ping " << req->id();
  }
}

void Connection::OnReadPacket(
  const char* data, size_t size, const rtc::PacketTime& packet_time) {
  rtc::scoped_ptr<IceMessage> msg;
  std::string remote_ufrag;
  const rtc::SocketAddress& addr(remote_candidate_.address());
  if (!port_->GetStunMessage(data, size, addr, msg.accept(), &remote_ufrag)) {
    // The packet did not parse as a valid STUN message

    // If this connection is readable, then pass along the packet.
    if (read_state_ == STATE_READABLE) {
      // readable means data from this address is acceptable
      // Send it on!

      last_data_received_ = rtc::Time();
      recv_rate_tracker_.Update(size);
      SignalReadPacket(this, data, size, packet_time);

      // If timed out sending writability checks, start up again
      if (!pruned_ && (write_state_ == STATE_WRITE_TIMEOUT)) {
        LOG(LS_WARNING) << "Received a data packet on a timed-out Connection. "
                        << "Resetting state to STATE_WRITE_INIT.";
        set_write_state(STATE_WRITE_INIT);
      }
    } else {
      // Not readable means the remote address hasn't sent a valid
      // binding request yet.

      LOG_J(LS_WARNING, this)
        << "Received non-STUN packet from an unreadable connection.";
    }
  } else if (!msg) {
    // The packet was STUN, but failed a check and was handled internally.
  } else {
    // The packet is STUN and passed the Port checks.
    // Perform our own checks to ensure this packet is valid.
    // If this is a STUN request, then update the readable bit and respond.
    // If this is a STUN response, then update the writable bit.
    switch (msg->type()) {
      case STUN_BINDING_REQUEST:
        if (remote_ufrag == remote_candidate_.username()) {
          // Check for role conflicts.
          if (port_->IsStandardIce() &&
              !port_->MaybeIceRoleConflict(addr, msg.get(), remote_ufrag)) {
            // Received conflicting role from the peer.
            LOG(LS_INFO) << "Received conflicting role from the peer.";
            return;
          }

          // Incoming, validated stun request from remote peer.
          // This call will also set the connection readable.
          port_->SendBindingResponse(msg.get(), addr);

          // If timed out sending writability checks, start up again
          if (!pruned_ && (write_state_ == STATE_WRITE_TIMEOUT))
            set_write_state(STATE_WRITE_INIT);

          if ((port_->IsStandardIce()) &&
              (port_->GetIceRole() == ICEROLE_CONTROLLED)) {
            const StunByteStringAttribute* use_candidate_attr =
                msg->GetByteString(STUN_ATTR_USE_CANDIDATE);
            if (use_candidate_attr)
              SignalUseCandidate(this);
          }
        } else {
          // The packet had the right local username, but the remote username
          // was not the right one for the remote address.
          LOG_J(LS_ERROR, this)
            << "Received STUN request with bad remote username "
            << remote_ufrag;
          port_->SendBindingErrorResponse(msg.get(), addr,
                                          STUN_ERROR_UNAUTHORIZED,
                                          STUN_ERROR_REASON_UNAUTHORIZED);

        }
        break;

      // Response from remote peer. Does it match request sent?
      // This doesn't just check, it makes callbacks if transaction
      // id's match.
      case STUN_BINDING_RESPONSE:
      case STUN_BINDING_ERROR_RESPONSE:
        if (port_->IsGoogleIce() ||
            msg->ValidateMessageIntegrity(
                data, size, remote_candidate().password())) {
          requests_.CheckResponse(msg.get());
        }
        // Otherwise silently discard the response message.
        break;

      // Remote end point sent an STUN indication instead of regular
      // binding request. In this case |last_ping_received_| will be updated.
      // Otherwise we can mark connection to read timeout. No response will be
      // sent in this scenario.
      case STUN_BINDING_INDICATION:
        if (port_->IsStandardIce() && read_state_ == STATE_READABLE) {
          ReceivedPing();
        } else {
          LOG_J(LS_WARNING, this) << "Received STUN binding indication "
                                  << "from an unreadable connection.";
        }
        break;

      default:
        ASSERT(false);
        break;
    }
  }
}

void Connection::OnReadyToSend() {
  if (write_state_ == STATE_WRITABLE) {
    SignalReadyToSend(this);
  }
}

void Connection::Prune() {
  if (!pruned_) {
    LOG_J(LS_VERBOSE, this) << "Connection pruned";
    pruned_ = true;
    requests_.Clear();
    set_write_state(STATE_WRITE_TIMEOUT);
  }
}

void Connection::Destroy() {
  LOG_J(LS_VERBOSE, this) << "Connection destroyed";
  set_read_state(STATE_READ_TIMEOUT);
  set_write_state(STATE_WRITE_TIMEOUT);
}

void Connection::UpdateState(uint32 now) {
  uint32 rtt = ConservativeRTTEstimate(rtt_);

  std::string pings;
  for (size_t i = 0; i < pings_since_last_response_.size(); ++i) {
    char buf[32];
    rtc::sprintfn(buf, sizeof(buf), "%u",
        pings_since_last_response_[i]);
    pings.append(buf).append(" ");
  }
  LOG_J(LS_VERBOSE, this) << "UpdateState(): pings_since_last_response_="
                          << pings << ", rtt=" << rtt << ", now=" << now
                          << ", last ping received: " << last_ping_received_
                          << ", last data_received: " << last_data_received_;

  // Check the readable state.
  //
  // Since we don't know how many pings the other side has attempted, the best
  // test we can do is a simple window.
  // If other side has not sent ping after connection has become readable, use
  // |last_data_received_| as the indication.
  // If remote endpoint is doing RFC 5245, it's not required to send ping
  // after connection is established. If this connection is serving a data
  // channel, it may not be in a position to send media continuously. Do not
  // mark connection timeout if it's in RFC5245 mode.
  // Below check will be performed with end point if it's doing google-ice.
  if (port_->IsGoogleIce() && (read_state_ == STATE_READABLE) &&
      (last_ping_received_ + CONNECTION_READ_TIMEOUT <= now) &&
      (last_data_received_ + CONNECTION_READ_TIMEOUT <= now)) {
    LOG_J(LS_INFO, this) << "Unreadable after "
                         << now - last_ping_received_
                         << " ms without a ping,"
                         << " ms since last received response="
                         << now - last_ping_response_received_
                         << " ms since last received data="
                         << now - last_data_received_
                         << " rtt=" << rtt;
    set_read_state(STATE_READ_TIMEOUT);
  }

  // Check the writable state.  (The order of these checks is important.)
  //
  // Before becoming unwritable, we allow for a fixed number of pings to fail
  // (i.e., receive no response).  We also have to give the response time to
  // get back, so we include a conservative estimate of this.
  //
  // Before timing out writability, we give a fixed amount of time.  This is to
  // allow for changes in network conditions.

  if ((write_state_ == STATE_WRITABLE) &&
      TooManyFailures(pings_since_last_response_,
                      CONNECTION_WRITE_CONNECT_FAILURES,
                      rtt,
                      now) &&
      TooLongWithoutResponse(pings_since_last_response_,
                             CONNECTION_WRITE_CONNECT_TIMEOUT,
                             now)) {
    uint32 max_pings = CONNECTION_WRITE_CONNECT_FAILURES;
    LOG_J(LS_INFO, this) << "Unwritable after " << max_pings
                         << " ping failures and "
                         << now - pings_since_last_response_[0]
                         << " ms without a response,"
                         << " ms since last received ping="
                         << now - last_ping_received_
                         << " ms since last received data="
                         << now - last_data_received_
                         << " rtt=" << rtt;
    set_write_state(STATE_WRITE_UNRELIABLE);
  }

  if ((write_state_ == STATE_WRITE_UNRELIABLE ||
       write_state_ == STATE_WRITE_INIT) &&
      TooLongWithoutResponse(pings_since_last_response_,
                             CONNECTION_WRITE_TIMEOUT,
                             now)) {
    LOG_J(LS_INFO, this) << "Timed out after "
                         << now - pings_since_last_response_[0]
                         << " ms without a response, rtt=" << rtt;
    set_write_state(STATE_WRITE_TIMEOUT);
  }
}

void Connection::Ping(uint32 now) {
  ASSERT(connected_);
  last_ping_sent_ = now;
  pings_since_last_response_.push_back(now);
  ConnectionRequest *req = new ConnectionRequest(this);
  LOG_J(LS_VERBOSE, this) << "Sending STUN ping " << req->id() << " at " << now;
  requests_.Send(req);
  state_ = STATE_INPROGRESS;
}

void Connection::ReceivedPing() {
  last_ping_received_ = rtc::Time();
  set_read_state(STATE_READABLE);
}

std::string Connection::ToDebugId() const {
  std::stringstream ss;
  ss << std::hex << this;
  return ss.str();
}

std::string Connection::ToString() const {
  const char CONNECT_STATE_ABBREV[2] = {
    '-',  // not connected (false)
    'C',  // connected (true)
  };
  const char READ_STATE_ABBREV[3] = {
    '-',  // STATE_READ_INIT
    'R',  // STATE_READABLE
    'x',  // STATE_READ_TIMEOUT
  };
  const char WRITE_STATE_ABBREV[4] = {
    'W',  // STATE_WRITABLE
    'w',  // STATE_WRITE_UNRELIABLE
    '-',  // STATE_WRITE_INIT
    'x',  // STATE_WRITE_TIMEOUT
  };
  const std::string ICESTATE[4] = {
    "W",  // STATE_WAITING
    "I",  // STATE_INPROGRESS
    "S",  // STATE_SUCCEEDED
    "F"   // STATE_FAILED
  };
  const Candidate& local = local_candidate();
  const Candidate& remote = remote_candidate();
  std::stringstream ss;
  ss << "Conn[" << ToDebugId()
     << ":" << port_->content_name()
     << ":" << local.id() << ":" << local.component()
     << ":" << local.generation()
     << ":" << local.type() << ":" << local.protocol()
     << ":" << local.address().ToSensitiveString()
     << "->" << remote.id() << ":" << remote.component()
     << ":" << remote.priority()
     << ":" << remote.type() << ":"
     << remote.protocol() << ":" << remote.address().ToSensitiveString() << "|"
     << CONNECT_STATE_ABBREV[connected()]
     << READ_STATE_ABBREV[read_state()]
     << WRITE_STATE_ABBREV[write_state()]
     << ICESTATE[state()] << "|"
     << priority() << "|";
  if (rtt_ < DEFAULT_RTT) {
    ss << rtt_ << "]";
  } else {
    ss << "-]";
  }
  return ss.str();
}

std::string Connection::ToSensitiveString() const {
  return ToString();
}

void Connection::OnConnectionRequestResponse(ConnectionRequest* request,
                                             StunMessage* response) {
  // We've already validated that this is a STUN binding response with
  // the correct local and remote username for this connection.
  // So if we're not already, become writable. We may be bringing a pruned
  // connection back to life, but if we don't really want it, we can always
  // prune it again.
  uint32 rtt = request->Elapsed();
  set_write_state(STATE_WRITABLE);
  set_state(STATE_SUCCEEDED);

  if (remote_ice_mode_ == ICEMODE_LITE) {
    // A ice-lite end point never initiates ping requests. This will allow
    // us to move to STATE_READABLE.
    ReceivedPing();
  }

  std::string pings;
  for (size_t i = 0; i < pings_since_last_response_.size(); ++i) {
    char buf[32];
    rtc::sprintfn(buf, sizeof(buf), "%u",
        pings_since_last_response_[i]);
    pings.append(buf).append(" ");
  }

  rtc::LoggingSeverity level =
      (pings_since_last_response_.size() > CONNECTION_WRITE_CONNECT_FAILURES) ?
          rtc::LS_INFO : rtc::LS_VERBOSE;

  LOG_JV(level, this) << "Received STUN ping response " << request->id()
                      << ", pings_since_last_response_=" << pings
                      << ", rtt=" << rtt;

  pings_since_last_response_.clear();
  last_ping_response_received_ = rtc::Time();
  rtt_ = (RTT_RATIO * rtt_ + rtt) / (RTT_RATIO + 1);

  // Peer reflexive candidate is only for RFC 5245 ICE.
  if (port_->IsStandardIce()) {
    MaybeAddPrflxCandidate(request, response);
  }
}

void Connection::OnConnectionRequestErrorResponse(ConnectionRequest* request,
                                                  StunMessage* response) {
  const StunErrorCodeAttribute* error_attr = response->GetErrorCode();
  int error_code = STUN_ERROR_GLOBAL_FAILURE;
  if (error_attr) {
    if (port_->IsGoogleIce()) {
      // When doing GICE, the error code is written out incorrectly, so we need
      // to unmunge it here.
      error_code = error_attr->eclass() * 256 + error_attr->number();
    } else {
      error_code = error_attr->code();
    }
  }

  if (error_code == STUN_ERROR_UNKNOWN_ATTRIBUTE ||
      error_code == STUN_ERROR_SERVER_ERROR ||
      error_code == STUN_ERROR_UNAUTHORIZED) {
    // Recoverable error, retry
  } else if (error_code == STUN_ERROR_STALE_CREDENTIALS) {
    // Race failure, retry
  } else if (error_code == STUN_ERROR_ROLE_CONFLICT) {
    HandleRoleConflictFromPeer();
  } else {
    // This is not a valid connection.
    LOG_J(LS_ERROR, this) << "Received STUN error response, code="
                          << error_code << "; killing connection";
    set_state(STATE_FAILED);
    set_write_state(STATE_WRITE_TIMEOUT);
  }
}

void Connection::OnConnectionRequestTimeout(ConnectionRequest* request) {
  // Log at LS_INFO if we miss a ping on a writable connection.
  rtc::LoggingSeverity sev = (write_state_ == STATE_WRITABLE) ?
      rtc::LS_INFO : rtc::LS_VERBOSE;
  LOG_JV(sev, this) << "Timing-out STUN ping " << request->id()
                    << " after " << request->Elapsed() << " ms";
}

void Connection::CheckTimeout() {
  // If both read and write have timed out or read has never initialized, then
  // this connection can contribute no more to p2p socket unless at some later
  // date readability were to come back.  However, we gave readability a long
  // time to timeout, so at this point, it seems fair to get rid of this
  // connection.
  if ((read_state_ == STATE_READ_TIMEOUT ||
       read_state_ == STATE_READ_INIT) &&
      write_state_ == STATE_WRITE_TIMEOUT) {
    port_->thread()->Post(this, MSG_DELETE);
  }
}

void Connection::HandleRoleConflictFromPeer() {
  port_->SignalRoleConflict(port_);
}

void Connection::MaybeSetRemoteIceCredentials(const std::string& ice_ufrag,
                                              const std::string& ice_pwd) {
  if (remote_candidate_.username() == ice_ufrag &&
      remote_candidate_.password().empty()) {
    remote_candidate_.set_password(ice_pwd);
  }
}

void Connection::MaybeUpdatePeerReflexiveCandidate(
    const Candidate& new_candidate) {
  if (remote_candidate_.type() == PRFLX_PORT_TYPE &&
      new_candidate.type() != PRFLX_PORT_TYPE &&
      remote_candidate_.protocol() == new_candidate.protocol() &&
      remote_candidate_.address() == new_candidate.address() &&
      remote_candidate_.username() == new_candidate.username() &&
      remote_candidate_.password() == new_candidate.password() &&
      remote_candidate_.generation() == new_candidate.generation()) {
    remote_candidate_ = new_candidate;
  }
}

void Connection::OnMessage(rtc::Message *pmsg) {
  ASSERT(pmsg->message_id == MSG_DELETE);

  LOG_J(LS_INFO, this) << "Connection deleted due to read or write timeout";
  SignalDestroyed(this);
  delete this;
}

size_t Connection::recv_bytes_second() {
  return recv_rate_tracker_.units_second();
}

size_t Connection::recv_total_bytes() {
  return recv_rate_tracker_.total_units();
}

size_t Connection::sent_bytes_second() {
  return send_rate_tracker_.units_second();
}

size_t Connection::sent_total_bytes() {
  return send_rate_tracker_.total_units();
}

size_t Connection::sent_discarded_packets() {
  return sent_packets_discarded_;
}

size_t Connection::sent_total_packets() {
  return sent_packets_total_;
}

void Connection::MaybeAddPrflxCandidate(ConnectionRequest* request,
                                        StunMessage* response) {
  // RFC 5245
  // The agent checks the mapped address from the STUN response.  If the
  // transport address does not match any of the local candidates that the
  // agent knows about, the mapped address represents a new candidate -- a
  // peer reflexive candidate.
  const StunAddressAttribute* addr =
      response->GetAddress(STUN_ATTR_XOR_MAPPED_ADDRESS);
  if (!addr) {
    LOG(LS_WARNING) << "Connection::OnConnectionRequestResponse - "
                    << "No MAPPED-ADDRESS or XOR-MAPPED-ADDRESS found in the "
                    << "stun response message";
    return;
  }

  bool known_addr = false;
  for (size_t i = 0; i < port_->Candidates().size(); ++i) {
    if (port_->Candidates()[i].address() == addr->GetAddress()) {
      known_addr = true;
      break;
    }
  }
  if (known_addr) {
    return;
  }

  // RFC 5245
  // Its priority is set equal to the value of the PRIORITY attribute
  // in the Binding request.
  const StunUInt32Attribute* priority_attr =
      request->msg()->GetUInt32(STUN_ATTR_PRIORITY);
  if (!priority_attr) {
    LOG(LS_WARNING) << "Connection::OnConnectionRequestResponse - "
                    << "No STUN_ATTR_PRIORITY found in the "
                    << "stun response message";
    return;
  }
  const uint32 priority = priority_attr->value();
  std::string id = rtc::CreateRandomString(8);

  Candidate new_local_candidate;
  new_local_candidate.set_id(id);
  new_local_candidate.set_component(local_candidate().component());
  new_local_candidate.set_type(PRFLX_PORT_TYPE);
  new_local_candidate.set_protocol(local_candidate().protocol());
  new_local_candidate.set_address(addr->GetAddress());
  new_local_candidate.set_priority(priority);
  new_local_candidate.set_username(local_candidate().username());
  new_local_candidate.set_password(local_candidate().password());
  new_local_candidate.set_network_name(local_candidate().network_name());
  new_local_candidate.set_network_type(local_candidate().network_type());
  new_local_candidate.set_related_address(local_candidate().address());
  new_local_candidate.set_foundation(
      ComputeFoundation(PRFLX_PORT_TYPE, local_candidate().protocol(),
                        local_candidate().address()));

  // Change the local candidate of this Connection to the new prflx candidate.
  local_candidate_index_ = port_->AddPrflxCandidate(new_local_candidate);

  // SignalStateChange to force a re-sort in P2PTransportChannel as this
  // Connection's local candidate has changed.
  SignalStateChange(this);
}

ProxyConnection::ProxyConnection(Port* port, size_t index,
                                 const Candidate& candidate)
  : Connection(port, index, candidate), error_(0) {
}

int ProxyConnection::Send(const void* data, size_t size,
                          const rtc::PacketOptions& options) {
  if (write_state_ == STATE_WRITE_INIT || write_state_ == STATE_WRITE_TIMEOUT) {
    error_ = EWOULDBLOCK;
    return SOCKET_ERROR;
  }
  sent_packets_total_++;
  int sent = port_->SendTo(data, size, remote_candidate_.address(),
                           options, true);
  if (sent <= 0) {
    ASSERT(sent < 0);
    error_ = port_->GetError();
    sent_packets_discarded_++;
  } else {
    send_rate_tracker_.Update(sent);
  }
  return sent;
}

}  // namespace cricket

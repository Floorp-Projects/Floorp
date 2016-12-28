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
    const std::vector<cricket::Connection::SentPing>& pings_since_last_response,
    uint32_t maximum_failures,
    uint32_t rtt_estimate,
    uint32_t now) {
  // If we haven't sent that many pings, then we can't have failed that many.
  if (pings_since_last_response.size() < maximum_failures)
    return false;

  // Check if the window in which we would expect a response to the ping has
  // already elapsed.
  uint32_t expected_response_time =
      pings_since_last_response[maximum_failures - 1].sent_time + rtt_estimate;
  return now > expected_response_time;
}

// Determines whether we have gone too long without seeing any response.
inline bool TooLongWithoutResponse(
    const std::vector<cricket::Connection::SentPing>& pings_since_last_response,
    uint32_t maximum_time,
    uint32_t now) {
  if (pings_since_last_response.size() == 0)
    return false;

  auto first = pings_since_last_response[0];
  return now > (first.sent_time + maximum_time);
}

// We will restrict RTT estimates (when used for determining state) to be
// within a reasonable range.
const uint32_t MINIMUM_RTT = 100;   // 0.1 seconds
const uint32_t MAXIMUM_RTT = 3000;  // 3 seconds

// When we don't have any RTT data, we have to pick something reasonable.  We
// use a large value just in case the connection is really slow.
const uint32_t DEFAULT_RTT = MAXIMUM_RTT;

// Computes our estimate of the RTT given the current estimate.
inline uint32_t ConservativeRTTEstimate(uint32_t rtt) {
  return std::max(MINIMUM_RTT, std::min(MAXIMUM_RTT, 2 * rtt));
}

// Weighting of the old rtt value to new data.
const int RTT_RATIO = 3;  // 3 : 1

// The delay before we begin checking if this port is useless.
const int kPortTimeoutDelay = 30 * 1000;  // 30 seconds
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
  return rtc::ToString<uint32_t>(rtc::ComputeCrc32(ost.str()));
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
           uint16_t min_port,
           uint16_t max_port,
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
      ice_role_(ICEROLE_UNKNOWN),
      tiebreaker_(0),
      shared_socket_(false),
      candidate_filter_(CF_ALL) {
  ASSERT(factory_ != NULL);
  Construct();
}

void Port::Construct() {
  // TODO(pthatcher): Remove this old behavior once we're sure no one
  // relies on it.  If the username_fragment and password are empty,
  // we should just create one.
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

  for (uint32_t i = 0; i < list.size(); i++)
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
                      const std::string& relay_protocol,
                      const std::string& tcptype,
                      const std::string& type,
                      uint32_t type_preference,
                      uint32_t relay_preference,
                      bool final) {
  if (protocol == TCP_PROTOCOL_NAME && type == LOCAL_PORT_TYPE) {
    ASSERT(!tcptype.empty());
  }

  Candidate c;
  c.set_id(rtc::CreateRandomString(8));
  c.set_component(component_);
  c.set_type(type);
  c.set_protocol(protocol);
  c.set_relay_protocol(relay_protocol);
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
    LOG(LS_INFO) << "Received STUN ping "
                 << " id=" << rtc::hex_encode(msg->transaction_id())
                 << " from unknown address " << addr.ToSensitiveString();

    // Check for role conflicts.
    if (!MaybeIceRoleConflict(addr, msg.get(), remote_username)) {
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
  if (!StunMessage::ValidateFingerprint(data, size)) {
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
        !stun_msg->GetByteString(STUN_ATTR_MESSAGE_INTEGRITY)) {
      LOG_J(LS_ERROR, this) << "Received STUN request without username/M-I "
                            << "from " << addr.ToSensitiveString();
      SendBindingErrorResponse(stun_msg.get(), addr, STUN_ERROR_BAD_REQUEST,
                               STUN_ERROR_REASON_BAD_REQUEST);
      return true;
    }

    // If the username is bad or unknown, fail with a 401 Unauthorized.
    std::string local_ufrag;
    std::string remote_ufrag;
    if (!ParseStunUsername(stun_msg.get(), &local_ufrag, &remote_ufrag) ||
        local_ufrag != username_fragment()) {
      LOG_J(LS_ERROR, this) << "Received STUN request with bad local username "
                            << local_ufrag << " from "
                            << addr.ToSensitiveString();
      SendBindingErrorResponse(stun_msg.get(), addr, STUN_ERROR_UNAUTHORIZED,
                               STUN_ERROR_REASON_UNAUTHORIZED);
      return true;
    }

    // If ICE, and the MESSAGE-INTEGRITY is bad, fail with a 401 Unauthorized
    if (!stun_msg->ValidateMessageIntegrity(data, size, password_)) {
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
  if (family == AF_INET6 &&
      (IPIsLinkLocal(ip()) != IPIsLinkLocal(addr.ipaddr()))) {
    return false;
  }
  return true;
}

bool Port::ParseStunUsername(const StunMessage* stun_msg,
                             std::string* local_ufrag,
                             std::string* remote_ufrag) const {
  // The packet must include a username that either begins or ends with our
  // fragment.  It should begin with our fragment if it is a request and it
  // should end with our fragment if it is a response.
  local_ufrag->clear();
  remote_ufrag->clear();
  const StunByteStringAttribute* username_attr =
        stun_msg->GetByteString(STUN_ATTR_USERNAME);
  if (username_attr == NULL)
    return false;

  // RFRAG:LFRAG
  const std::string username = username_attr->GetString();
  size_t colon_pos = username.find(":");
  if (colon_pos == std::string::npos) {
    return false;
  }

  *local_ufrag = username.substr(0, colon_pos);
  *remote_ufrag = username.substr(colon_pos + 1, username.size());
  return true;
}

bool Port::MaybeIceRoleConflict(
    const rtc::SocketAddress& addr, IceMessage* stun_msg,
    const std::string& remote_ufrag) {
  // Validate ICE_CONTROLLING or ICE_CONTROLLED attributes.
  bool ret = true;
  IceRole remote_ice_role = ICEROLE_UNKNOWN;
  uint64_t remote_tiebreaker = 0;
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
  stun_username_attr_str->append(":");
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

  response.AddAttribute(
      new StunXorAddressAttribute(STUN_ATTR_XOR_MAPPED_ADDRESS, addr));
  response.AddMessageIntegrity(password_);
  response.AddFingerprint();

  // Send the response message.
  rtc::ByteBuffer buf;
  response.Write(&buf);
  rtc::PacketOptions options(DefaultDscpValue());
  auto err = SendTo(buf.Data(), buf.Length(), addr, options, false);
  if (err < 0) {
    LOG_J(LS_ERROR, this)
        << "Failed to send STUN ping response"
        << ", to=" << addr.ToSensitiveString()
        << ", err=" << err
        << ", id=" << rtc::hex_encode(response.transaction_id());
  } else {
    // Log at LS_INFO if we send a stun ping response on an unwritable
    // connection.
    Connection* conn = GetConnection(addr);
    rtc::LoggingSeverity sev = (conn && !conn->writable()) ?
        rtc::LS_INFO : rtc::LS_VERBOSE;
    LOG_JV(sev, this)
        << "Sent STUN ping response"
        << ", to=" << addr.ToSensitiveString()
        << ", id=" << rtc::hex_encode(response.transaction_id());
  }
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
  error_attr->SetCode(error_code);
  error_attr->SetReason(reason);
  response.AddAttribute(error_attr);

  // Per Section 10.1.2, certain error cases don't get a MESSAGE-INTEGRITY,
  // because we don't have enough information to determine the shared secret.
  if (error_code != STUN_ERROR_BAD_REQUEST &&
      error_code != STUN_ERROR_UNAUTHORIZED)
    response.AddMessageIntegrity(password_);
  response.AddFingerprint();

  // Send the response message.
  rtc::ByteBuffer buf;
  response.Write(&buf);
  rtc::PacketOptions options(DefaultDscpValue());
  SendTo(buf.Data(), buf.Length(), addr, options, false);
  LOG_J(LS_INFO, this) << "Sending STUN binding error: reason=" << reason
                       << " to " << addr.ToSensitiveString();
}

void Port::OnMessage(rtc::Message *pmsg) {
  ASSERT(pmsg->message_id == MSG_DEAD);
  if (dead()) {
    Destroy();
  }
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

  // On the controlled side, ports time out after all connections fail.
  // Note: If a new connection is added after this message is posted, but it
  // fails and is removed before kPortTimeoutDelay, then this message will
  // still cause the Port to be destroyed.
  if (dead()) {
    thread_->PostDelayed(timeout_delay_, this, MSG_DEAD);
  }
}

void Port::Destroy() {
  ASSERT(connections_.empty());
  LOG_J(LS_INFO, this) << "Port deleted";
  SignalDestroyed(this);
  delete this;
}

const std::string Port::username_fragment() const {
  return ice_username_fragment_;
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

  void Prepare(StunMessage* request) override {
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
          static_cast<uint32_t>(connection_->pings_since_last_response_.size() -
                                1)));
    }

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
    uint32_t prflx_priority =
        ICE_TYPE_PREFERENCE_PRFLX << 24 |
        (connection_->local_candidate().priority() & 0x00FFFFFF);
    request->AddAttribute(
        new StunUInt32Attribute(STUN_ATTR_PRIORITY, prflx_priority));

    // Adding Message Integrity attribute.
    request->AddMessageIntegrity(connection_->remote_candidate().password());
    // Adding Fingerprint.
    request->AddFingerprint();
  }

  void OnResponse(StunMessage* response) override {
    connection_->OnConnectionRequestResponse(this, response);
  }

  void OnErrorResponse(StunMessage* response) override {
    connection_->OnConnectionRequestErrorResponse(this, response);
  }

  void OnTimeout() override {
    connection_->OnConnectionRequestTimeout(this);
  }

  void OnSent() override {
    connection_->OnConnectionRequestSent(this);
    // Each request is sent only once.  After a single delay , the request will
    // time out.
    timeout_ = true;
  }

  int resend_delay() override {
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
      write_state_(STATE_WRITE_INIT),
      receiving_(false),
      connected_(true),
      pruned_(false),
      use_candidate_attr_(false),
      nominated_(false),
      remote_ice_mode_(ICEMODE_FULL),
      requests_(port->thread()),
      rtt_(DEFAULT_RTT),
      last_ping_sent_(0),
      last_ping_received_(0),
      last_data_received_(0),
      last_ping_response_received_(0),
      recv_rate_tracker_(100u, 10u),
      send_rate_tracker_(100u, 10u),
      sent_packets_discarded_(0),
      sent_packets_total_(0),
      reported_(false),
      state_(STATE_WAITING),
      receiving_timeout_(WEAK_CONNECTION_RECEIVE_TIMEOUT),
      time_created_ms_(rtc::Time()) {
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

uint64_t Connection::priority() const {
  uint64_t priority = 0;
  // RFC 5245 - 5.7.2.  Computing Pair Priority and Ordering Pairs
  // Let G be the priority for the candidate provided by the controlling
  // agent.  Let D be the priority for the candidate provided by the
  // controlled agent.
  // pair priority = 2^32*MIN(G,D) + 2*MAX(G,D) + (G>D?1:0)
  IceRole role = port_->GetIceRole();
  if (role != ICEROLE_UNKNOWN) {
    uint32_t g = 0;
    uint32_t d = 0;
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

void Connection::set_write_state(WriteState value) {
  WriteState old_value = write_state_;
  write_state_ = value;
  if (value != old_value) {
    LOG_J(LS_VERBOSE, this) << "set_write_state from: " << old_value << " to "
                            << value;
    SignalStateChange(this);
  }
}

void Connection::set_receiving(bool value) {
  if (value != receiving_) {
    LOG_J(LS_VERBOSE, this) << "set_receiving to " << value;
    receiving_ = value;
    SignalStateChange(this);
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
    LOG_J(LS_VERBOSE, this) << "set_connected from: " << old_value << " to "
                            << value;
  }
}

void Connection::set_use_candidate_attr(bool enable) {
  use_candidate_attr_ = enable;
}

void Connection::OnSendStunPacket(const void* data, size_t size,
                                  StunRequest* req) {
  rtc::PacketOptions options(port_->DefaultDscpValue());
  auto err = port_->SendTo(
      data, size, remote_candidate_.address(), options, false);
  if (err < 0) {
    LOG_J(LS_WARNING, this) << "Failed to send STUN ping "
                            << " err=" << err
                            << " id=" << rtc::hex_encode(req->id());
  }
}

void Connection::OnReadPacket(
  const char* data, size_t size, const rtc::PacketTime& packet_time) {
  rtc::scoped_ptr<IceMessage> msg;
  std::string remote_ufrag;
  const rtc::SocketAddress& addr(remote_candidate_.address());
  if (!port_->GetStunMessage(data, size, addr, msg.accept(), &remote_ufrag)) {
    // The packet did not parse as a valid STUN message
    // This is a data packet, pass it along.
    set_receiving(true);
    last_data_received_ = rtc::Time();
    recv_rate_tracker_.AddSamples(size);
    SignalReadPacket(this, data, size, packet_time);

    // If timed out sending writability checks, start up again
    if (!pruned_ && (write_state_ == STATE_WRITE_TIMEOUT)) {
      LOG(LS_WARNING) << "Received a data packet on a timed-out Connection. "
                      << "Resetting state to STATE_WRITE_INIT.";
      set_write_state(STATE_WRITE_INIT);
    }
  } else if (!msg) {
    // The packet was STUN, but failed a check and was handled internally.
  } else {
    // The packet is STUN and passed the Port checks.
    // Perform our own checks to ensure this packet is valid.
    // If this is a STUN request, then update the receiving bit and respond.
    // If this is a STUN response, then update the writable bit.
    // Log at LS_INFO if we receive a ping on an unwritable connection.
    rtc::LoggingSeverity sev = (!writable() ? rtc::LS_INFO : rtc::LS_VERBOSE);
    switch (msg->type()) {
      case STUN_BINDING_REQUEST:
        LOG_JV(sev, this) << "Received STUN ping"
                          << ", id=" << rtc::hex_encode(msg->transaction_id());

        if (remote_ufrag == remote_candidate_.username()) {
          HandleBindingRequest(msg.get());
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
        if (msg->ValidateMessageIntegrity(
                data, size, remote_candidate().password())) {
          requests_.CheckResponse(msg.get());
        }
        // Otherwise silently discard the response message.
        break;

      // Remote end point sent an STUN indication instead of regular binding
      // request. In this case |last_ping_received_| will be updated but no
      // response will be sent.
      case STUN_BINDING_INDICATION:
        ReceivedPing();
        break;

      default:
        ASSERT(false);
        break;
    }
  }
}

void Connection::HandleBindingRequest(IceMessage* msg) {
  // This connection should now be receiving.
  ReceivedPing();

  const rtc::SocketAddress& remote_addr = remote_candidate_.address();
  const std::string& remote_ufrag = remote_candidate_.username();
  // Check for role conflicts.
  if (!port_->MaybeIceRoleConflict(remote_addr, msg, remote_ufrag)) {
    // Received conflicting role from the peer.
    LOG(LS_INFO) << "Received conflicting role from the peer.";
    return;
  }

  // This is a validated stun request from remote peer.
  port_->SendBindingResponse(msg, remote_addr);

  // If it timed out on writing check, start up again
  if (!pruned_ && write_state_ == STATE_WRITE_TIMEOUT) {
    set_write_state(STATE_WRITE_INIT);
  }

  if (port_->GetIceRole() == ICEROLE_CONTROLLED) {
    const StunByteStringAttribute* use_candidate_attr =
        msg->GetByteString(STUN_ATTR_USE_CANDIDATE);
    if (use_candidate_attr) {
      set_nominated(true);
      SignalNominated(this);
    }
  }
}

void Connection::OnReadyToSend() {
  if (write_state_ == STATE_WRITABLE) {
    SignalReadyToSend(this);
  }
}

void Connection::Prune() {
  if (!pruned_ || active()) {
    LOG_J(LS_VERBOSE, this) << "Connection pruned";
    pruned_ = true;
    requests_.Clear();
    set_write_state(STATE_WRITE_TIMEOUT);
  }
}

void Connection::Destroy() {
  LOG_J(LS_VERBOSE, this) << "Connection destroyed";
  port_->thread()->Post(this, MSG_DELETE);
}

void Connection::FailAndDestroy() {
  set_state(Connection::STATE_FAILED);
  Destroy();
}

void Connection::PrintPingsSinceLastResponse(std::string* s, size_t max) {
  std::ostringstream oss;
  oss << std::boolalpha;
  if (pings_since_last_response_.size() > max) {
    for (size_t i = 0; i < max; i++) {
      const SentPing& ping = pings_since_last_response_[i];
      oss << rtc::hex_encode(ping.id) << " ";
    }
    oss << "... " << (pings_since_last_response_.size() - max) << " more";
  } else {
    for (const SentPing& ping : pings_since_last_response_) {
      oss << rtc::hex_encode(ping.id) << " ";
    }
  }
  *s = oss.str();
}

void Connection::UpdateState(uint32_t now) {
  uint32_t rtt = ConservativeRTTEstimate(rtt_);

  if (LOG_CHECK_LEVEL(LS_VERBOSE)) {
    std::string pings;
    PrintPingsSinceLastResponse(&pings, 5);
    LOG_J(LS_VERBOSE, this) << "UpdateState()"
                            << ", ms since last received response="
                            << now - last_ping_response_received_
                            << ", ms since last received data="
                            << now - last_data_received_
                            << ", rtt=" << rtt
                            << ", pings_since_last_response=" << pings;
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
    uint32_t max_pings = CONNECTION_WRITE_CONNECT_FAILURES;
    LOG_J(LS_INFO, this) << "Unwritable after " << max_pings
                         << " ping failures and "
                         << now - pings_since_last_response_[0].sent_time
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
                         << now - pings_since_last_response_[0].sent_time
                         << " ms without a response"
                         << ", rtt=" << rtt;
    set_write_state(STATE_WRITE_TIMEOUT);
  }

  // Check the receiving state.
  uint32_t last_recv_time = last_received();
  bool receiving = now <= last_recv_time + receiving_timeout_;
  set_receiving(receiving);
  if (dead(now)) {
    Destroy();
  }
}

void Connection::Ping(uint32_t now) {
  last_ping_sent_ = now;
  ConnectionRequest *req = new ConnectionRequest(this);
  pings_since_last_response_.push_back(SentPing(req->id(), now));
  LOG_J(LS_VERBOSE, this) << "Sending STUN ping "
                          << ", id=" << rtc::hex_encode(req->id());
  requests_.Send(req);
  state_ = STATE_INPROGRESS;
}

void Connection::ReceivedPing() {
  set_receiving(true);
  last_ping_received_ = rtc::Time();
}

void Connection::ReceivedPingResponse() {
  // We've already validated that this is a STUN binding response with
  // the correct local and remote username for this connection.
  // So if we're not already, become writable. We may be bringing a pruned
  // connection back to life, but if we don't really want it, we can always
  // prune it again.
  set_receiving(true);
  set_write_state(STATE_WRITABLE);
  set_state(STATE_SUCCEEDED);
  pings_since_last_response_.clear();
  last_ping_response_received_ = rtc::Time();
}

bool Connection::dead(uint32_t now) const {
  if (last_received() > 0) {
    // If it has ever received anything, we keep it alive until it hasn't
    // received anything for DEAD_CONNECTION_RECEIVE_TIMEOUT. This covers the
    // normal case of a successfully used connection that stops working. This
    // also allows a remote peer to continue pinging over a locally inactive
    // (pruned) connection.
    return (now > (last_received() + DEAD_CONNECTION_RECEIVE_TIMEOUT));
  }

  if (active()) {
    // If it has never received anything, keep it alive as long as it is
    // actively pinging and not pruned. Otherwise, the connection might be
    // deleted before it has a chance to ping. This is the normal case for a
    // new connection that is pinging but hasn't received anything yet.
    return false;
  }

  // If it has never received anything and is not actively pinging (pruned), we
  // keep it around for at least MIN_CONNECTION_LIFETIME to prevent connections
  // from being pruned too quickly during a network change event when two
  // networks would be up simultaneously but only for a brief period.
  return now > (time_created_ms_ + MIN_CONNECTION_LIFETIME);
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
  const char RECEIVE_STATE_ABBREV[2] = {
    '-',  // not receiving (false)
    'R',  // receiving (true)
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
     << RECEIVE_STATE_ABBREV[receiving()]
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
  // Log at LS_INFO if we receive a ping response on an unwritable
  // connection.
  rtc::LoggingSeverity sev = !writable() ? rtc::LS_INFO : rtc::LS_VERBOSE;

  uint32_t rtt = request->Elapsed();

  ReceivedPingResponse();

  if (LOG_CHECK_LEVEL_V(sev)) {
    bool use_candidate = (
        response->GetByteString(STUN_ATTR_USE_CANDIDATE) != nullptr);
    std::string pings;
    PrintPingsSinceLastResponse(&pings, 5);
    LOG_JV(sev, this) << "Received STUN ping response"
                      << ", id=" << rtc::hex_encode(request->id())
                      << ", code=0"  // Makes logging easier to parse.
                      << ", rtt=" << rtt
                      << ", use_candidate=" << use_candidate
                      << ", pings_since_last_response=" << pings;
  }

  rtt_ = (RTT_RATIO * rtt_ + rtt) / (RTT_RATIO + 1);

  MaybeAddPrflxCandidate(request, response);
}

void Connection::OnConnectionRequestErrorResponse(ConnectionRequest* request,
                                                  StunMessage* response) {
  const StunErrorCodeAttribute* error_attr = response->GetErrorCode();
  int error_code = STUN_ERROR_GLOBAL_FAILURE;
  if (error_attr) {
    error_code = error_attr->code();
  }

  LOG_J(LS_INFO, this) << "Received STUN error response"
                       << " id=" << rtc::hex_encode(request->id())
                       << " code=" << error_code
                       << " rtt=" << request->Elapsed();

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
    FailAndDestroy();
  }
}

void Connection::OnConnectionRequestTimeout(ConnectionRequest* request) {
  // Log at LS_INFO if we miss a ping on a writable connection.
  rtc::LoggingSeverity sev = writable() ? rtc::LS_INFO : rtc::LS_VERBOSE;
  LOG_JV(sev, this) << "Timing-out STUN ping "
                    << rtc::hex_encode(request->id())
                    << " after " << request->Elapsed() << " ms";
}

void Connection::OnConnectionRequestSent(ConnectionRequest* request) {
  // Log at LS_INFO if we send a ping on an unwritable connection.
  rtc::LoggingSeverity sev = !writable() ? rtc::LS_INFO : rtc::LS_VERBOSE;
  bool use_candidate = use_candidate_attr();
  LOG_JV(sev, this) << "Sent STUN ping"
                    << ", id=" << rtc::hex_encode(request->id())
                    << ", use_candidate=" << use_candidate;
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
  LOG_J(LS_INFO, this) << "Connection deleted";
  SignalDestroyed(this);
  delete this;
}

uint32_t Connection::last_received() const {
  return std::max(last_data_received_,
             std::max(last_ping_received_, last_ping_response_received_));
}

size_t Connection::recv_bytes_second() {
  return round(recv_rate_tracker_.ComputeRate());
}

size_t Connection::recv_total_bytes() {
  return recv_rate_tracker_.TotalSampleCount();
}

size_t Connection::sent_bytes_second() {
  return round(send_rate_tracker_.ComputeRate());
}

size_t Connection::sent_total_bytes() {
  return send_rate_tracker_.TotalSampleCount();
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
  const uint32_t priority = priority_attr->value();
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

ProxyConnection::ProxyConnection(Port* port,
                                 size_t index,
                                 const Candidate& remote_candidate)
    : Connection(port, index, remote_candidate) {}

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
    send_rate_tracker_.AddSamples(sent);
  }
  return sent;
}

}  // namespace cricket

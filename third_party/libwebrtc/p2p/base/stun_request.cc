/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "p2p/base/stun_request.h"

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "absl/memory/memory.h"
#include "rtc_base/checks.h"
#include "rtc_base/helpers.h"
#include "rtc_base/logging.h"
#include "rtc_base/string_encode.h"
#include "rtc_base/time_utils.h"  // For TimeMillis

namespace cricket {

const uint32_t MSG_STUN_SEND = 1;

// RFC 5389 says SHOULD be 500ms.
// For years, this was 100ms, but for networks that
// experience moments of high RTT (such as 2G networks), this doesn't
// work well.
const int STUN_INITIAL_RTO = 250;  // milliseconds

// The timeout doubles each retransmission, up to this many times
// RFC 5389 says SHOULD retransmit 7 times.
// This has been 8 for years (not sure why).
const int STUN_MAX_RETRANSMISSIONS = 8;           // Total sends: 9

// We also cap the doubling, even though the standard doesn't say to.
// This has been 1.6 seconds for years, but for networks that
// experience moments of high RTT (such as 2G networks), this doesn't
// work well.
const int STUN_MAX_RTO = 8000;  // milliseconds, or 5 doublings

StunRequestManager::StunRequestManager(rtc::Thread* thread) : thread_(thread) {}

StunRequestManager::~StunRequestManager() = default;

void StunRequestManager::Send(StunRequest* request) {
  SendDelayed(request, 0);
}

void StunRequestManager::SendDelayed(StunRequest* request, int delay) {
  RTC_DCHECK_RUN_ON(thread_);
  RTC_DCHECK_EQ(this, request->manager());
  request->Construct();
  auto [iter, was_inserted] =
      requests_.emplace(request->id(), absl::WrapUnique(request));
  RTC_DCHECK(was_inserted);
  if (delay > 0) {
    thread_->PostDelayed(RTC_FROM_HERE, delay, iter->second.get(),
                         MSG_STUN_SEND, NULL);
  } else {
    thread_->Send(RTC_FROM_HERE, iter->second.get(), MSG_STUN_SEND, NULL);
  }
}

void StunRequestManager::FlushForTest(int msg_type) {
  RTC_DCHECK_RUN_ON(thread_);
  for (const auto& [unused, request] : requests_) {
    if (msg_type == kAllRequests || msg_type == request->type()) {
      thread_->Clear(request.get(), MSG_STUN_SEND);
      thread_->Send(RTC_FROM_HERE, request.get(), MSG_STUN_SEND, NULL);
    }
  }
}

bool StunRequestManager::HasRequestForTest(int msg_type) {
  RTC_DCHECK_RUN_ON(thread_);
  for (const auto& [unused, request] : requests_) {
    if (msg_type == kAllRequests || msg_type == request->type()) {
      return true;
    }
  }
  return false;
}

void StunRequestManager::Clear() {
  RTC_DCHECK_RUN_ON(thread_);
  requests_.clear();
}

bool StunRequestManager::CheckResponse(StunMessage* msg) {
  RTC_DCHECK_RUN_ON(thread_);
  RequestMap::iterator iter = requests_.find(msg->transaction_id());
  if (iter == requests_.end()) {
    // TODO(pthatcher): Log unknown responses without being too spammy
    // in the logs.
    return false;
  }

  StunRequest* request = iter->second.get();

  // Now that we know the request, we can see if the response is
  // integrity-protected or not.
  // For some tests, the message integrity is not set in the request.
  // Complain, and then don't check.
  bool skip_integrity_checking = false;
  if (request->msg()->integrity() == StunMessage::IntegrityStatus::kNotSet) {
    skip_integrity_checking = true;
  } else {
    msg->ValidateMessageIntegrity(request->msg()->password());
  }

  bool success = true;

  if (!msg->GetNonComprehendedAttributes().empty()) {
    // If a response contains unknown comprehension-required attributes, it's
    // simply discarded and the transaction is considered failed. See RFC5389
    // sections 7.3.3 and 7.3.4.
    RTC_LOG(LS_ERROR) << ": Discarding response due to unknown "
                         "comprehension-required attribute.";
    success = false;
  } else if (msg->type() == GetStunSuccessResponseType(request->type())) {
    if (!msg->IntegrityOk() && !skip_integrity_checking) {
      return false;
    }
    request->OnResponse(msg);
  } else if (msg->type() == GetStunErrorResponseType(request->type())) {
    request->OnErrorResponse(msg);
  } else {
    RTC_LOG(LS_ERROR) << "Received response with wrong type: " << msg->type()
                      << " (expecting "
                      << GetStunSuccessResponseType(request->type()) << ")";
    return false;
  }

  requests_.erase(iter);
  return success;
}

bool StunRequestManager::empty() const {
  RTC_DCHECK_RUN_ON(thread_);
  return requests_.empty();
}

bool StunRequestManager::CheckResponse(const char* data, size_t size) {
  RTC_DCHECK_RUN_ON(thread_);
  // Check the appropriate bytes of the stream to see if they match the
  // transaction ID of a response we are expecting.

  if (size < 20)
    return false;

  std::string id;
  id.append(data + kStunTransactionIdOffset, kStunTransactionIdLength);

  RequestMap::iterator iter = requests_.find(id);
  if (iter == requests_.end()) {
    // TODO(pthatcher): Log unknown responses without being too spammy
    // in the logs.
    return false;
  }

  // Parse the STUN message and continue processing as usual.

  rtc::ByteBufferReader buf(data, size);
  std::unique_ptr<StunMessage> response(iter->second->msg_->CreateNew());
  if (!response->Read(&buf)) {
    RTC_LOG(LS_WARNING) << "Failed to read STUN response "
                        << rtc::hex_encode(id);
    return false;
  }

  return CheckResponse(response.get());
}

void StunRequestManager::OnRequestTimedOut(StunRequest* request) {
  RTC_DCHECK_RUN_ON(thread_);
  requests_.erase(request->id());
}

StunRequest::StunRequest(StunRequestManager& manager)
    : manager_(manager),
      msg_(new StunMessage()),
      tstamp_(0),
      count_(0),
      timeout_(false) {
  msg_->SetTransactionID(rtc::CreateRandomString(kStunTransactionIdLength));
}

StunRequest::StunRequest(StunRequestManager& manager,
                         std::unique_ptr<StunMessage> request)
    : manager_(manager),
      msg_(std::move(request)),
      tstamp_(0),
      count_(0),
      timeout_(false) {
  msg_->SetTransactionID(rtc::CreateRandomString(kStunTransactionIdLength));
}

StunRequest::~StunRequest() {
  manager_.network_thread()->Clear(this);
}

void StunRequest::Construct() {
  if (msg_->type() == 0) {
    Prepare(msg_.get());
    RTC_DCHECK(msg_->type() != 0);
  }
}

int StunRequest::type() {
  RTC_DCHECK(msg_ != NULL);
  return msg_->type();
}

const StunMessage* StunRequest::msg() const {
  return msg_.get();
}

int StunRequest::Elapsed() const {
  RTC_DCHECK_RUN_ON(network_thread());
  return static_cast<int>(rtc::TimeMillis() - tstamp_);
}

void StunRequest::OnMessage(rtc::Message* pmsg) {
  RTC_DCHECK_RUN_ON(network_thread());
  RTC_DCHECK(pmsg->message_id == MSG_STUN_SEND);

  if (timeout_) {
    OnTimeout();
    manager_.OnRequestTimedOut(this);
    return;
  }

  tstamp_ = rtc::TimeMillis();

  rtc::ByteBufferWriter buf;
  msg_->Write(&buf);
  manager_.SignalSendPacket(buf.Data(), buf.Length(), this);

  OnSent();
  manager_.network_thread()->PostDelayed(RTC_FROM_HERE, resend_delay(), this,
                                         MSG_STUN_SEND, NULL);
}

void StunRequest::OnSent() {
  RTC_DCHECK_RUN_ON(network_thread());
  count_ += 1;
  int retransmissions = (count_ - 1);
  if (retransmissions >= STUN_MAX_RETRANSMISSIONS) {
    timeout_ = true;
  }
  RTC_DLOG(LS_VERBOSE) << "Sent STUN request " << count_
                       << "; resend delay = " << resend_delay();
}

int StunRequest::resend_delay() {
  RTC_DCHECK_RUN_ON(network_thread());
  if (count_ == 0) {
    return 0;
  }
  int retransmissions = (count_ - 1);
  int rto = STUN_INITIAL_RTO << retransmissions;
  return std::min(rto, STUN_MAX_RTO);
}

void StunRequest::set_timed_out() {
  RTC_DCHECK_RUN_ON(network_thread());
  timeout_ = true;
}

}  // namespace cricket

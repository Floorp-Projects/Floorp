// SPDX-License-Identifier: MPL-2.0

#pragma once

#include "Protocol.h"
#include <mach/mach.h>
#include <optional>
#include <functional>
#include <memory>

namespace floorp::shim {

class Port {
 public:
  Port() = default;
  explicit Port(mach_port_t name, bool receive = false)
      : name_(name), receive_(receive) {}
  ~Port();
  Port(const Port&) = delete;
  Port& operator=(const Port&) = delete;
  Port(Port&& other) noexcept;
  Port& operator=(Port&& other) noexcept;
  mach_port_t get() const { return name_; }
  mach_port_t release();
  explicit operator bool() const { return MACH_PORT_VALID(name_); }
  static Port Receive();
  static Port Lookup(NSString* service);

 private:
  mach_port_t name_ = MACH_PORT_NULL;
  bool receive_ = false;
};

struct Message {
  WireHeader header;
  __strong NSDictionary* payload = nil;
  audit_token_t auditToken = {};
  Port attachment;
};

enum class ReceiveStatus { Message, Timeout, Invalid, Failed };
struct ReceiveResult {
  ReceiveStatus status;
  std::optional<Message> message;
};

bool SendMessage(mach_port_t destination, MessageType type, uint64_t sequence,
                 NSDictionary* payload, mach_port_t attachment = MACH_PORT_NULL,
                 mach_msg_type_name_t disposition = MACH_MSG_TYPE_COPY_SEND);
ReceiveResult ReceiveMessage(mach_port_t receivePort, mach_msg_timeout_t timeout);

// Ordered, bounded delivery on a private worker queue. Enqueue only admits a
// message; delivery failure is reported asynchronously on the main queue.
// Stop never blocks and suppresses pending failure callbacks. A send already
// accepted by the kernel cannot be recalled. Both destination and attachment
// send rights are independently retained until delivery or cancellation.
class MessageSender {
 public:
  MessageSender(mach_port_t destination, std::function<void()> onFailure);
  ~MessageSender();
  MessageSender(const MessageSender&) = delete;
  MessageSender& operator=(const MessageSender&) = delete;
  bool Enqueue(MessageType type, uint64_t sequence, NSDictionary* payload,
               mach_port_t attachment = MACH_PORT_NULL);
  void Stop();

 private:
  struct State;
  std::shared_ptr<State> state_;
};

}  // namespace floorp::shim

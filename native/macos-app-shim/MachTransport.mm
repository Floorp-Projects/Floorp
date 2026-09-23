// SPDX-License-Identifier: MPL-2.0

#include "MachTransport.h"

#include <servers/bootstrap.h>
#include <cstring>
#include <utility>
#include <vector>
#include <chrono>
#include <deque>
#include <mutex>

namespace floorp::shim {

Port::~Port() {
  if (!MACH_PORT_VALID(name_)) return;
  if (receive_) {
    mach_port_mod_refs(mach_task_self(), name_, MACH_PORT_RIGHT_RECEIVE, -1);
  } else {
    mach_port_deallocate(mach_task_self(), name_);
  }
}

Port::Port(Port&& other) noexcept
    : name_(other.release()), receive_(other.receive_) {}

Port& Port::operator=(Port&& other) noexcept {
  if (this != &other) {
    Port old(std::move(*this));
    name_ = other.release();
    receive_ = other.receive_;
  }
  return *this;
}

mach_port_t Port::release() {
  return std::exchange(name_, MACH_PORT_NULL);
}

Port Port::Receive() {
  mach_port_t port = MACH_PORT_NULL;
  if (mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &port) != KERN_SUCCESS) {
    return {};
  }
  return Port(port, true);
}

Port Port::Lookup(NSString* service) {
  if (!service.length || [service lengthOfBytesUsingEncoding:NSUTF8StringEncoding] >= BOOTSTRAP_MAX_NAME_LEN) {
    return {};
  }
  mach_port_t port = MACH_PORT_NULL;
  if (bootstrap_look_up(bootstrap_port, service.UTF8String, &port) != KERN_SUCCESS) {
    return {};
  }
  return Port(port);
}

static kern_return_t SendEncoded(mach_port_t destination, MessageType type,
                                uint64_t sequence, NSData* data,
                                mach_port_t attachment,
                                mach_msg_type_name_t disposition) {
  bool attached = MACH_PORT_VALID(attachment);
  size_t prefix = sizeof(mach_msg_header_t) + sizeof(mach_msg_body_t) +
                  (attached ? sizeof(mach_msg_port_descriptor_t) : 0);
  size_t length = prefix + sizeof(WireHeader) + data.length;
  size_t aligned = (length + 3) & ~size_t(3);
  std::vector<uint8_t> buffer(aligned, 0);
  auto* header = reinterpret_cast<mach_msg_header_t*>(buffer.data());
  header->msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, 0) | MACH_MSGH_BITS_COMPLEX;
  header->msgh_size = static_cast<mach_msg_size_t>(aligned);
  header->msgh_remote_port = destination;
  header->msgh_id = kProtocolMagic;
  auto* body = reinterpret_cast<mach_msg_body_t*>(buffer.data() + sizeof(*header));
  body->msgh_descriptor_count = attached ? 1 : 0;
  if (attached) {
    auto* descriptor = reinterpret_cast<mach_msg_port_descriptor_t*>(body + 1);
    descriptor->name = attachment;
    descriptor->disposition = disposition;
    descriptor->type = MACH_MSG_PORT_DESCRIPTOR;
  }
  WireHeader wire;
  wire.type = static_cast<uint32_t>(type);
  wire.sequence = sequence;
  wire.payloadBytes = static_cast<uint32_t>(data.length);
  std::memcpy(buffer.data() + prefix, &wire, sizeof(wire));
  std::memcpy(buffer.data() + prefix + sizeof(wire), data.bytes, data.length);
  return mach_msg(header, MACH_SEND_MSG | MACH_SEND_TIMEOUT,
                                  header->msgh_size, 0, MACH_PORT_NULL, 100,
                                  MACH_PORT_NULL);
}

bool SendMessage(mach_port_t destination, MessageType type, uint64_t sequence,
                 NSDictionary* payload, mach_port_t attachment,
                 mach_msg_type_name_t disposition) {
  NSData* data = EncodePayload(payload);
  if (!data || !MACH_PORT_VALID(destination) || !sequence ||
      !IsKnownMessage(static_cast<uint32_t>(type)) ||
      (disposition != MACH_MSG_TYPE_COPY_SEND && disposition != MACH_MSG_TYPE_MAKE_SEND)) return false;
  return SendEncoded(destination, type, sequence, data, attachment, disposition) == KERN_SUCCESS;
}

struct MessageSender::State : std::enable_shared_from_this<MessageSender::State> {
  struct Pending {
    MessageType type;
    uint64_t sequence;
    __strong NSData* data;
    Port attachment;
  };
  // One maximal replacement frame is Begin + 128 Set + 128 Remove + Commit.
  static constexpr size_t kMaxMessages = 512;
  static constexpr size_t kMaxBytes = 4 * 1024 * 1024;
  std::mutex mutex;
  std::deque<Pending> pending;
  Port destination;
  __strong dispatch_queue_t worker = dispatch_queue_create("org.floorp.app-shim.send", DISPATCH_QUEUE_SERIAL);
  std::function<void()> onFailure;
  bool stopped = false;
  bool cancelled = false;
  bool running = false;
  size_t count = 0;
  size_t bytes = 0;
  uint64_t lastSequence = 0;

  void Pump() {
    for (;;) {
      @autoreleasepool {
        std::optional<Pending> next;
        {
          std::lock_guard<std::mutex> lock(mutex);
          if (stopped || pending.empty()) {
            running = false;
            return;
          }
          next.emplace(std::move(pending.front()));
          pending.pop_front();
        }
        auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        kern_return_t result;
        for (;;) {
          {
            std::lock_guard<std::mutex> lock(mutex);
            if (stopped) return;
          }
          result = SendEncoded(destination.get(), next->type, next->sequence,
                               next->data, next->attachment.get(), MACH_MSG_TYPE_COPY_SEND);
          if (result != MACH_SEND_TIMED_OUT || std::chrono::steady_clock::now() >= deadline) break;
        }
        {
          std::lock_guard<std::mutex> lock(mutex);
          if (stopped) return;
          --count;
          bytes -= next->data.length;
          if (result == KERN_SUCCESS) continue;
          stopped = true;
          pending.clear();
          count = bytes = 0;
        }
        fprintf(stderr, "App Shim transport delivery failed: message=%u, mach=0x%x\n",
                static_cast<unsigned>(next->type), result);
        auto self = shared_from_this();
        dispatch_async(dispatch_get_main_queue(), ^{
          std::function<void()> callback;
          {
            std::lock_guard<std::mutex> lock(self->mutex);
            if (!self->cancelled) callback = self->onFailure;
          }
          if (callback) callback();
        });
        return;
      }
    }
  }
};

MessageSender::MessageSender(mach_port_t destination, std::function<void()> onFailure)
    : state_(std::make_shared<State>()) {
  state_->onFailure = std::move(onFailure);
  if (!MACH_PORT_VALID(destination) ||
      mach_port_mod_refs(mach_task_self(), destination, MACH_PORT_RIGHT_SEND, 1) != KERN_SUCCESS) {
    state_->stopped = true;
    return;
  }
  state_->destination = Port(destination);
}

MessageSender::~MessageSender() { Stop(); }

bool MessageSender::Enqueue(MessageType type, uint64_t sequence, NSDictionary* payload,
                            mach_port_t attachment) {
  NSData* data = EncodePayload(payload);
  if (!data || !sequence || !IsKnownMessage(static_cast<uint32_t>(type))) return false;
  Port retainedAttachment;
  if (MACH_PORT_VALID(attachment)) {
    if (mach_port_mod_refs(mach_task_self(), attachment, MACH_PORT_RIGHT_SEND, 1) != KERN_SUCCESS) return false;
    retainedAttachment = Port(attachment);
  } else if (attachment != MACH_PORT_NULL) {
    return false;
  }
  auto state = state_;
  bool start = false;
  {
    std::lock_guard<std::mutex> lock(state->mutex);
    if (state->stopped || sequence <= state->lastSequence ||
        state->count >= State::kMaxMessages || data.length > State::kMaxBytes - state->bytes) return false;
    state->lastSequence = sequence;
    ++state->count;
    state->bytes += data.length;
    state->pending.push_back({type, sequence, data, std::move(retainedAttachment)});
    if (!state->running) state->running = start = true;
  }
  if (start) dispatch_async(state->worker, ^{ state->Pump(); });
  return true;
}

void MessageSender::Stop() {
  if (!state_) return;
  std::lock_guard<std::mutex> lock(state_->mutex);
  state_->stopped = state_->cancelled = true;
  state_->pending.clear();
  state_->count = state_->bytes = 0;
  state_->onFailure = nullptr;
}

ReceiveResult ReceiveMessage(mach_port_t receivePort, mach_msg_timeout_t timeout) {
  constexpr size_t kMaximum = sizeof(mach_msg_header_t) + sizeof(mach_msg_body_t) +
                              sizeof(mach_msg_port_descriptor_t) + sizeof(WireHeader) +
                              kMaxPayloadBytes + sizeof(mach_msg_max_trailer_t) + 8;
  std::vector<uint8_t> buffer(kMaximum, 0);
  auto* header = reinterpret_cast<mach_msg_header_t*>(buffer.data());
  mach_msg_option_t options = MACH_RCV_MSG | MACH_RCV_TIMEOUT |
      MACH_RCV_TRAILER_TYPE(MACH_MSG_TRAILER_FORMAT_0) |
      MACH_RCV_TRAILER_ELEMENTS(MACH_RCV_TRAILER_AUDIT);
  kern_return_t result = mach_msg(header, options, 0, static_cast<mach_msg_size_t>(buffer.size()),
                                  receivePort, timeout, MACH_PORT_NULL);
  if (result == MACH_RCV_TIMED_OUT) return {ReceiveStatus::Timeout, std::nullopt};
  if (result != KERN_SUCCESS) return {ReceiveStatus::Failed, std::nullopt};
  auto invalid = [&]() {
    mach_msg_destroy(header);
    return ReceiveResult{ReceiveStatus::Invalid, std::nullopt};
  };
  size_t minimum = sizeof(*header) + sizeof(mach_msg_body_t);
  if (header->msgh_id != static_cast<mach_msg_id_t>(kProtocolMagic) ||
      !(header->msgh_bits & MACH_MSGH_BITS_COMPLEX) || header->msgh_size < minimum ||
      header->msgh_remote_port != MACH_PORT_NULL) {
    return invalid();
  }
  auto* body = reinterpret_cast<mach_msg_body_t*>(buffer.data() + sizeof(*header));
  if (body->msgh_descriptor_count > 1) return invalid();
  size_t prefix = minimum + body->msgh_descriptor_count * sizeof(mach_msg_port_descriptor_t);
  if (header->msgh_size < prefix + sizeof(WireHeader)) return invalid();
  mach_msg_port_descriptor_t* descriptor = nullptr;
  if (body->msgh_descriptor_count) {
    descriptor = reinterpret_cast<mach_msg_port_descriptor_t*>(body + 1);
    if (descriptor->type != MACH_MSG_PORT_DESCRIPTOR ||
        descriptor->disposition != MACH_MSG_TYPE_PORT_SEND || !MACH_PORT_VALID(descriptor->name)) {
      return invalid();
    }
  }
  Message message;
  std::memcpy(&message.header, buffer.data() + prefix, sizeof(WireHeader));
  const WireHeader& wire = message.header;
  size_t expectedLength = (prefix + sizeof(WireHeader) + wire.payloadBytes + 3) & ~size_t(3);
  if (wire.magic != kProtocolMagic || wire.major != kProtocolMajor || wire.minor > kProtocolMinor ||
      !IsKnownMessage(wire.type) || !wire.sequence || wire.reserved || wire.reserved2 ||
      wire.payloadBytes > kMaxPayloadBytes || expectedLength != header->msgh_size) {
    return invalid();
  }
  size_t trailerOffset = (header->msgh_size + 3) & ~size_t(3);
  if (trailerOffset + sizeof(mach_msg_audit_trailer_t) > buffer.size()) return invalid();
  auto* trailer = reinterpret_cast<mach_msg_audit_trailer_t*>(buffer.data() + trailerOffset);
  if (trailer->msgh_trailer_type != MACH_MSG_TRAILER_FORMAT_0 ||
      trailer->msgh_trailer_size < sizeof(mach_msg_audit_trailer_t)) {
    return invalid();
  }
  message.auditToken = trailer->msgh_audit;
  message.payload = DecodePayload([NSData dataWithBytes:buffer.data() + prefix + sizeof(WireHeader)
                                                length:wire.payloadBytes]);
  if (!message.payload) return invalid();
  if (descriptor) {
    message.attachment = Port(descriptor->name);
    descriptor->name = MACH_PORT_NULL;
  }
  mach_msg_destroy(header);
  return {ReceiveStatus::Message, std::move(message)};
}

}  // namespace floorp::shim

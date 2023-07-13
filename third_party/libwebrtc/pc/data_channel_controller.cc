/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "pc/data_channel_controller.h"

#include <utility>

#include "absl/algorithm/container.h"
#include "api/peer_connection_interface.h"
#include "api/rtc_error.h"
#include "pc/peer_connection_internal.h"
#include "pc/sctp_utils.h"
#include "rtc_base/logging.h"

namespace webrtc {

DataChannelController::~DataChannelController() {
#if RTC_DCHECK_IS_ON
  // `sctp_data_channels_n_` might be empty while `sctp_data_channels_` is
  // not. An example of that is when the `DataChannelController` goes out of
  // scope with outstanding channels that have been properly terminated on the
  // network thread but not yet cleared from `sctp_data_channels_`. However,
  // if `sctp_data_channels_n_` is not empty, then `sctp_data_channels_n_` and
  // sctp_data_channels_ should hold the same contents.
  if (!sctp_data_channels_n_.empty()) {
    RTC_DCHECK_EQ(sctp_data_channels_n_.size(), sctp_data_channels_.size());
  }
#endif
}

bool DataChannelController::HasDataChannelsForTest() const {
  RTC_DCHECK_RUN_ON(signaling_thread());
  return !sctp_data_channels_.empty();
}

bool DataChannelController::HasUsedDataChannels() const {
  RTC_DCHECK_RUN_ON(signaling_thread());
  return has_used_data_channels_;
}

RTCError DataChannelController::SendData(
    StreamId sid,
    const SendDataParams& params,
    const rtc::CopyOnWriteBuffer& payload) {
  if (data_channel_transport())
    return DataChannelSendData(sid, params, payload);
  RTC_LOG(LS_ERROR) << "SendData called before transport is ready";
  return RTCError(RTCErrorType::INVALID_STATE);
}

void DataChannelController::AddSctpDataStream(StreamId sid) {
  RTC_DCHECK_RUN_ON(network_thread());
  RTC_DCHECK(sid.HasValue());
  if (data_channel_transport()) {
    data_channel_transport()->OpenChannel(sid.stream_id_int());
  }
}

void DataChannelController::RemoveSctpDataStream(StreamId sid) {
  RTC_DCHECK_RUN_ON(network_thread());
  if (data_channel_transport()) {
    data_channel_transport()->CloseChannel(sid.stream_id_int());
  }
}

void DataChannelController::OnChannelStateChanged(
    SctpDataChannel* channel,
    DataChannelInterface::DataState state) {
  RTC_DCHECK_RUN_ON(signaling_thread());
  if (state == DataChannelInterface::DataState::kClosed)
    OnSctpDataChannelClosed(channel);

  pc_->OnSctpDataChannelStateChanged(channel->internal_id(), state);
}

void DataChannelController::OnDataReceived(
    int channel_id,
    DataMessageType type,
    const rtc::CopyOnWriteBuffer& buffer) {
  RTC_DCHECK_RUN_ON(network_thread());

  if (HandleOpenMessage_n(channel_id, type, buffer))
    return;

  signaling_thread()->PostTask(
      SafeTask(signaling_safety_.flag(), [this, channel_id, type, buffer] {
        RTC_DCHECK_RUN_ON(signaling_thread());
        // TODO(bugs.webrtc.org/11547): The data being received should be
        // delivered on the network thread.
        auto it = FindChannel(StreamId(channel_id));
        if (it != sctp_data_channels_.end())
          (*it)->OnDataReceived(type, buffer);
      }));
}

void DataChannelController::OnChannelClosing(int channel_id) {
  RTC_DCHECK_RUN_ON(network_thread());
  signaling_thread()->PostTask(
      SafeTask(signaling_safety_.flag(), [this, channel_id] {
        RTC_DCHECK_RUN_ON(signaling_thread());
        // TODO(bugs.webrtc.org/11547): Should run on the network thread.
        auto it = FindChannel(StreamId(channel_id));
        if (it != sctp_data_channels_.end())
          (*it)->OnClosingProcedureStartedRemotely();
      }));
}

void DataChannelController::OnChannelClosed(int channel_id) {
  RTC_DCHECK_RUN_ON(network_thread());
  StreamId sid(channel_id);
  sid_allocator_.ReleaseSid(sid);
  auto it = absl::c_find_if(sctp_data_channels_n_,
                            [&](const auto& c) { return c->sid() == sid; });

  if (it != sctp_data_channels_n_.end())
    sctp_data_channels_n_.erase(it);

  signaling_thread()->PostTask(SafeTask(signaling_safety_.flag(), [this, sid] {
    RTC_DCHECK_RUN_ON(signaling_thread());
    auto it = FindChannel(sid);
    // Remove the channel from our list, close it and free up resources.
    if (it != sctp_data_channels_.end()) {
      rtc::scoped_refptr<SctpDataChannel> channel = std::move(*it);
      // Note: this causes OnSctpDataChannelClosed() to not do anything
      // when called from within `OnClosingProcedureComplete`.
      sctp_data_channels_.erase(it);

      channel->OnClosingProcedureComplete();
    }
  }));
}

void DataChannelController::OnReadyToSend() {
  RTC_DCHECK_RUN_ON(network_thread());
  signaling_thread()->PostTask(SafeTask(signaling_safety_.flag(), [this] {
    RTC_DCHECK_RUN_ON(signaling_thread());
    auto copy = sctp_data_channels_;
    for (const auto& channel : copy)
      channel->OnTransportReady();
  }));
}

void DataChannelController::OnTransportClosed(RTCError error) {
  RTC_DCHECK_RUN_ON(network_thread());
  signaling_thread()->PostTask(
      SafeTask(signaling_safety_.flag(), [this, error] {
        RTC_DCHECK_RUN_ON(signaling_thread());
        OnTransportChannelClosed(error);
      }));
}

void DataChannelController::SetupDataChannelTransport_n() {
  RTC_DCHECK_RUN_ON(network_thread());

  // There's a new data channel transport.  This needs to be signaled to the
  // `sctp_data_channels_` so that they can reopen and reconnect.  This is
  // necessary when bundling is applied.
  NotifyDataChannelsOfTransportCreated();
}

void DataChannelController::TeardownDataChannelTransport_n() {
  RTC_DCHECK_RUN_ON(network_thread());
  if (data_channel_transport()) {
    data_channel_transport()->SetDataSink(nullptr);
  }
  set_data_channel_transport(nullptr);
  sctp_data_channels_n_.clear();
}

void DataChannelController::OnTransportChanged(
    DataChannelTransportInterface* new_data_channel_transport) {
  RTC_DCHECK_RUN_ON(network_thread());
  if (data_channel_transport() &&
      data_channel_transport() != new_data_channel_transport) {
    // Changed which data channel transport is used for `sctp_mid_` (eg. now
    // it's bundled).
    data_channel_transport()->SetDataSink(nullptr);
    set_data_channel_transport(new_data_channel_transport);
    if (new_data_channel_transport) {
      new_data_channel_transport->SetDataSink(this);

      // There's a new data channel transport.  This needs to be signaled to the
      // `sctp_data_channels_` so that they can reopen and reconnect.  This is
      // necessary when bundling is applied.
      NotifyDataChannelsOfTransportCreated();
    }
  }
}

std::vector<DataChannelStats> DataChannelController::GetDataChannelStats()
    const {
  RTC_DCHECK_RUN_ON(signaling_thread());
  std::vector<DataChannelStats> stats;
  stats.reserve(sctp_data_channels_.size());
  for (const auto& channel : sctp_data_channels_)
    stats.push_back(channel->GetStats());
  return stats;
}

bool DataChannelController::HandleOpenMessage_n(
    int channel_id,
    DataMessageType type,
    const rtc::CopyOnWriteBuffer& buffer) {
  if (type != DataMessageType::kControl || !IsOpenMessage(buffer))
    return false;

  // Received OPEN message; parse and signal that a new data channel should
  // be created.
  std::string label;
  InternalDataChannelInit config;
  config.id = channel_id;
  if (!ParseDataChannelOpenMessage(buffer, &label, &config)) {
    RTC_LOG(LS_WARNING) << "Failed to parse the OPEN message for sid "
                        << channel_id;
  } else {
    config.open_handshake_role = InternalDataChannelInit::kAcker;
    signaling_thread()->PostTask(
        SafeTask(signaling_safety_.flag(),
                 [this, label = std::move(label), config = std::move(config)] {
                   RTC_DCHECK_RUN_ON(signaling_thread());
                   OnDataChannelOpenMessage(label, config);
                 }));
  }
  return true;
}

void DataChannelController::OnDataChannelOpenMessage(
    const std::string& label,
    const InternalDataChannelInit& config) {
  auto channel_or_error = InternalCreateDataChannelWithProxy(label, config);
  if (!channel_or_error.ok()) {
    RTC_LOG(LS_ERROR) << "Failed to create DataChannel from the OPEN message."
                      << ToString(channel_or_error.error().type());
    return;
  }

  pc_->Observer()->OnDataChannel(channel_or_error.MoveValue());
  pc_->NoteDataAddedEvent();
}

// RTC_RUN_ON(network_thread())
RTCError DataChannelController::ReserveOrAllocateSid(
    StreamId& sid,
    absl::optional<rtc::SSLRole> fallback_ssl_role) {
  if (sid.HasValue()) {
    return sid_allocator_.ReserveSid(sid)
               ? RTCError::OK()
               : RTCError(RTCErrorType::INVALID_RANGE,
                          "StreamId out of range or reserved.");
  }

  // Attempt to allocate an ID based on the negotiated role.
  absl::optional<rtc::SSLRole> role = pc_->GetSctpSslRole_n();
  if (!role)
    role = fallback_ssl_role;
  if (role) {
    sid = sid_allocator_.AllocateSid(*role);
    if (!sid.HasValue())
      return RTCError(RTCErrorType::RESOURCE_EXHAUSTED);
  }
  // When we get here, we may still not have an ID, but that's a supported case
  // whereby an id will be assigned later.
  RTC_DCHECK(sid.HasValue() || !role);
  return RTCError::OK();
}

RTCErrorOr<rtc::scoped_refptr<DataChannelInterface>>
DataChannelController::InternalCreateDataChannelWithProxy(
    const std::string& label,
    const InternalDataChannelInit& config) {
  RTC_DCHECK_RUN_ON(signaling_thread());
  RTC_DCHECK(!pc_->IsClosed());
  if (!config.IsValid()) {
    LOG_AND_RETURN_ERROR(RTCErrorType::INVALID_PARAMETER,
                         "Invalid DataChannelInit");
  }

  bool ready_to_send = false;
  InternalDataChannelInit new_config = config;
  StreamId sid(new_config.id);
  auto weak_ptr = weak_factory_.GetWeakPtr();
  RTC_DCHECK(weak_ptr);  // Associate with current thread.
  auto ret = network_thread()->BlockingCall(
      [&]() -> RTCErrorOr<rtc::scoped_refptr<SctpDataChannel>> {
        RTC_DCHECK_RUN_ON(network_thread());
        RTCError err = ReserveOrAllocateSid(sid, new_config.fallback_ssl_role);
        if (!err.ok())
          return err;

        // In case `sid` has changed. Update `new_config` accordingly.
        new_config.id = sid.stream_id_int();
        ready_to_send =
            data_channel_transport_ && data_channel_transport_->IsReadyToSend();

        rtc::scoped_refptr<SctpDataChannel> channel(SctpDataChannel::Create(
            std::move(weak_ptr), label, data_channel_transport_ != nullptr,
            new_config, signaling_thread(), network_thread()));
        RTC_DCHECK(channel);
        sctp_data_channels_n_.push_back(channel);

        // If we have an id already, notify the transport.
        if (sid.HasValue())
          AddSctpDataStream(sid);

        return channel;
      });

  if (!ret.ok())
    return ret.MoveError();

  if (ready_to_send) {
    // Checks if the transport is ready to send because the initial channel
    // ready signal may have been sent before the DataChannel creation.
    // This has to be done async because the upper layer objects (e.g.
    // Chrome glue and WebKit) are not wired up properly until after this
    // function returns.
    signaling_thread()->PostTask(
        SafeTask(signaling_safety_.flag(), [channel = ret.value()] {
          if (channel->state() != DataChannelInterface::DataState::kClosed)
            channel->OnTransportReady();
        }));
  }

  sctp_data_channels_.push_back(ret.value());
  has_used_data_channels_ = true;
  return SctpDataChannel::CreateProxy(ret.MoveValue());
}

void DataChannelController::AllocateSctpSids(rtc::SSLRole role) {
  RTC_DCHECK_RUN_ON(signaling_thread());

  std::vector<std::pair<SctpDataChannel*, StreamId>> channels_to_update;
  std::vector<rtc::scoped_refptr<SctpDataChannel>> channels_to_close;

  network_thread()->BlockingCall([&] {
    RTC_DCHECK_RUN_ON(network_thread());
    for (auto it = sctp_data_channels_n_.begin();
         it != sctp_data_channels_n_.end();) {
      if (!(*it)->sid().HasValue()) {
        StreamId sid = sid_allocator_.AllocateSid(role);
        if (sid.HasValue()) {
          AddSctpDataStream(sid);
          channels_to_update.push_back(std::make_pair((*it).get(), sid));
        } else {
          channels_to_close.push_back(std::move(*it));
          it = sctp_data_channels_n_.erase(it);
          continue;
        }
      }
      ++it;
    }
  });

  // Since closing modifies the list of channels, we have to do the actual
  // closing outside the loop.
  for (const auto& channel : channels_to_close) {
    channel->CloseAbruptlyWithDataChannelFailure("Failed to allocate SCTP SID");
    // The channel should now have been removed from sctp_data_channels_.
    RTC_DCHECK(absl::c_find_if(sctp_data_channels_, [&](const auto& c) {
                 return c.get() == channel.get();
               }) == sctp_data_channels_.end());
  }

  for (auto& pair : channels_to_update) {
    auto it = absl::c_find_if(sctp_data_channels_, [&](const auto& c) {
      return c.get() == pair.first;
    });
    RTC_DCHECK(it != sctp_data_channels_.end());
    (*it)->SetSctpSid(pair.second);
  }
}

void DataChannelController::OnSctpDataChannelClosed(SctpDataChannel* channel) {
  RTC_DCHECK_RUN_ON(signaling_thread());

  // TODO(tommi): `sid()` should be called on the network thread.
  // `sid()` and `SctpDataChannel::id_`should have thread guards to enforce
  // correct usage.
  network_thread()->BlockingCall([&, sid = channel->sid()] {
    RTC_DCHECK_RUN_ON(network_thread());
    // After the closing procedure is done, it's safe to use this ID for
    // another data channel.
    if (sid.HasValue())
      sid_allocator_.ReleaseSid(sid);

    auto it = absl::c_find_if(sctp_data_channels_n_, [&](const auto& c) {
      return c.get() == channel;
    });

    if (it != sctp_data_channels_n_.end())
      sctp_data_channels_n_.erase(it);
  });

  for (auto it = sctp_data_channels_.begin(); it != sctp_data_channels_.end();
       ++it) {
    if (it->get() == channel) {
      // Since this method is triggered by a signal from the DataChannel,
      // we can't free it directly here; we need to free it asynchronously.
      rtc::scoped_refptr<SctpDataChannel> release = std::move(*it);
      sctp_data_channels_.erase(it);
      signaling_thread()->PostTask(SafeTask(signaling_safety_.flag(),
                                            [release = std::move(release)] {}));
      return;
    }
  }
}

void DataChannelController::OnTransportChannelClosed(RTCError error) {
  RTC_DCHECK_RUN_ON(signaling_thread());
  // Use a temporary copy of the SCTP DataChannel list because the
  // DataChannel may callback to us and try to modify the list.
  // TODO(tommi): `OnTransportChannelClosed` is called from
  // `SdpOfferAnswerHandler::DestroyDataChannelTransport` just before
  // `TeardownDataChannelTransport_n` is called (but on the network thread) from
  // the same function. Once `sctp_data_channels_` moves to the network thread,
  // we can get rid of this function (OnTransportChannelClosed) and run this
  // loop from within the TeardownDataChannelTransport_n callback.
  std::vector<rtc::scoped_refptr<SctpDataChannel>> temp_sctp_dcs;
  temp_sctp_dcs.swap(sctp_data_channels_);
  for (const auto& channel : temp_sctp_dcs) {
    channel->OnTransportChannelClosed(error);
  }
}

DataChannelTransportInterface* DataChannelController::data_channel_transport()
    const {
  // TODO(bugs.webrtc.org/11547): Only allow this accessor to be called on the
  // network thread.
  // RTC_DCHECK_RUN_ON(network_thread());
  return data_channel_transport_;
}

void DataChannelController::set_data_channel_transport(
    DataChannelTransportInterface* transport) {
  RTC_DCHECK_RUN_ON(network_thread());
  data_channel_transport_ = transport;
}

RTCError DataChannelController::DataChannelSendData(
    StreamId sid,
    const SendDataParams& params,
    const rtc::CopyOnWriteBuffer& payload) {
  // TODO(bugs.webrtc.org/11547): Expect method to be called on the network
  // thread instead. Remove the BlockingCall() below and move associated state
  // to the network thread.
  RTC_DCHECK_RUN_ON(signaling_thread());
  RTC_DCHECK(data_channel_transport());

  return network_thread()->BlockingCall([this, sid, params, payload] {
    return data_channel_transport()->SendData(sid.stream_id_int(), params,
                                              payload);
  });
}

void DataChannelController::NotifyDataChannelsOfTransportCreated() {
  RTC_DCHECK_RUN_ON(network_thread());
  RTC_DCHECK(data_channel_transport());

  // TODO(tommi): Move the blocking call to `AddSctpDataStream` from
  // `SctpDataChannel::OnTransportChannelCreated` to here and be consistent
  // with other call sites to `AddSctpDataStream`. We're already
  // on the right (network) thread here.

  signaling_thread()->PostTask(SafeTask(signaling_safety_.flag(), [this] {
    RTC_DCHECK_RUN_ON(signaling_thread());
    auto copy = sctp_data_channels_;
    for (const auto& channel : copy) {
      channel->OnTransportChannelCreated();
    }
  }));
}

std::vector<rtc::scoped_refptr<SctpDataChannel>>::iterator
DataChannelController::FindChannel(StreamId stream_id) {
  RTC_DCHECK_RUN_ON(signaling_thread());
  return absl::c_find_if(sctp_data_channels_,
                         [&](const auto& c) { return c->sid() == stream_id; });
}

rtc::Thread* DataChannelController::network_thread() const {
  return pc_->network_thread();
}
rtc::Thread* DataChannelController::signaling_thread() const {
  return pc_->signaling_thread();
}

}  // namespace webrtc

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

DataChannelController::~DataChannelController() {}

bool DataChannelController::HasDataChannels() const {
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
  if (data_channel_transport()) {
    network_thread()->BlockingCall([this, sid] {
      if (data_channel_transport()) {
        data_channel_transport()->OpenChannel(sid.stream_id_int());
      }
    });
  }
}

void DataChannelController::RemoveSctpDataStream(StreamId sid) {
  if (data_channel_transport()) {
    network_thread()->BlockingCall([this, sid] {
      if (data_channel_transport()) {
        data_channel_transport()->CloseChannel(sid.stream_id_int());
      }
    });
  }
}

bool DataChannelController::ReadyToSendData() const {
  RTC_DCHECK_RUN_ON(signaling_thread());
  return (data_channel_transport() && data_channel_transport_ready_to_send_);
}

void DataChannelController::OnChannelStateChanged(
    SctpDataChannel* channel,
    DataChannelInterface::DataState state) {
  RTC_DCHECK_RUN_ON(signaling_thread());
  if (state == DataChannelInterface::DataState::kClosed)
    OnSctpDataChannelClosed(channel);

  pc_->OnSctpDataChannelStateChanged(channel, state);
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
  signaling_thread()->PostTask(
      SafeTask(signaling_safety_.flag(), [this, channel_id] {
        RTC_DCHECK_RUN_ON(signaling_thread());
        auto it = FindChannel(StreamId(channel_id));
        // Remove the channel from our list, close it and free up resources.
        if (it != sctp_data_channels_.end()) {
          rtc::scoped_refptr<SctpDataChannel> channel = std::move(*it);
          // Note: this causes OnSctpDataChannelClosed() to not do anything
          // when called from within `OnClosingProcedureComplete`.
          sctp_data_channels_.erase(it);
          sid_allocator_.ReleaseSid(channel->sid());

          channel->OnClosingProcedureComplete();
        }
      }));
}

void DataChannelController::OnReadyToSend() {
  RTC_DCHECK_RUN_ON(network_thread());
  signaling_thread()->PostTask(SafeTask(signaling_safety_.flag(), [this] {
    RTC_DCHECK_RUN_ON(signaling_thread());
    data_channel_transport_ready_to_send_ = true;
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
  rtc::scoped_refptr<DataChannelInterface> channel(
      InternalCreateDataChannelWithProxy(label, &config));
  if (!channel.get()) {
    RTC_LOG(LS_ERROR) << "Failed to create DataChannel from the OPEN message.";
    return;
  }

  pc_->Observer()->OnDataChannel(std::move(channel));
  pc_->NoteDataAddedEvent();
}

rtc::scoped_refptr<DataChannelInterface>
DataChannelController::InternalCreateDataChannelWithProxy(
    const std::string& label,
    const InternalDataChannelInit* config) {
  RTC_DCHECK_RUN_ON(signaling_thread());
  if (pc_->IsClosed()) {
    return nullptr;
  }

  rtc::scoped_refptr<SctpDataChannel> channel =
      InternalCreateSctpDataChannel(label, config);
  if (channel) {
    return SctpDataChannel::CreateProxy(channel);
  }

  return nullptr;
}

rtc::scoped_refptr<SctpDataChannel>
DataChannelController::InternalCreateSctpDataChannel(
    const std::string& label,
    const InternalDataChannelInit* config) {
  RTC_DCHECK_RUN_ON(signaling_thread());
  if (config && !config->IsValid()) {
    RTC_LOG(LS_ERROR) << "Failed to initialize the SCTP data channel due to "
                         "invalid DataChannelInit.";
    return nullptr;
  }

  InternalDataChannelInit new_config =
      config ? (*config) : InternalDataChannelInit();
  StreamId sid(new_config.id);
  if (!sid.HasValue()) {
    rtc::SSLRole role;
    // TODO(bugs.webrtc.org/11547): `GetSctpSslRole` likely involves a hop to
    // the network thread. (unless there's no transport). Change this so that
    // the role is checked on the network thread and any network thread related
    // initialization is done at the same time (to avoid additional hops).
    // Use `GetSctpSslRole_n` on the network thread.
    if (pc_->GetSctpSslRole(&role)) {
      sid = sid_allocator_.AllocateSid(role);
      if (!sid.HasValue())
        return nullptr;
    }
    // Note that when we get here, the ID may still be invalid.
  } else if (!sid_allocator_.ReserveSid(sid)) {
    RTC_LOG(LS_ERROR) << "Failed to create a SCTP data channel "
                         "because the id is already in use or out of range.";
    return nullptr;
  }
  // In case `sid` has changed. Update `new_config` accordingly.
  new_config.id = sid.stream_id_int();
  // TODO(bugs.webrtc.org/11547): The `data_channel_transport_` pointer belongs
  // to the network thread but there are a few places where we check this
  // pointer from the signaling thread. Instead of this approach, we should have
  // a separate channel initialization step that runs on the network thread
  // where we inform the channel of information about whether there's a
  // transport or not, what the role is, and supply an id if any. Subsequently
  // all that state in the channel code, is needed for callbacks from the
  // transport which is already initiated from the network thread. Then we can
  // Remove the trampoline code (see e.g. PostTask() calls in this file) that
  // travels between the signaling and network threads.
  rtc::scoped_refptr<SctpDataChannel> channel(SctpDataChannel::Create(
      weak_factory_.GetWeakPtr(), label, data_channel_transport() != nullptr,
      new_config, signaling_thread(), network_thread()));
  RTC_DCHECK(channel);

  if (ReadyToSendData()) {
    // Checks if the transport is ready to send because the initial channel
    // ready signal may have been sent before the DataChannel creation.
    // This has to be done async because the upper layer objects (e.g.
    // Chrome glue and WebKit) are not wired up properly until after this
    // function returns.
    signaling_thread()->PostTask(
        SafeTask(signaling_safety_.flag(), [channel = channel] {
          if (channel->state() != DataChannelInterface::DataState::kClosed)
            channel->OnTransportReady();
        }));
  }

  sctp_data_channels_.push_back(channel);
  has_used_data_channels_ = true;
  return channel;
}

void DataChannelController::AllocateSctpSids(rtc::SSLRole role) {
  RTC_DCHECK_RUN_ON(signaling_thread());
  std::vector<rtc::scoped_refptr<SctpDataChannel>> channels_to_close;
  for (const auto& channel : sctp_data_channels_) {
    if (!channel->sid().HasValue()) {
      StreamId sid = sid_allocator_.AllocateSid(role);
      if (!sid.HasValue()) {
        channels_to_close.push_back(channel);
        continue;
      }
      // TODO(bugs.webrtc.org/11547): This hides a blocking call to the network
      // thread via AddSctpDataStream. Maybe it's better to move the whole loop
      // to the network thread? Maybe even `sctp_data_channels_`?
      channel->SetSctpSid(sid);
    }
  }
  // Since closing modifies the list of channels, we have to do the actual
  // closing outside the loop.
  for (const auto& channel : channels_to_close) {
    channel->CloseAbruptlyWithDataChannelFailure("Failed to allocate SCTP SID");
  }
}

void DataChannelController::OnSctpDataChannelClosed(SctpDataChannel* channel) {
  RTC_DCHECK_RUN_ON(signaling_thread());
  for (auto it = sctp_data_channels_.begin(); it != sctp_data_channels_.end();
       ++it) {
    if (it->get() == channel) {
      if (channel->sid().HasValue()) {
        // After the closing procedure is done, it's safe to use this ID for
        // another data channel.
        sid_allocator_.ReleaseSid(channel->sid());
      }

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

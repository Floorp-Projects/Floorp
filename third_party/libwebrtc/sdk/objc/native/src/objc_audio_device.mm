/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "objc_audio_device.h"

#include "rtc_base/logging.h"

namespace webrtc {
namespace objc_adm {

ObjCAudioDeviceModule::ObjCAudioDeviceModule() {
  RTC_DLOG_F(LS_VERBOSE) << "";
}

ObjCAudioDeviceModule::~ObjCAudioDeviceModule() {
  RTC_DLOG_F(LS_VERBOSE) << "";
}

int32_t ObjCAudioDeviceModule::RegisterAudioCallback(AudioTransport* audioCallback) {
  RTC_DLOG_F(LS_VERBOSE) << "Not yet implemented";
  return -1;
}

int32_t ObjCAudioDeviceModule::Init() {
  RTC_DLOG_F(LS_VERBOSE) << "Not yet implemented";
  return -1;
}

int32_t ObjCAudioDeviceModule::Terminate() {
  RTC_DLOG_F(LS_VERBOSE) << "Not yet implemented";
  return -1;
}

bool ObjCAudioDeviceModule::Initialized() const {
  RTC_DLOG_F(LS_VERBOSE) << "Not yet implemented";
  return false;
}

int32_t ObjCAudioDeviceModule::PlayoutIsAvailable(bool* available) {
  RTC_DLOG_F(LS_VERBOSE) << "Not yet implemented";
  return -1;
}

bool ObjCAudioDeviceModule::PlayoutIsInitialized() const {
  RTC_DLOG_F(LS_VERBOSE) << "Not yet implemented";
  return false;
}

int32_t ObjCAudioDeviceModule::InitPlayout() {
  RTC_DLOG_F(LS_VERBOSE) << "Not yet implemented";
  return -1;
}

bool ObjCAudioDeviceModule::Playing() const {
  RTC_DLOG_F(LS_VERBOSE) << "Not yet implemented";
  return false;
}

int32_t ObjCAudioDeviceModule::StartPlayout() {
  RTC_DLOG_F(LS_VERBOSE) << "Not yet implemented";
  return -1;
}

int32_t ObjCAudioDeviceModule::StopPlayout() {
  RTC_DLOG_F(LS_VERBOSE) << "Not yet implemented";
  return -1;
}

int32_t ObjCAudioDeviceModule::PlayoutDelay(uint16_t* delayMS) const {
  RTC_DLOG_F(LS_VERBOSE) << "Not yet implemented";
  return -1;
}

int32_t ObjCAudioDeviceModule::RecordingIsAvailable(bool* available) {
  RTC_DLOG_F(LS_VERBOSE) << "Not yet implemented";
  return -1;
}

bool ObjCAudioDeviceModule::RecordingIsInitialized() const {
  RTC_DLOG_F(LS_VERBOSE) << "Not yet implemented";
  return false;
}

int32_t ObjCAudioDeviceModule::InitRecording() {
  RTC_DLOG_F(LS_VERBOSE) << "Not yet implemented";
  return -1;
}

bool ObjCAudioDeviceModule::Recording() const {
  RTC_DLOG_F(LS_VERBOSE) << "Not yet implemented";
  return false;
}

int32_t ObjCAudioDeviceModule::StartRecording() {
  RTC_DLOG_F(LS_VERBOSE) << "Not yet implemented";
  return -1;
}

int32_t ObjCAudioDeviceModule::StopRecording() {
  RTC_DLOG_F(LS_VERBOSE) << "Not yet implemented";
  return -1;
}

#if defined(WEBRTC_IOS)

int ObjCAudioDeviceModule::GetPlayoutAudioParameters(AudioParameters* params) const {
  RTC_DLOG_F(LS_VERBOSE) << "Not yet implemented";
  return -1;
}

int ObjCAudioDeviceModule::GetRecordAudioParameters(AudioParameters* params) const {
  RTC_DLOG_F(LS_VERBOSE) << "Not yet implemented";
  return -1;
}

#endif  // WEBRTC_IOS

#pragma mark - Not implemented/Not relevant

int32_t ObjCAudioDeviceModule::ActiveAudioLayer(AudioLayer* audioLayer) const {
  return -1;
}

int16_t ObjCAudioDeviceModule::PlayoutDevices() {
  return 0;
}

int16_t ObjCAudioDeviceModule::RecordingDevices() {
  return 0;
}

int32_t ObjCAudioDeviceModule::PlayoutDeviceName(uint16_t index,
                                                 char name[kAdmMaxDeviceNameSize],
                                                 char guid[kAdmMaxGuidSize]) {
  return -1;
}

int32_t ObjCAudioDeviceModule::RecordingDeviceName(uint16_t index,
                                                   char name[kAdmMaxDeviceNameSize],
                                                   char guid[kAdmMaxGuidSize]) {
  return -1;
}

int32_t ObjCAudioDeviceModule::SetPlayoutDevice(uint16_t index) {
  return 0;
}

int32_t ObjCAudioDeviceModule::SetPlayoutDevice(WindowsDeviceType device) {
  return -1;
}

int32_t ObjCAudioDeviceModule::SetRecordingDevice(uint16_t index) {
  return 0;
}

int32_t ObjCAudioDeviceModule::SetRecordingDevice(WindowsDeviceType device) {
  return -1;
}

int32_t ObjCAudioDeviceModule::InitSpeaker() {
  return 0;
}

bool ObjCAudioDeviceModule::SpeakerIsInitialized() const {
  return true;
}

int32_t ObjCAudioDeviceModule::InitMicrophone() {
  return 0;
}

bool ObjCAudioDeviceModule::MicrophoneIsInitialized() const {
  return true;
}

int32_t ObjCAudioDeviceModule::SpeakerVolumeIsAvailable(bool* available) {
  *available = false;
  return 0;
}

int32_t ObjCAudioDeviceModule::SetSpeakerVolume(uint32_t volume) {
  return -1;
}

int32_t ObjCAudioDeviceModule::SpeakerVolume(uint32_t* volume) const {
  return -1;
}

int32_t ObjCAudioDeviceModule::MaxSpeakerVolume(uint32_t* maxVolume) const {
  return -1;
}

int32_t ObjCAudioDeviceModule::MinSpeakerVolume(uint32_t* minVolume) const {
  return -1;
}

int32_t ObjCAudioDeviceModule::SpeakerMuteIsAvailable(bool* available) {
  *available = false;
  return 0;
}

int32_t ObjCAudioDeviceModule::SetSpeakerMute(bool enable) {
  return -1;
}

int32_t ObjCAudioDeviceModule::SpeakerMute(bool* enabled) const {
  return -1;
}

int32_t ObjCAudioDeviceModule::MicrophoneMuteIsAvailable(bool* available) {
  *available = false;
  return 0;
}

int32_t ObjCAudioDeviceModule::SetMicrophoneMute(bool enable) {
  return -1;
}

int32_t ObjCAudioDeviceModule::MicrophoneMute(bool* enabled) const {
  return -1;
}

int32_t ObjCAudioDeviceModule::MicrophoneVolumeIsAvailable(bool* available) {
  *available = false;
  return 0;
}

int32_t ObjCAudioDeviceModule::SetMicrophoneVolume(uint32_t volume) {
  return -1;
}

int32_t ObjCAudioDeviceModule::MicrophoneVolume(uint32_t* volume) const {
  return -1;
}

int32_t ObjCAudioDeviceModule::MaxMicrophoneVolume(uint32_t* maxVolume) const {
  return -1;
}

int32_t ObjCAudioDeviceModule::MinMicrophoneVolume(uint32_t* minVolume) const {
  return -1;
}

int32_t ObjCAudioDeviceModule::StereoPlayoutIsAvailable(bool* available) const {
  *available = false;
  return 0;
}

int32_t ObjCAudioDeviceModule::SetStereoPlayout(bool enable) {
  return -1;
}

int32_t ObjCAudioDeviceModule::StereoPlayout(bool* enabled) const {
  *enabled = false;
  return 0;
}

int32_t ObjCAudioDeviceModule::StereoRecordingIsAvailable(bool* available) const {
  *available = false;
  return 0;
}

int32_t ObjCAudioDeviceModule::SetStereoRecording(bool enable) {
  return -1;
}

int32_t ObjCAudioDeviceModule::StereoRecording(bool* enabled) const {
  *enabled = false;
  return 0;
}

bool ObjCAudioDeviceModule::BuiltInAECIsAvailable() const {
  return false;
}

int32_t ObjCAudioDeviceModule::EnableBuiltInAEC(bool enable) {
  return 0;
}

bool ObjCAudioDeviceModule::BuiltInAGCIsAvailable() const {
  return false;
}

int32_t ObjCAudioDeviceModule::EnableBuiltInAGC(bool enable) {
  return 0;
}

bool ObjCAudioDeviceModule::BuiltInNSIsAvailable() const {
  return false;
}

int32_t ObjCAudioDeviceModule::EnableBuiltInNS(bool enable) {
  return 0;
}

int32_t ObjCAudioDeviceModule::GetPlayoutUnderrunCount() const {
  return -1;
}

}  // namespace objc_adm

}  // namespace webrtc
/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#import <AVFoundation/AVFoundation.h>
#import <Foundation/Foundation.h>

#include "webrtc/modules/audio_device/ios/audio_device_ios.h"

#include "webrtc/base/atomicops.h"
#include "webrtc/base/checks.h"
#include "webrtc/base/criticalsection.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/thread_annotations.h"
#include "webrtc/modules/audio_device/fine_audio_buffer.h"
#include "webrtc/modules/utility/include/helpers_ios.h"

namespace webrtc {

// Protects |g_audio_session_users|.
static rtc::GlobalLockPod g_lock;

// Counts number of users (=instances of this object) who needs an active
// audio session. This variable is used to ensure that we only activate an audio
// session for the first user and deactivate it for the last.
// Member is static to ensure that the value is counted for all instances
// and not per instance.
static int g_audio_session_users GUARDED_BY(g_lock) = 0;

#define LOGI() LOG(LS_INFO) << "AudioDeviceIOS::"

#define LOG_AND_RETURN_IF_ERROR(error, message) \
  do {                                          \
    OSStatus err = error;                       \
    if (err) {                                  \
      LOG(LS_ERROR) << message << ": " << err;  \
      return false;                             \
    }                                           \
  } while (0)

#define LOG_IF_ERROR(error, message)           \
  do {                                         \
    OSStatus err = error;                      \
    if (err) {                                 \
      LOG(LS_ERROR) << message << ": " << err; \
    }                                          \
  } while (0)

// Preferred hardware sample rate (unit is in Hertz). The client sample rate
// will be set to this value as well to avoid resampling the the audio unit's
// format converter. Note that, some devices, e.g. BT headsets, only supports
// 8000Hz as native sample rate.
const double kPreferredSampleRate = 48000.0;
// Use a hardware I/O buffer size (unit is in seconds) that matches the 10ms
// size used by WebRTC. The exact actual size will differ between devices.
// Example: using 48kHz on iPhone 6 results in a native buffer size of
// ~10.6667ms or 512 audio frames per buffer. The FineAudioBuffer instance will
// take care of any buffering required to convert between native buffers and
// buffers used by WebRTC. It is beneficial for the performance if the native
// size is as close to 10ms as possible since it results in "clean" callback
// sequence without bursts of callbacks back to back.
const double kPreferredIOBufferDuration = 0.01;
// Try to use mono to save resources. Also avoids channel format conversion
// in the I/O audio unit. Initial tests have shown that it is possible to use
// mono natively for built-in microphones and for BT headsets but not for
// wired headsets. Wired headsets only support stereo as native channel format
// but it is a low cost operation to do a format conversion to mono in the
// audio unit. Hence, we will not hit a RTC_CHECK in
// VerifyAudioParametersForActiveAudioSession() for a mismatch between the
// preferred number of channels and the actual number of channels.
const int kPreferredNumberOfChannels = 1;
// Number of bytes per audio sample for 16-bit signed integer representation.
const UInt32 kBytesPerSample = 2;
// Hardcoded delay estimates based on real measurements.
// TODO(henrika): these value is not used in combination with built-in AEC.
// Can most likely be removed.
const UInt16 kFixedPlayoutDelayEstimate = 30;
const UInt16 kFixedRecordDelayEstimate = 30;
// Calls to AudioUnitInitialize() can fail if called back-to-back on different
// ADM instances. A fall-back solution is to allow multiple sequential calls
// with as small delay between each. This factor sets the max number of allowed
// initialization attempts.
const int kMaxNumberOfAudioUnitInitializeAttempts = 5;


using ios::CheckAndLogError;

// Verifies that the current audio session supports input audio and that the
// required category and mode are enabled.
static bool VerifyAudioSession(AVAudioSession* session) {
  LOG(LS_INFO) << "VerifyAudioSession";
  // Ensure that the device currently supports audio input.
  if (!session.isInputAvailable) {
    LOG(LS_ERROR) << "No audio input path is available!";
    return false;
  }

  // Ensure that the required category and mode are actually activated.
  if (![session.category isEqualToString:AVAudioSessionCategoryPlayAndRecord]) {
    LOG(LS_ERROR)
        << "Failed to set category to AVAudioSessionCategoryPlayAndRecord";
    return false;
  }
  if (![session.mode isEqualToString:AVAudioSessionModeVoiceChat]) {
    LOG(LS_ERROR) << "Failed to set mode to AVAudioSessionModeVoiceChat";
    return false;
  }
  return true;
}

// Activates an audio session suitable for full duplex VoIP sessions when
// |activate| is true. Also sets the preferred sample rate and IO buffer
// duration. Deactivates an active audio session if |activate| is set to false.
static bool ActivateAudioSession(AVAudioSession* session, bool activate)
    EXCLUSIVE_LOCKS_REQUIRED(g_lock) {
  LOG(LS_INFO) << "ActivateAudioSession(" << activate << ")";
  @autoreleasepool {
    NSError* error = nil;
    BOOL success = NO;

    if (!activate) {
      // Deactivate the audio session using an extra option and then return.
      // AVAudioSessionSetActiveOptionNotifyOthersOnDeactivation is used to
      // ensure that other audio sessions that were interrupted by our session
      // can return to their active state. It is recommended for VoIP apps to
      // use this option.
      success = [session
            setActive:NO
          withOptions:AVAudioSessionSetActiveOptionNotifyOthersOnDeactivation
                error:&error];
      return CheckAndLogError(success, error);
    }

    // Go ahead and active our own audio session since |activate| is true.
    // Use a category which supports simultaneous recording and playback.
    // By default, using this category implies that our app’s audio is
    // nonmixable, hence activating the session will interrupt any other
    // audio sessions which are also nonmixable.
    if (session.category != AVAudioSessionCategoryPlayAndRecord) {
      error = nil;
      success = [session setCategory:AVAudioSessionCategoryPlayAndRecord
                         withOptions:AVAudioSessionCategoryOptionAllowBluetooth
                               error:&error];
      RTC_DCHECK(CheckAndLogError(success, error));
    }

    // Specify mode for two-way voice communication (e.g. VoIP).
    if (session.mode != AVAudioSessionModeVoiceChat) {
      error = nil;
      success = [session setMode:AVAudioSessionModeVoiceChat error:&error];
      RTC_DCHECK(CheckAndLogError(success, error));
    }

    // Set the session's sample rate or the hardware sample rate.
    // It is essential that we use the same sample rate as stream format
    // to ensure that the I/O unit does not have to do sample rate conversion.
    error = nil;
    success =
        [session setPreferredSampleRate:kPreferredSampleRate error:&error];
    RTC_DCHECK(CheckAndLogError(success, error));

    // Set the preferred audio I/O buffer duration, in seconds.
    error = nil;
    success = [session setPreferredIOBufferDuration:kPreferredIOBufferDuration
                                              error:&error];
    RTC_DCHECK(CheckAndLogError(success, error));

    // Activate the audio session. Activation can fail if another active audio
    // session (e.g. phone call) has higher priority than ours.
    error = nil;
    success = [session setActive:YES error:&error];
    if (!CheckAndLogError(success, error)) {
      return false;
    }

    // Ensure that the active audio session has the correct category and mode.
    if (!VerifyAudioSession(session)) {
      LOG(LS_ERROR) << "Failed to verify audio session category and mode";
      return false;
    }

    // Try to set the preferred number of hardware audio channels. These calls
    // must be done after setting the audio session’s category and mode and
    // activating the session.
    // We try to use mono in both directions to save resources and format
    // conversions in the audio unit. Some devices does only support stereo;
    // e.g. wired headset on iPhone 6.
    // TODO(henrika): add support for stereo if needed.
    error = nil;
    success =
        [session setPreferredInputNumberOfChannels:kPreferredNumberOfChannels
                                             error:&error];
    RTC_DCHECK(CheckAndLogError(success, error));
    error = nil;
    success =
        [session setPreferredOutputNumberOfChannels:kPreferredNumberOfChannels
                                              error:&error];
    RTC_DCHECK(CheckAndLogError(success, error));
    return true;
  }
}

// An application can create more than one ADM and start audio streaming
// for all of them. It is essential that we only activate the app's audio
// session once (for the first one) and deactivate it once (for the last).
static bool ActivateAudioSession() {
  LOGI() << "ActivateAudioSession";
  rtc::GlobalLockScope ls(&g_lock);
  if (g_audio_session_users == 0) {
    // The system provides an audio session object upon launch of an
    // application. However, we must initialize the session in order to
    // handle interruptions. Implicit initialization occurs when obtaining
    // a reference to the AVAudioSession object.
    AVAudioSession* session = [AVAudioSession sharedInstance];
    // Try to activate the audio session and ask for a set of preferred audio
    // parameters.
    if (!ActivateAudioSession(session, true)) {
      LOG(LS_ERROR) << "Failed to activate the audio session";
      return false;
    }
    LOG(LS_INFO) << "The audio session is now activated";
  }
  ++g_audio_session_users;
  LOG(LS_INFO) << "Number of audio session users: " << g_audio_session_users;
  return true;
}

// If more than one object is using the audio session, ensure that only the
// last object deactivates. Apple recommends: "activate your audio session
// only as needed and deactivate it when you are not using audio".
static bool DeactivateAudioSession() {
  LOGI() << "DeactivateAudioSession";
  rtc::GlobalLockScope ls(&g_lock);
  if (g_audio_session_users == 1) {
    AVAudioSession* session = [AVAudioSession sharedInstance];
    if (!ActivateAudioSession(session, false)) {
      LOG(LS_ERROR) << "Failed to deactivate the audio session";
      return false;
    }
    LOG(LS_INFO) << "Our audio session is now deactivated";
  }
  --g_audio_session_users;
  LOG(LS_INFO) << "Number of audio session users: " << g_audio_session_users;
  return true;
}

#if !defined(NDEBUG)
// Helper method for printing out an AudioStreamBasicDescription structure.
static void LogABSD(AudioStreamBasicDescription absd) {
  char formatIDString[5];
  UInt32 formatID = CFSwapInt32HostToBig(absd.mFormatID);
  bcopy(&formatID, formatIDString, 4);
  formatIDString[4] = '\0';
  LOG(LS_INFO) << "LogABSD";
  LOG(LS_INFO) << " sample rate: " << absd.mSampleRate;
  LOG(LS_INFO) << " format ID: " << formatIDString;
  LOG(LS_INFO) << " format flags: " << std::hex << absd.mFormatFlags;
  LOG(LS_INFO) << " bytes per packet: " << absd.mBytesPerPacket;
  LOG(LS_INFO) << " frames per packet: " << absd.mFramesPerPacket;
  LOG(LS_INFO) << " bytes per frame: " << absd.mBytesPerFrame;
  LOG(LS_INFO) << " channels per packet: " << absd.mChannelsPerFrame;
  LOG(LS_INFO) << " bits per channel: " << absd.mBitsPerChannel;
  LOG(LS_INFO) << " reserved: " << absd.mReserved;
}

// Helper method that logs essential device information strings.
static void LogDeviceInfo() {
  LOG(LS_INFO) << "LogDeviceInfo";
  @autoreleasepool {
    LOG(LS_INFO) << " system name: " << ios::GetSystemName();
    LOG(LS_INFO) << " system version: " << ios::GetSystemVersion();
    LOG(LS_INFO) << " device type: " << ios::GetDeviceType();
    LOG(LS_INFO) << " device name: " << ios::GetDeviceName();
  }
}
#endif  // !defined(NDEBUG)

AudioDeviceIOS::AudioDeviceIOS()
    : audio_device_buffer_(nullptr),
      vpio_unit_(nullptr),
      recording_(0),
      playing_(0),
      initialized_(false),
      rec_is_initialized_(false),
      play_is_initialized_(false),
      audio_interruption_observer_(nullptr),
      route_change_observer_(nullptr) {
  LOGI() << "ctor" << ios::GetCurrentThreadDescription();
}

AudioDeviceIOS::~AudioDeviceIOS() {
  LOGI() << "~dtor" << ios::GetCurrentThreadDescription();
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  Terminate();
}

void AudioDeviceIOS::AttachAudioBuffer(AudioDeviceBuffer* audioBuffer) {
  LOGI() << "AttachAudioBuffer";
  RTC_DCHECK(audioBuffer);
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  audio_device_buffer_ = audioBuffer;
}

int32_t AudioDeviceIOS::Init() {
  LOGI() << "Init";
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  if (initialized_) {
    return 0;
  }
#if !defined(NDEBUG)
  LogDeviceInfo();
#endif
  // Store the preferred sample rate and preferred number of channels already
  // here. They have not been set and confirmed yet since ActivateAudioSession()
  // is not called until audio is about to start. However, it makes sense to
  // store the parameters now and then verify at a later stage.
  playout_parameters_.reset(kPreferredSampleRate, kPreferredNumberOfChannels);
  record_parameters_.reset(kPreferredSampleRate, kPreferredNumberOfChannels);
  // Ensure that the audio device buffer (ADB) knows about the internal audio
  // parameters. Note that, even if we are unable to get a mono audio session,
  // we will always tell the I/O audio unit to do a channel format conversion
  // to guarantee mono on the "input side" of the audio unit.
  UpdateAudioDeviceBuffer();
  initialized_ = true;
  return 0;
}

int32_t AudioDeviceIOS::Terminate() {
  LOGI() << "Terminate";
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  if (!initialized_) {
    return 0;
  }
  StopPlayout();
  StopRecording();
  initialized_ = false;
  {
    rtc::GlobalLockScope ls(&g_lock);
    if (g_audio_session_users != 0) {
      LOG(LS_WARNING) << "Object is destructed with an active audio session";
    }
    RTC_DCHECK_GE(g_audio_session_users, 0);
  }
  return 0;
}

int32_t AudioDeviceIOS::InitPlayout() {
  LOGI() << "InitPlayout";
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  RTC_DCHECK(initialized_);
  RTC_DCHECK(!play_is_initialized_);
  RTC_DCHECK(!playing_);
  if (!rec_is_initialized_) {
    if (!InitPlayOrRecord()) {
      LOG_F(LS_ERROR) << "InitPlayOrRecord failed for InitPlayout!";
      return -1;
    }
  }
  play_is_initialized_ = true;
  return 0;
}

int32_t AudioDeviceIOS::InitRecording() {
  LOGI() << "InitRecording";
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  RTC_DCHECK(initialized_);
  RTC_DCHECK(!rec_is_initialized_);
  RTC_DCHECK(!recording_);
  if (!play_is_initialized_) {
    if (!InitPlayOrRecord()) {
      LOG_F(LS_ERROR) << "InitPlayOrRecord failed for InitRecording!";
      return -1;
    }
  }
  rec_is_initialized_ = true;
  return 0;
}

int32_t AudioDeviceIOS::StartPlayout() {
  LOGI() << "StartPlayout";
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  RTC_DCHECK(play_is_initialized_);
  RTC_DCHECK(!playing_);
  fine_audio_buffer_->ResetPlayout();
  if (!recording_) {
    OSStatus result = AudioOutputUnitStart(vpio_unit_);
    if (result != noErr) {
      LOG_F(LS_ERROR) << "AudioOutputUnitStart failed for StartPlayout: "
                      << result;
      return -1;
    }
    LOG(LS_INFO) << "Voice-Processing I/O audio unit is now started";
  }
  rtc::AtomicOps::ReleaseStore(&playing_, 1);
  return 0;
}

int32_t AudioDeviceIOS::StopPlayout() {
  LOGI() << "StopPlayout";
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  if (!play_is_initialized_ || !playing_) {
    return 0;
  }
  if (!recording_) {
    ShutdownPlayOrRecord();
  }
  play_is_initialized_ = false;
  rtc::AtomicOps::ReleaseStore(&playing_, 0);
  return 0;
}

int32_t AudioDeviceIOS::StartRecording() {
  LOGI() << "StartRecording";
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  RTC_DCHECK(rec_is_initialized_);
  RTC_DCHECK(!recording_);
  fine_audio_buffer_->ResetRecord();
  if (!playing_) {
    OSStatus result = AudioOutputUnitStart(vpio_unit_);
    if (result != noErr) {
      LOG_F(LS_ERROR) << "AudioOutputUnitStart failed for StartRecording: "
                      << result;
      return -1;
    }
    LOG(LS_INFO) << "Voice-Processing I/O audio unit is now started";
  }
  rtc::AtomicOps::ReleaseStore(&recording_, 1);
  return 0;
}

int32_t AudioDeviceIOS::StopRecording() {
  LOGI() << "StopRecording";
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  if (!rec_is_initialized_ || !recording_) {
    return 0;
  }
  if (!playing_) {
    ShutdownPlayOrRecord();
  }
  rec_is_initialized_ = false;
  rtc::AtomicOps::ReleaseStore(&recording_, 0);
  return 0;
}

// Change the default receiver playout route to speaker.
int32_t AudioDeviceIOS::SetLoudspeakerStatus(bool enable) {
  LOGI() << "SetLoudspeakerStatus(" << enable << ")";

  AVAudioSession* session = [AVAudioSession sharedInstance];
  NSString* category = session.category;
  AVAudioSessionCategoryOptions options = session.categoryOptions;
  // Respect old category options if category is
  // AVAudioSessionCategoryPlayAndRecord. Otherwise reset it since old options
  // might not be valid for this category.
  if ([category isEqualToString:AVAudioSessionCategoryPlayAndRecord]) {
    if (enable) {
      options |= AVAudioSessionCategoryOptionDefaultToSpeaker;
    } else {
      options &= ~AVAudioSessionCategoryOptionDefaultToSpeaker;
    }
  } else {
    options = AVAudioSessionCategoryOptionDefaultToSpeaker;
  }
  NSError* error = nil;
  BOOL success = [session setCategory:AVAudioSessionCategoryPlayAndRecord
                          withOptions:options
                                error:&error];
  ios::CheckAndLogError(success, error);
  return (error == nil) ? 0 : -1;
}

int32_t AudioDeviceIOS::GetLoudspeakerStatus(bool& enabled) const {
  LOGI() << "GetLoudspeakerStatus";
  AVAudioSession* session = [AVAudioSession sharedInstance];
  AVAudioSessionCategoryOptions options = session.categoryOptions;
  enabled = options & AVAudioSessionCategoryOptionDefaultToSpeaker;
  return 0;
}

int32_t AudioDeviceIOS::PlayoutDelay(uint16_t& delayMS) const {
  delayMS = kFixedPlayoutDelayEstimate;
  return 0;
}

int32_t AudioDeviceIOS::RecordingDelay(uint16_t& delayMS) const {
  delayMS = kFixedRecordDelayEstimate;
  return 0;
}

int AudioDeviceIOS::GetPlayoutAudioParameters(AudioParameters* params) const {
  LOGI() << "GetPlayoutAudioParameters";
  RTC_DCHECK(playout_parameters_.is_valid());
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  *params = playout_parameters_;
  return 0;
}

int AudioDeviceIOS::GetRecordAudioParameters(AudioParameters* params) const {
  LOGI() << "GetRecordAudioParameters";
  RTC_DCHECK(record_parameters_.is_valid());
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  *params = record_parameters_;
  return 0;
}

void AudioDeviceIOS::UpdateAudioDeviceBuffer() {
  LOGI() << "UpdateAudioDevicebuffer";
  // AttachAudioBuffer() is called at construction by the main class but check
  // just in case.
  RTC_DCHECK(audio_device_buffer_) << "AttachAudioBuffer must be called first";
  // Inform the audio device buffer (ADB) about the new audio format.
  audio_device_buffer_->SetPlayoutSampleRate(playout_parameters_.sample_rate());
  audio_device_buffer_->SetPlayoutChannels(playout_parameters_.channels());
  audio_device_buffer_->SetRecordingSampleRate(
      record_parameters_.sample_rate());
  audio_device_buffer_->SetRecordingChannels(record_parameters_.channels());
}

void AudioDeviceIOS::RegisterNotificationObservers() {
  LOGI() << "RegisterNotificationObservers";
  // This code block will be called when AVAudioSessionInterruptionNotification
  // is observed.
  void (^interrupt_block)(NSNotification*) = ^(NSNotification* notification) {
    NSNumber* type_number =
        notification.userInfo[AVAudioSessionInterruptionTypeKey];
    AVAudioSessionInterruptionType type =
        (AVAudioSessionInterruptionType)type_number.unsignedIntegerValue;
    LOG(LS_INFO) << "Audio session interruption:";
    switch (type) {
      case AVAudioSessionInterruptionTypeBegan:
        // The system has deactivated our audio session.
        // Stop the active audio unit.
        LOG(LS_INFO) << " Began => stopping the audio unit";
        LOG_IF_ERROR(AudioOutputUnitStop(vpio_unit_),
                     "Failed to stop the the Voice-Processing I/O unit");
        break;
      case AVAudioSessionInterruptionTypeEnded:
        // The interruption has ended. Restart the audio session and start the
        // initialized audio unit again.
        LOG(LS_INFO) << " Ended => restarting audio session and audio unit";
        NSError* error = nil;
        BOOL success = NO;
        AVAudioSession* session = [AVAudioSession sharedInstance];
        success = [session setActive:YES error:&error];
        if (CheckAndLogError(success, error)) {
          LOG_IF_ERROR(AudioOutputUnitStart(vpio_unit_),
                       "Failed to start the the Voice-Processing I/O unit");
        }
        break;
    }
  };

  // This code block will be called when AVAudioSessionRouteChangeNotification
  // is observed.
  void (^route_change_block)(NSNotification*) =
      ^(NSNotification* notification) {
        // Get reason for current route change.
        NSNumber* reason_number =
            notification.userInfo[AVAudioSessionRouteChangeReasonKey];
        AVAudioSessionRouteChangeReason reason =
            (AVAudioSessionRouteChangeReason)reason_number.unsignedIntegerValue;
        bool valid_route_change = true;
        LOG(LS_INFO) << "Route change:";
        switch (reason) {
          case AVAudioSessionRouteChangeReasonUnknown:
            LOG(LS_INFO) << " ReasonUnknown";
            break;
          case AVAudioSessionRouteChangeReasonNewDeviceAvailable:
            LOG(LS_INFO) << " NewDeviceAvailable";
            break;
          case AVAudioSessionRouteChangeReasonOldDeviceUnavailable:
            LOG(LS_INFO) << " OldDeviceUnavailable";
            break;
          case AVAudioSessionRouteChangeReasonCategoryChange:
            // It turns out that we see this notification (at least in iOS 9.2)
            // when making a switch from a BT device to e.g. Speaker using the
            // iOS Control Center and that we therefore must check if the sample
            // rate has changed. And if so is the case, restart the audio unit.
            LOG(LS_INFO) << " CategoryChange";
            LOG(LS_INFO) << " New category: " << ios::GetAudioSessionCategory();
            break;
          case AVAudioSessionRouteChangeReasonOverride:
            LOG(LS_INFO) << " Override";
            break;
          case AVAudioSessionRouteChangeReasonWakeFromSleep:
            LOG(LS_INFO) << " WakeFromSleep";
            break;
          case AVAudioSessionRouteChangeReasonNoSuitableRouteForCategory:
            LOG(LS_INFO) << " NoSuitableRouteForCategory";
            break;
          case AVAudioSessionRouteChangeReasonRouteConfigurationChange:
            // The set of input and output ports has not changed, but their
            // configuration has, e.g., a port’s selected data source has
            // changed. Ignore this type of route change since we are focusing
            // on detecting headset changes.
            LOG(LS_INFO) << " RouteConfigurationChange (ignored)";
            valid_route_change = false;
            break;
        }

        if (valid_route_change) {
          // Log previous route configuration.
          AVAudioSessionRouteDescription* prev_route =
              notification.userInfo[AVAudioSessionRouteChangePreviousRouteKey];
          LOG(LS_INFO) << "Previous route:";
          LOG(LS_INFO) << ios::StdStringFromNSString(
              [NSString stringWithFormat:@"%@", prev_route]);

          // Only restart audio for a valid route change and if the
          // session sample rate has changed.
          AVAudioSession* session = [AVAudioSession sharedInstance];
          const double session_sample_rate = session.sampleRate;
          LOG(LS_INFO) << "session sample rate: " << session_sample_rate;
          if (playout_parameters_.sample_rate() != session_sample_rate) {
            if (!RestartAudioUnitWithNewFormat(session_sample_rate)) {
              LOG(LS_ERROR) << "Audio restart failed";
            }
          }
        }
      };

  // Get the default notification center of the current process.
  NSNotificationCenter* center = [NSNotificationCenter defaultCenter];

  // Add AVAudioSessionInterruptionNotification observer.
  id interruption_observer =
      [center addObserverForName:AVAudioSessionInterruptionNotification
                          object:nil
                           queue:[NSOperationQueue mainQueue]
                      usingBlock:interrupt_block];
  // Add AVAudioSessionRouteChangeNotification observer.
  id route_change_observer =
      [center addObserverForName:AVAudioSessionRouteChangeNotification
                          object:nil
                           queue:[NSOperationQueue mainQueue]
                      usingBlock:route_change_block];

  // Increment refcount on observers using ARC bridge. Instance variable is a
  // void* instead of an id because header is included in other pure C++
  // files.
  audio_interruption_observer_ = (__bridge_retained void*)interruption_observer;
  route_change_observer_ = (__bridge_retained void*)route_change_observer;
}

void AudioDeviceIOS::UnregisterNotificationObservers() {
  LOGI() << "UnregisterNotificationObservers";
  // Transfer ownership of observer back to ARC, which will deallocate the
  // observer once it exits this scope.
  NSNotificationCenter* center = [NSNotificationCenter defaultCenter];
  if (audio_interruption_observer_ != nullptr) {
    id observer = (__bridge_transfer id)audio_interruption_observer_;
    [center removeObserver:observer];
    audio_interruption_observer_ = nullptr;
  }
  if (route_change_observer_ != nullptr) {
    id observer = (__bridge_transfer id)route_change_observer_;
    [center removeObserver:observer];
    route_change_observer_ = nullptr;
  }
}

void AudioDeviceIOS::SetupAudioBuffersForActiveAudioSession() {
  LOGI() << "SetupAudioBuffersForActiveAudioSession";
  // Verify the current values once the audio session has been activated.
  AVAudioSession* session = [AVAudioSession sharedInstance];
  LOG(LS_INFO) << " sample rate: " << session.sampleRate;
  LOG(LS_INFO) << " IO buffer duration: " << session.IOBufferDuration;
  LOG(LS_INFO) << " output channels: " << session.outputNumberOfChannels;
  LOG(LS_INFO) << " input channels: " << session.inputNumberOfChannels;
  LOG(LS_INFO) << " output latency: " << session.outputLatency;
  LOG(LS_INFO) << " input latency: " << session.inputLatency;

  // Log a warning message for the case when we are unable to set the preferred
  // hardware sample rate but continue and use the non-ideal sample rate after
  // reinitializing the audio parameters. Most BT headsets only support 8kHz or
  // 16kHz.
  if (session.sampleRate != kPreferredSampleRate) {
    LOG(LS_WARNING) << "Unable to set the preferred sample rate";
  }

  // At this stage, we also know the exact IO buffer duration and can add
  // that info to the existing audio parameters where it is converted into
  // number of audio frames.
  // Example: IO buffer size = 0.008 seconds <=> 128 audio frames at 16kHz.
  // Hence, 128 is the size we expect to see in upcoming render callbacks.
  playout_parameters_.reset(session.sampleRate, playout_parameters_.channels(),
                            session.IOBufferDuration);
  RTC_DCHECK(playout_parameters_.is_complete());
  record_parameters_.reset(session.sampleRate, record_parameters_.channels(),
                           session.IOBufferDuration);
  RTC_DCHECK(record_parameters_.is_complete());
  LOG(LS_INFO) << " frames per I/O buffer: "
               << playout_parameters_.frames_per_buffer();
  LOG(LS_INFO) << " bytes per I/O buffer: "
               << playout_parameters_.GetBytesPerBuffer();
  RTC_DCHECK_EQ(playout_parameters_.GetBytesPerBuffer(),
                record_parameters_.GetBytesPerBuffer());

  // Update the ADB parameters since the sample rate might have changed.
  UpdateAudioDeviceBuffer();

  // Create a modified audio buffer class which allows us to ask for,
  // or deliver, any number of samples (and not only multiple of 10ms) to match
  // the native audio unit buffer size.
  RTC_DCHECK(audio_device_buffer_);
  fine_audio_buffer_.reset(new FineAudioBuffer(
      audio_device_buffer_, playout_parameters_.GetBytesPerBuffer(),
      playout_parameters_.sample_rate()));

  // The extra/temporary playoutbuffer must be of this size to avoid
  // unnecessary memcpy while caching data between successive callbacks.
  const int required_playout_buffer_size =
      fine_audio_buffer_->RequiredPlayoutBufferSizeBytes();
  LOG(LS_INFO) << " required playout buffer size: "
               << required_playout_buffer_size;
  playout_audio_buffer_.reset(new SInt8[required_playout_buffer_size]);

  // Allocate AudioBuffers to be used as storage for the received audio.
  // The AudioBufferList structure works as a placeholder for the
  // AudioBuffer structure, which holds a pointer to the actual data buffer
  // in |record_audio_buffer_|. Recorded audio will be rendered into this memory
  // at each input callback when calling AudioUnitRender().
  const int data_byte_size = record_parameters_.GetBytesPerBuffer();
  record_audio_buffer_.reset(new SInt8[data_byte_size]);
  audio_record_buffer_list_.mNumberBuffers = 1;
  AudioBuffer* audio_buffer = &audio_record_buffer_list_.mBuffers[0];
  audio_buffer->mNumberChannels = record_parameters_.channels();
  audio_buffer->mDataByteSize = data_byte_size;
  audio_buffer->mData = record_audio_buffer_.get();
}

bool AudioDeviceIOS::SetupAndInitializeVoiceProcessingAudioUnit() {
  LOGI() << "SetupAndInitializeVoiceProcessingAudioUnit";
  RTC_DCHECK(!vpio_unit_) << "VoiceProcessingIO audio unit already exists";
  // Create an audio component description to identify the Voice-Processing
  // I/O audio unit.
  AudioComponentDescription vpio_unit_description;
  vpio_unit_description.componentType = kAudioUnitType_Output;
  vpio_unit_description.componentSubType = kAudioUnitSubType_VoiceProcessingIO;
  vpio_unit_description.componentManufacturer = kAudioUnitManufacturer_Apple;
  vpio_unit_description.componentFlags = 0;
  vpio_unit_description.componentFlagsMask = 0;

  // Obtain an audio unit instance given the description.
  AudioComponent found_vpio_unit_ref =
      AudioComponentFindNext(nullptr, &vpio_unit_description);

  // Create a Voice-Processing IO audio unit.
  OSStatus result = noErr;
  result = AudioComponentInstanceNew(found_vpio_unit_ref, &vpio_unit_);
  if (result != noErr) {
    vpio_unit_ = nullptr;
    LOG(LS_ERROR) << "AudioComponentInstanceNew failed: " << result;
    return false;
  }

  // A VP I/O unit's bus 1 connects to input hardware (microphone). Enable
  // input on the input scope of the input element.
  AudioUnitElement input_bus = 1;
  UInt32 enable_input = 1;
  result = AudioUnitSetProperty(vpio_unit_, kAudioOutputUnitProperty_EnableIO,
                                kAudioUnitScope_Input, input_bus, &enable_input,
                                sizeof(enable_input));
  if (result != noErr) {
    DisposeAudioUnit();
    LOG(LS_ERROR) << "Failed to enable input on input scope of input element: "
                  << result;
    return false;
  }

  // A VP I/O unit's bus 0 connects to output hardware (speaker). Enable
  // output on the output scope of the output element.
  AudioUnitElement output_bus = 0;
  UInt32 enable_output = 1;
  result = AudioUnitSetProperty(vpio_unit_, kAudioOutputUnitProperty_EnableIO,
                                kAudioUnitScope_Output, output_bus,
                                &enable_output, sizeof(enable_output));
  if (result != noErr) {
    DisposeAudioUnit();
    LOG(LS_ERROR)
        << "Failed to enable output on output scope of output element: "
        << result;
    return false;
  }

  // Set the application formats for input and output:
  // - use same format in both directions
  // - avoid resampling in the I/O unit by using the hardware sample rate
  // - linear PCM => noncompressed audio data format with one frame per packet
  // - no need to specify interleaving since only mono is supported
  AudioStreamBasicDescription application_format = {0};
  UInt32 size = sizeof(application_format);
  RTC_DCHECK_EQ(playout_parameters_.sample_rate(),
                record_parameters_.sample_rate());
  RTC_DCHECK_EQ(1, kPreferredNumberOfChannels);
  application_format.mSampleRate = playout_parameters_.sample_rate();
  application_format.mFormatID = kAudioFormatLinearPCM;
  application_format.mFormatFlags =
      kLinearPCMFormatFlagIsSignedInteger | kLinearPCMFormatFlagIsPacked;
  application_format.mBytesPerPacket = kBytesPerSample;
  application_format.mFramesPerPacket = 1;  // uncompressed
  application_format.mBytesPerFrame = kBytesPerSample;
  application_format.mChannelsPerFrame = kPreferredNumberOfChannels;
  application_format.mBitsPerChannel = 8 * kBytesPerSample;
  // Store the new format.
  application_format_ = application_format;
#if !defined(NDEBUG)
  LogABSD(application_format_);
#endif

  // Set the application format on the output scope of the input element/bus.
  result = AudioUnitSetProperty(vpio_unit_, kAudioUnitProperty_StreamFormat,
                                kAudioUnitScope_Output, input_bus,
                                &application_format, size);
  if (result != noErr) {
    DisposeAudioUnit();
    LOG(LS_ERROR)
        << "Failed to set application format on output scope of input bus: "
        << result;
    return false;
  }

  // Set the application format on the input scope of the output element/bus.
  result = AudioUnitSetProperty(vpio_unit_, kAudioUnitProperty_StreamFormat,
                                kAudioUnitScope_Input, output_bus,
                                &application_format, size);
  if (result != noErr) {
    DisposeAudioUnit();
    LOG(LS_ERROR)
        << "Failed to set application format on input scope of output bus: "
        << result;
    return false;
  }

  // Specify the callback function that provides audio samples to the audio
  // unit.
  AURenderCallbackStruct render_callback;
  render_callback.inputProc = GetPlayoutData;
  render_callback.inputProcRefCon = this;
  result = AudioUnitSetProperty(
      vpio_unit_, kAudioUnitProperty_SetRenderCallback, kAudioUnitScope_Input,
      output_bus, &render_callback, sizeof(render_callback));
  if (result != noErr) {
    DisposeAudioUnit();
    LOG(LS_ERROR) << "Failed to specify the render callback on the output bus: "
                  << result;
    return false;
  }

  // Disable AU buffer allocation for the recorder, we allocate our own.
  // TODO(henrika): not sure that it actually saves resource to make this call.
  UInt32 flag = 0;
  result = AudioUnitSetProperty(
      vpio_unit_, kAudioUnitProperty_ShouldAllocateBuffer,
      kAudioUnitScope_Output, input_bus, &flag, sizeof(flag));
  if (result != noErr) {
    DisposeAudioUnit();
    LOG(LS_ERROR) << "Failed to disable buffer allocation on the input bus: "
                  << result;
  }

  // Specify the callback to be called by the I/O thread to us when input audio
  // is available. The recorded samples can then be obtained by calling the
  // AudioUnitRender() method.
  AURenderCallbackStruct input_callback;
  input_callback.inputProc = RecordedDataIsAvailable;
  input_callback.inputProcRefCon = this;
  result = AudioUnitSetProperty(vpio_unit_,
                                kAudioOutputUnitProperty_SetInputCallback,
                                kAudioUnitScope_Global, input_bus,
                                &input_callback, sizeof(input_callback));
  if (result != noErr) {
    DisposeAudioUnit();
    LOG(LS_ERROR) << "Failed to specify the input callback on the input bus: "
                  << result;
  }

  // Initialize the Voice-Processing I/O unit instance.
  // Calls to AudioUnitInitialize() can fail if called back-to-back on
  // different ADM instances. The error message in this case is -66635 which is
  // undocumented. Tests have shown that calling AudioUnitInitialize a second
  // time, after a short sleep, avoids this issue.
  // See webrtc:5166 for details.
  int failed_initalize_attempts = 0;
  result = AudioUnitInitialize(vpio_unit_);
  while (result != noErr) {
    LOG(LS_ERROR) << "Failed to initialize the Voice-Processing I/O unit: "
                  << result;
    ++failed_initalize_attempts;
    if (failed_initalize_attempts == kMaxNumberOfAudioUnitInitializeAttempts) {
      // Max number of initialization attempts exceeded, hence abort.
      LOG(LS_WARNING) << "Too many initialization attempts";
      DisposeAudioUnit();
      return false;
    }
    LOG(LS_INFO) << "pause 100ms and try audio unit initialization again...";
    [NSThread sleepForTimeInterval:0.1f];
    result = AudioUnitInitialize(vpio_unit_);
  }
  LOG(LS_INFO) << "Voice-Processing I/O unit is now initialized";
  return true;
}

bool AudioDeviceIOS::RestartAudioUnitWithNewFormat(float sample_rate) {
  LOGI() << "RestartAudioUnitWithNewFormat(sample_rate=" << sample_rate << ")";
  // Stop the active audio unit.
  LOG_AND_RETURN_IF_ERROR(AudioOutputUnitStop(vpio_unit_),
                          "Failed to stop the the Voice-Processing I/O unit");

  // The stream format is about to be changed and it requires that we first
  // uninitialize it to deallocate its resources.
  LOG_AND_RETURN_IF_ERROR(
      AudioUnitUninitialize(vpio_unit_),
      "Failed to uninitialize the the Voice-Processing I/O unit");

  // Allocate new buffers given the new stream format.
  SetupAudioBuffersForActiveAudioSession();

  // Update the existing application format using the new sample rate.
  application_format_.mSampleRate = playout_parameters_.sample_rate();
  UInt32 size = sizeof(application_format_);
  AudioUnitSetProperty(vpio_unit_, kAudioUnitProperty_StreamFormat,
                       kAudioUnitScope_Output, 1, &application_format_, size);
  AudioUnitSetProperty(vpio_unit_, kAudioUnitProperty_StreamFormat,
                       kAudioUnitScope_Input, 0, &application_format_, size);

  // Prepare the audio unit to render audio again.
  LOG_AND_RETURN_IF_ERROR(AudioUnitInitialize(vpio_unit_),
                          "Failed to initialize the Voice-Processing I/O unit");
  LOG(LS_INFO) << "Voice-Processing I/O unit is now reinitialized";

  // Start rendering audio using the new format.
  LOG_AND_RETURN_IF_ERROR(AudioOutputUnitStart(vpio_unit_),
                          "Failed to start the Voice-Processing I/O unit");
  LOG(LS_INFO) << "Voice-Processing I/O unit is now restarted";
  return true;
}

bool AudioDeviceIOS::InitPlayOrRecord() {
  LOGI() << "InitPlayOrRecord";
  // Activate the audio session if not already activated.
  if (!ActivateAudioSession()) {
    return false;
  }

  // Ensure that the active audio session has the correct category and mode.
  AVAudioSession* session = [AVAudioSession sharedInstance];
  if (!VerifyAudioSession(session)) {
    DeactivateAudioSession();
    LOG(LS_ERROR) << "Failed to verify audio session category and mode";
    return false;
  }

  // Start observing audio session interruptions and route changes.
  RegisterNotificationObservers();

  // Ensure that we got what what we asked for in our active audio session.
  SetupAudioBuffersForActiveAudioSession();

  // Create, setup and initialize a new Voice-Processing I/O unit.
  if (!SetupAndInitializeVoiceProcessingAudioUnit()) {
    // Reduce usage count for the audio session and possibly deactivate it if
    // this object is the only user.
    DeactivateAudioSession();
    return false;
  }
  return true;
}

void AudioDeviceIOS::ShutdownPlayOrRecord() {
  LOGI() << "ShutdownPlayOrRecord";
  // Close and delete the voice-processing I/O unit.
  OSStatus result = -1;
  if (nullptr != vpio_unit_) {
    result = AudioOutputUnitStop(vpio_unit_);
    if (result != noErr) {
      LOG_F(LS_ERROR) << "AudioOutputUnitStop failed: " << result;
    }
    result = AudioUnitUninitialize(vpio_unit_);
    if (result != noErr) {
      LOG_F(LS_ERROR) << "AudioUnitUninitialize failed: " << result;
    }
    DisposeAudioUnit();
  }

  // Remove audio session notification observers.
  UnregisterNotificationObservers();

  // All I/O should be stopped or paused prior to deactivating the audio
  // session, hence we deactivate as last action.
  DeactivateAudioSession();
}

void AudioDeviceIOS::DisposeAudioUnit() {
  if (nullptr == vpio_unit_)
    return;
  OSStatus result = AudioComponentInstanceDispose(vpio_unit_);
  if (result != noErr) {
    LOG(LS_ERROR) << "AudioComponentInstanceDispose failed:" << result;
  }
  vpio_unit_ = nullptr;
}

OSStatus AudioDeviceIOS::RecordedDataIsAvailable(
    void* in_ref_con,
    AudioUnitRenderActionFlags* io_action_flags,
    const AudioTimeStamp* in_time_stamp,
    UInt32 in_bus_number,
    UInt32 in_number_frames,
    AudioBufferList* io_data) {
  RTC_DCHECK_EQ(1u, in_bus_number);
  RTC_DCHECK(
      !io_data);  // no buffer should be allocated for input at this stage
  AudioDeviceIOS* audio_device_ios = static_cast<AudioDeviceIOS*>(in_ref_con);
  return audio_device_ios->OnRecordedDataIsAvailable(
      io_action_flags, in_time_stamp, in_bus_number, in_number_frames);
}

OSStatus AudioDeviceIOS::OnRecordedDataIsAvailable(
    AudioUnitRenderActionFlags* io_action_flags,
    const AudioTimeStamp* in_time_stamp,
    UInt32 in_bus_number,
    UInt32 in_number_frames) {
  OSStatus result = noErr;
  // Simply return if recording is not enabled.
  if (!rtc::AtomicOps::AcquireLoad(&recording_))
    return result;
  if (in_number_frames != record_parameters_.frames_per_buffer()) {
    // We have seen short bursts (1-2 frames) where |in_number_frames| changes.
    // Add a log to keep track of longer sequences if that should ever happen.
    // Also return since calling AudioUnitRender in this state will only result
    // in kAudio_ParamError (-50) anyhow.
    LOG(LS_WARNING) << "in_number_frames (" << in_number_frames
                    << ") != " << record_parameters_.frames_per_buffer();
    return noErr;
  }
  // Obtain the recorded audio samples by initiating a rendering cycle.
  // Since it happens on the input bus, the |io_data| parameter is a reference
  // to the preallocated audio buffer list that the audio unit renders into.
  // TODO(henrika): should error handling be improved?
  AudioBufferList* io_data = &audio_record_buffer_list_;
  result = AudioUnitRender(vpio_unit_, io_action_flags, in_time_stamp,
                           in_bus_number, in_number_frames, io_data);
  if (result != noErr) {
    LOG_F(LS_ERROR) << "AudioUnitRender failed: " << result;
    return result;
  }
  // Get a pointer to the recorded audio and send it to the WebRTC ADB.
  // Use the FineAudioBuffer instance to convert between native buffer size
  // and the 10ms buffer size used by WebRTC.
  const UInt32 data_size_in_bytes = io_data->mBuffers[0].mDataByteSize;
  RTC_CHECK_EQ(data_size_in_bytes / kBytesPerSample, in_number_frames);
  SInt8* data = static_cast<SInt8*>(io_data->mBuffers[0].mData);
  fine_audio_buffer_->DeliverRecordedData(data, data_size_in_bytes,
                                          kFixedPlayoutDelayEstimate,
                                          kFixedRecordDelayEstimate);
  return noErr;
}

OSStatus AudioDeviceIOS::GetPlayoutData(
    void* in_ref_con,
    AudioUnitRenderActionFlags* io_action_flags,
    const AudioTimeStamp* in_time_stamp,
    UInt32 in_bus_number,
    UInt32 in_number_frames,
    AudioBufferList* io_data) {
  RTC_DCHECK_EQ(0u, in_bus_number);
  RTC_DCHECK(io_data);
  AudioDeviceIOS* audio_device_ios = static_cast<AudioDeviceIOS*>(in_ref_con);
  return audio_device_ios->OnGetPlayoutData(io_action_flags, in_number_frames,
                                            io_data);
}

OSStatus AudioDeviceIOS::OnGetPlayoutData(
    AudioUnitRenderActionFlags* io_action_flags,
    UInt32 in_number_frames,
    AudioBufferList* io_data) {
  // Verify 16-bit, noninterleaved mono PCM signal format.
  RTC_DCHECK_EQ(1u, io_data->mNumberBuffers);
  RTC_DCHECK_EQ(1u, io_data->mBuffers[0].mNumberChannels);
  // Get pointer to internal audio buffer to which new audio data shall be
  // written.
  const UInt32 dataSizeInBytes = io_data->mBuffers[0].mDataByteSize;
  RTC_CHECK_EQ(dataSizeInBytes / kBytesPerSample, in_number_frames);
  SInt8* destination = static_cast<SInt8*>(io_data->mBuffers[0].mData);
  // Produce silence and give audio unit a hint about it if playout is not
  // activated.
  if (!rtc::AtomicOps::AcquireLoad(&playing_)) {
    *io_action_flags |= kAudioUnitRenderAction_OutputIsSilence;
    memset(destination, 0, dataSizeInBytes);
    return noErr;
  }
  // Read decoded 16-bit PCM samples from WebRTC (using a size that matches
  // the native I/O audio unit) to a preallocated intermediate buffer and
  // copy the result to the audio buffer in the |io_data| destination.
  SInt8* source = playout_audio_buffer_.get();
  fine_audio_buffer_->GetPlayoutData(source);
  memcpy(destination, source, dataSizeInBytes);
  return noErr;
}

}  // namespace webrtc

/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "webrtc/modules/audio_device/test/audio_device_test_defines.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "webrtc/test/testsupport/fileutils.h"

#include "webrtc/modules/audio_device/audio_device_config.h"
#include "webrtc/modules/audio_device/audio_device_impl.h"
#include "webrtc/modules/audio_device/audio_device_utility.h"
#include "webrtc/system_wrappers/interface/sleep.h"

// Helper functions
#if defined(ANDROID)
char filenameStr[2][256] =
{ {0},
  {0},
}; // Allow two buffers for those API calls taking two filenames
int currentStr = 0;

const char* GetFilename(const char* filename)
{
  currentStr = !currentStr;
  sprintf(filenameStr[currentStr], "/sdcard/admtest/%s", filename);
  return filenameStr[currentStr];
}
#elif !defined(WEBRTC_IOS)
const char* GetFilename(const char* filename) {
  std::string full_path_filename = webrtc::test::OutputPath() + filename;
  return full_path_filename.c_str();
}
#endif

using namespace webrtc;

class AudioEventObserverAPI: public AudioDeviceObserver {
 public:
  AudioEventObserverAPI(AudioDeviceModule* audioDevice)
      : error_(kRecordingError),
        warning_(kRecordingWarning),
        audio_device_(audioDevice) {
  }

  ~AudioEventObserverAPI() {}

  virtual void OnErrorIsReported(const ErrorCode error) {
    TEST_LOG("\n[*** ERROR ***] => OnErrorIsReported(%d)\n\n", error);
    error_ = error;
  }

  virtual void OnWarningIsReported(const WarningCode warning) {
    TEST_LOG("\n[*** WARNING ***] => OnWarningIsReported(%d)\n\n", warning);
    warning_ = warning;
    EXPECT_EQ(0, audio_device_->StopRecording());
    EXPECT_EQ(0, audio_device_->StopPlayout());
  }

 public:
  ErrorCode error_;
  WarningCode warning_;
 private:
  AudioDeviceModule* audio_device_;
};

class AudioTransportAPI: public AudioTransport {
 public:
  AudioTransportAPI(AudioDeviceModule* audioDevice)
      : rec_count_(0),
        play_count_(0) {
  }

  ~AudioTransportAPI() {}

  virtual int32_t RecordedDataIsAvailable(
      const void* audioSamples,
      const uint32_t nSamples,
      const uint8_t nBytesPerSample,
      const uint8_t nChannels,
      const uint32_t sampleRate,
      const uint32_t totalDelay,
      const int32_t clockSkew,
      const uint32_t currentMicLevel,
      const bool keyPressed,
      uint32_t& newMicLevel) {
    rec_count_++;
    if (rec_count_ % 100 == 0) {
      if (nChannels == 1) {
        // mono
        TEST_LOG("-");
      } else if ((nChannels == 2) && (nBytesPerSample == 2)) {
        // stereo but only using one channel
        TEST_LOG("-|");
      } else {
        // stereo
        TEST_LOG("--");
      }
    }
    return 0;
  }

  virtual int32_t NeedMorePlayData(
      const uint32_t nSamples,
      const uint8_t nBytesPerSample,
      const uint8_t nChannels,
      const uint32_t sampleRate,
      void* audioSamples,
      uint32_t& nSamplesOut) {
    play_count_++;
    if (play_count_ % 100 == 0) {
      if (nChannels == 1) {
        TEST_LOG("+");
      } else {
        TEST_LOG("++");
      }
    }
    nSamplesOut = 480;
    return 0;
  }

  virtual int OnDataAvailable(const int voe_channels[],
                              int number_of_voe_channels,
                              const int16_t* audio_data,
                              int sample_rate,
                              int number_of_channels,
                              int number_of_frames,
                              int audio_delay_milliseconds,
                              int current_volume,
                              bool key_pressed,
                              bool need_audio_processing) {
    return 0;
  }

  virtual void OnData(int voe_channel, const void* audio_data,
                      int bits_per_sample, int sample_rate,
                      int number_of_channels,
                      int number_of_frames) {}
 private:
  uint32_t rec_count_;
  uint32_t play_count_;
};

class AudioDeviceAPITest: public testing::Test {
 protected:
  AudioDeviceAPITest() {}

  virtual ~AudioDeviceAPITest() {}

  static void SetUpTestCase() {
    process_thread_ = ProcessThread::CreateProcessThread();
    process_thread_->Start();

    // Windows:
    //      if (WEBRTC_WINDOWS_CORE_AUDIO_BUILD)
    //          user can select between default (Core) or Wave
    //      else
    //          user can select between default (Wave) or Wave
    const int32_t kId = 444;

#if defined(_WIN32)
    EXPECT_TRUE((audio_device_ = AudioDeviceModuleImpl::Create(
                kId, AudioDeviceModule::kLinuxAlsaAudio)) == NULL);
#if defined(WEBRTC_WINDOWS_CORE_AUDIO_BUILD)
    TEST_LOG("WEBRTC_WINDOWS_CORE_AUDIO_BUILD is defined!\n\n");
    // create default implementation (=Core Audio) instance
    EXPECT_TRUE((audio_device_ = AudioDeviceModuleImpl::Create(
                kId, AudioDeviceModule::kPlatformDefaultAudio)) != NULL);
    audio_device_->AddRef();
    EXPECT_EQ(0, audio_device_->Release());
    // create non-default (=Wave Audio) instance
    EXPECT_TRUE((audio_device_ = AudioDeviceModuleImpl::Create(
                kId, AudioDeviceModule::kWindowsWaveAudio)) != NULL);
    audio_device_->AddRef();
    EXPECT_EQ(0, audio_device_->Release());
    // explicitly specify usage of Core Audio (same as default)
    EXPECT_TRUE((audio_device_ = AudioDeviceModuleImpl::Create(
                kId, AudioDeviceModule::kWindowsCoreAudio)) != NULL);
#else
    TEST_LOG("WEBRTC_WINDOWS_CORE_AUDIO_BUILD is *not* defined!\n");
    EXPECT_TRUE((audio_device_ = AudioDeviceModuleImpl::Create(
                kId, AudioDeviceModule::kWindowsCoreAudio)) == NULL);
    // create default implementation (=Wave Audio) instance
    EXPECT_TRUE((audio_device_ = AudioDeviceModuleImpl::Create(
                kId, AudioDeviceModule::kPlatformDefaultAudio)) != NULL);
    audio_device_->AddRef();
    EXPECT_EQ(0, audio_device_->Release());
    // explicitly specify usage of Wave Audio (same as default)
    EXPECT_TRUE((audio_device_ = AudioDeviceModuleImpl::Create(
                kId, AudioDeviceModule::kWindowsWaveAudio)) != NULL);
#endif
#endif

#if defined(ANDROID)
    // Fails tests
    EXPECT_TRUE((audio_device_ = AudioDeviceModuleImpl::Create(
                kId, AudioDeviceModule::kWindowsWaveAudio)) == NULL);
    EXPECT_TRUE((audio_device_ = AudioDeviceModuleImpl::Create(
                kId, AudioDeviceModule::kWindowsCoreAudio)) == NULL);
    EXPECT_TRUE((audio_device_ = AudioDeviceModuleImpl::Create(
                kId, AudioDeviceModule::kLinuxAlsaAudio)) == NULL);
    EXPECT_TRUE((audio_device_ = AudioDeviceModuleImpl::Create(
                kId, AudioDeviceModule::kLinuxPulseAudio)) == NULL);
    // Create default implementation instance
    EXPECT_TRUE((audio_device_ = AudioDeviceModuleImpl::Create(
                kId, AudioDeviceModule::kPlatformDefaultAudio)) != NULL);
#elif defined(WEBRTC_LINUX)
    EXPECT_TRUE((audio_device_ = AudioDeviceModuleImpl::Create(
                kId, AudioDeviceModule::kWindowsWaveAudio)) == NULL);
    EXPECT_TRUE((audio_device_ = AudioDeviceModuleImpl::Create(
                kId, AudioDeviceModule::kWindowsCoreAudio)) == NULL);
    // create default implementation instance
    EXPECT_TRUE((audio_device_ = AudioDeviceModuleImpl::Create(
                kId, AudioDeviceModule::kPlatformDefaultAudio)) != NULL);
    audio_device_->AddRef();
    EXPECT_EQ(0, audio_device_->Terminate());
    EXPECT_EQ(0, audio_device_->Release());
    // explicitly specify usage of Pulse Audio (same as default)
    EXPECT_TRUE((audio_device_ = AudioDeviceModuleImpl::Create(
                kId, AudioDeviceModule::kLinuxPulseAudio)) != NULL);
#endif

#if defined(WEBRTC_MAC)
    // Fails tests
    EXPECT_TRUE((audio_device_ = AudioDeviceModuleImpl::Create(
                kId, AudioDeviceModule::kWindowsWaveAudio)) == NULL);
    EXPECT_TRUE((audio_device_ = AudioDeviceModuleImpl::Create(
                kId, AudioDeviceModule::kWindowsCoreAudio)) == NULL);
    EXPECT_TRUE((audio_device_ = AudioDeviceModuleImpl::Create(
                kId, AudioDeviceModule::kLinuxAlsaAudio)) == NULL);
    EXPECT_TRUE((audio_device_ = AudioDeviceModuleImpl::Create(
                kId, AudioDeviceModule::kLinuxPulseAudio)) == NULL);
    // Create default implementation instance
    EXPECT_TRUE((audio_device_ = AudioDeviceModuleImpl::Create(
                kId, AudioDeviceModule::kPlatformDefaultAudio)) != NULL);
#endif

    if (audio_device_ == NULL) {
      FAIL() << "Failed creating audio device object!";
    }

    // The ADM is reference counted.
    audio_device_->AddRef();

    process_thread_->RegisterModule(audio_device_);

    AudioDeviceModule::AudioLayer audio_layer =
        AudioDeviceModule::kPlatformDefaultAudio;
    EXPECT_EQ(0, audio_device_->ActiveAudioLayer(&audio_layer));
    if (audio_layer == AudioDeviceModule::kLinuxAlsaAudio) {
      linux_alsa_ = true;
    }
  }

  static void TearDownTestCase() {
    if (process_thread_) {
      process_thread_->DeRegisterModule(audio_device_);
      process_thread_->Stop();
      ProcessThread::DestroyProcessThread(process_thread_);
    }
    if (event_observer_) {
      delete event_observer_;
      event_observer_ = NULL;
    }
    if (audio_transport_) {
      delete audio_transport_;
      audio_transport_ = NULL;
    }
    if (audio_device_) {
      EXPECT_EQ(0, audio_device_->Release());
    }
    PRINT_TEST_RESULTS;
  }

  void SetUp() {
    if (linux_alsa_) {
      FAIL() << "API Test is not available on ALSA on Linux!";
    }
    EXPECT_EQ(0, audio_device_->Init());
    EXPECT_TRUE(audio_device_->Initialized());
  }

  void TearDown() {
    EXPECT_EQ(0, audio_device_->Terminate());
  }

  void CheckVolume(uint32_t expected, uint32_t actual) {
    // Mac and Windows have lower resolution on the volume settings.
#if defined(WEBRTC_MAC) || defined(_WIN32)
    int diff = abs(static_cast<int>(expected - actual));
    EXPECT_LE(diff, 5);
#else
    EXPECT_TRUE((actual == expected) || (actual == expected-1));
#endif
  }

  void CheckInitialPlayoutStates() {
    EXPECT_FALSE(audio_device_->PlayoutIsInitialized());
    EXPECT_FALSE(audio_device_->Playing());
    EXPECT_FALSE(audio_device_->SpeakerIsInitialized());
  }

  void CheckInitialRecordingStates() {
    EXPECT_FALSE(audio_device_->RecordingIsInitialized());
    EXPECT_FALSE(audio_device_->Recording());
    EXPECT_FALSE(audio_device_->MicrophoneIsInitialized());
  }

  static bool linux_alsa_;
  static ProcessThread* process_thread_;
  static AudioDeviceModule* audio_device_;
  static AudioTransportAPI* audio_transport_;
  static AudioEventObserverAPI* event_observer_;
};

// Must be initialized like this to handle static SetUpTestCase() above.
bool AudioDeviceAPITest::linux_alsa_ = false;
ProcessThread* AudioDeviceAPITest::process_thread_ = NULL;
AudioDeviceModule* AudioDeviceAPITest::audio_device_ = NULL;
AudioTransportAPI* AudioDeviceAPITest::audio_transport_ = NULL;
AudioEventObserverAPI* AudioDeviceAPITest::event_observer_ = NULL;

TEST_F(AudioDeviceAPITest, RegisterEventObserver) {
  event_observer_ = new AudioEventObserverAPI(audio_device_);
  EXPECT_EQ(0, audio_device_->RegisterEventObserver(NULL));
  EXPECT_EQ(0, audio_device_->RegisterEventObserver(event_observer_));
  EXPECT_EQ(0, audio_device_->RegisterEventObserver(NULL));
}

TEST_F(AudioDeviceAPITest, RegisterAudioCallback) {
  audio_transport_ = new AudioTransportAPI(audio_device_);
  EXPECT_EQ(0, audio_device_->RegisterAudioCallback(NULL));
  EXPECT_EQ(0, audio_device_->RegisterAudioCallback(audio_transport_));
  EXPECT_EQ(0, audio_device_->RegisterAudioCallback(NULL));
}

TEST_F(AudioDeviceAPITest, Init) {
  EXPECT_TRUE(audio_device_->Initialized());
  EXPECT_EQ(0, audio_device_->Init());
  EXPECT_TRUE(audio_device_->Initialized());
  EXPECT_EQ(0, audio_device_->Terminate());
  EXPECT_FALSE(audio_device_->Initialized());
  EXPECT_EQ(0, audio_device_->Init());
  EXPECT_TRUE(audio_device_->Initialized());
  EXPECT_EQ(0, audio_device_->Terminate());
  EXPECT_FALSE(audio_device_->Initialized());
}

TEST_F(AudioDeviceAPITest, Terminate) {
  EXPECT_TRUE(audio_device_->Initialized());
  EXPECT_EQ(0, audio_device_->Terminate());
  EXPECT_FALSE(audio_device_->Initialized());
  EXPECT_EQ(0, audio_device_->Terminate());
  EXPECT_FALSE(audio_device_->Initialized());
  EXPECT_EQ(0, audio_device_->Init());
  EXPECT_TRUE(audio_device_->Initialized());
  EXPECT_EQ(0, audio_device_->Terminate());
  EXPECT_FALSE(audio_device_->Initialized());
}

TEST_F(AudioDeviceAPITest, PlayoutDevices) {
  EXPECT_GT(audio_device_->PlayoutDevices(), 0);
  EXPECT_GT(audio_device_->PlayoutDevices(), 0);
}

TEST_F(AudioDeviceAPITest, RecordingDevices) {
  EXPECT_GT(audio_device_->RecordingDevices(), 0);
  EXPECT_GT(audio_device_->RecordingDevices(), 0);
}

TEST_F(AudioDeviceAPITest, PlayoutDeviceName) {
  char name[kAdmMaxDeviceNameSize];
  char guid[kAdmMaxGuidSize];
  int16_t no_devices = audio_device_->PlayoutDevices();

  // fail tests
  EXPECT_EQ(-1, audio_device_->PlayoutDeviceName(-2, name, guid));
  EXPECT_EQ(-1, audio_device_->PlayoutDeviceName(no_devices, name, guid));
  EXPECT_EQ(-1, audio_device_->PlayoutDeviceName(0, NULL, guid));

  // bulk tests
  EXPECT_EQ(0, audio_device_->PlayoutDeviceName(0, name, NULL));
#ifdef _WIN32
  // shall be mapped to 0.
  EXPECT_EQ(0, audio_device_->PlayoutDeviceName(-1, name, NULL));
#else
  EXPECT_EQ(-1, audio_device_->PlayoutDeviceName(-1, name, NULL));
#endif
  for (int i = 0; i < no_devices; i++) {
    EXPECT_EQ(0, audio_device_->PlayoutDeviceName(i, name, guid));
    EXPECT_EQ(0, audio_device_->PlayoutDeviceName(i, name, NULL));
  }
}

TEST_F(AudioDeviceAPITest, RecordingDeviceName) {
  char name[kAdmMaxDeviceNameSize];
  char guid[kAdmMaxGuidSize];
  int16_t no_devices = audio_device_->RecordingDevices();

  // fail tests
  EXPECT_EQ(-1, audio_device_->RecordingDeviceName(-2, name, guid));
  EXPECT_EQ(-1, audio_device_->RecordingDeviceName(no_devices, name, guid));
  EXPECT_EQ(-1, audio_device_->RecordingDeviceName(0, NULL, guid));

  // bulk tests
  EXPECT_EQ(0, audio_device_->RecordingDeviceName(0, name, NULL));
#ifdef _WIN32
  // shall me mapped to 0
  EXPECT_EQ(0, audio_device_->RecordingDeviceName(-1, name, NULL));
#else
  EXPECT_EQ(-1, audio_device_->RecordingDeviceName(-1, name, NULL));
#endif
  for (int i = 0; i < no_devices; i++) {
    EXPECT_EQ(0, audio_device_->RecordingDeviceName(i, name, guid));
    EXPECT_EQ(0, audio_device_->RecordingDeviceName(i, name, NULL));
  }
}

TEST_F(AudioDeviceAPITest, SetPlayoutDevice) {
  int16_t no_devices = audio_device_->PlayoutDevices();

  // fail tests
  EXPECT_EQ(-1, audio_device_->SetPlayoutDevice(-1));
  EXPECT_EQ(-1, audio_device_->SetPlayoutDevice(no_devices));

  // bulk tests
#ifdef _WIN32
  EXPECT_EQ(0, audio_device_->SetPlayoutDevice(
      AudioDeviceModule::kDefaultCommunicationDevice));
  EXPECT_EQ(0, audio_device_->SetPlayoutDevice(
      AudioDeviceModule::kDefaultDevice));
#else
  EXPECT_EQ(-1, audio_device_->SetPlayoutDevice(
      AudioDeviceModule::kDefaultCommunicationDevice));
  EXPECT_EQ(-1, audio_device_->SetPlayoutDevice(
      AudioDeviceModule::kDefaultDevice));
#endif
  for (int i = 0; i < no_devices; i++) {
    EXPECT_EQ(0, audio_device_->SetPlayoutDevice(i));
  }
}

TEST_F(AudioDeviceAPITest, SetRecordingDevice) {
  EXPECT_EQ(0, audio_device_->Init());
  int16_t no_devices = audio_device_->RecordingDevices();

  // fail tests
  EXPECT_EQ(-1, audio_device_->SetRecordingDevice(-1));
  EXPECT_EQ(-1, audio_device_->SetRecordingDevice(no_devices));

  // bulk tests
#ifdef _WIN32
  EXPECT_TRUE(audio_device_->SetRecordingDevice(
          AudioDeviceModule::kDefaultCommunicationDevice) == 0);
  EXPECT_EQ(0, audio_device_->SetRecordingDevice(
      AudioDeviceModule::kDefaultDevice));
#else
  EXPECT_TRUE(audio_device_->SetRecordingDevice(
      AudioDeviceModule::kDefaultCommunicationDevice) == -1);
  EXPECT_TRUE(audio_device_->SetRecordingDevice(
      AudioDeviceModule::kDefaultDevice) == -1);
#endif
  for (int i = 0; i < no_devices; i++) {
    EXPECT_EQ(0, audio_device_->SetRecordingDevice(i));
  }
}

TEST_F(AudioDeviceAPITest, PlayoutIsAvailable) {
  bool available;
#ifdef _WIN32
  EXPECT_TRUE(audio_device_->SetPlayoutDevice(
          AudioDeviceModule::kDefaultCommunicationDevice) == 0);
  EXPECT_EQ(0, audio_device_->PlayoutIsAvailable(&available));
  // Availability check should not initialize.
  EXPECT_FALSE(audio_device_->PlayoutIsInitialized());

  EXPECT_EQ(0,
            audio_device_->SetPlayoutDevice(AudioDeviceModule::kDefaultDevice));
  EXPECT_EQ(0, audio_device_->PlayoutIsAvailable(&available));
  EXPECT_FALSE(audio_device_->PlayoutIsInitialized());
#endif

  int16_t no_devices = audio_device_->PlayoutDevices();
  for (int i = 0; i < no_devices; i++) {
    EXPECT_EQ(0, audio_device_->SetPlayoutDevice(i));
    EXPECT_EQ(0, audio_device_->PlayoutIsAvailable(&available));
    EXPECT_FALSE(audio_device_->PlayoutIsInitialized());
  }
}

TEST_F(AudioDeviceAPITest, RecordingIsAvailable) {
  bool available;
#ifdef _WIN32
  EXPECT_EQ(0, audio_device_->SetRecordingDevice(
      AudioDeviceModule::kDefaultCommunicationDevice));
  EXPECT_EQ(0, audio_device_->RecordingIsAvailable(&available));
  EXPECT_FALSE(audio_device_->RecordingIsInitialized());

  EXPECT_EQ(0, audio_device_->SetRecordingDevice(
      AudioDeviceModule::kDefaultDevice));
  EXPECT_EQ(0, audio_device_->RecordingIsAvailable(&available));
  EXPECT_FALSE(audio_device_->RecordingIsInitialized());
#endif

  int16_t no_devices = audio_device_->RecordingDevices();
  for (int i = 0; i < no_devices; i++) {
    EXPECT_EQ(0, audio_device_->SetRecordingDevice(i));
    EXPECT_EQ(0, audio_device_->RecordingIsAvailable(&available));
    EXPECT_FALSE(audio_device_->RecordingIsInitialized());
  }
}

TEST_F(AudioDeviceAPITest, InitPlayout) {
  // check initial state
  EXPECT_FALSE(audio_device_->PlayoutIsInitialized());

  // ensure that device must be set before we can initialize
  EXPECT_EQ(-1, audio_device_->InitPlayout());
  EXPECT_EQ(0, audio_device_->SetPlayoutDevice(MACRO_DEFAULT_DEVICE));
  EXPECT_EQ(0, audio_device_->InitPlayout());
  EXPECT_TRUE(audio_device_->PlayoutIsInitialized());

  // bulk tests
  bool available;
  EXPECT_EQ(0, audio_device_->PlayoutIsAvailable(&available));
  if (available) {
    EXPECT_EQ(0, audio_device_->InitPlayout());
    EXPECT_TRUE(audio_device_->PlayoutIsInitialized());
    EXPECT_EQ(0, audio_device_->InitPlayout());
    EXPECT_EQ(-1, audio_device_->SetPlayoutDevice(
        MACRO_DEFAULT_COMMUNICATION_DEVICE));
    EXPECT_EQ(0, audio_device_->StopPlayout());
    EXPECT_FALSE(audio_device_->PlayoutIsInitialized());
  }

  EXPECT_EQ(0, audio_device_->SetPlayoutDevice(
      MACRO_DEFAULT_COMMUNICATION_DEVICE));
  EXPECT_EQ(0, audio_device_->PlayoutIsAvailable(&available));
  if (available) {
    EXPECT_EQ(0, audio_device_->InitPlayout());
    // Sleep is needed for e.g. iPhone since we after stopping then starting may
    // have a hangover time of a couple of ms before initialized.
    SleepMs(50);
    EXPECT_TRUE(audio_device_->PlayoutIsInitialized());
  }

  int16_t no_devices = audio_device_->PlayoutDevices();
  for (int i = 0; i < no_devices; i++) {
    EXPECT_EQ(0, audio_device_->PlayoutIsAvailable(&available));
    if (available) {
      EXPECT_EQ(0, audio_device_->StopPlayout());
      EXPECT_FALSE(audio_device_->PlayoutIsInitialized());
      EXPECT_EQ(0, audio_device_->SetPlayoutDevice(i));
      EXPECT_EQ(0, audio_device_->PlayoutIsAvailable(&available));
      if (available) {
        EXPECT_EQ(0, audio_device_->InitPlayout());
        EXPECT_TRUE(audio_device_->PlayoutIsInitialized());
      }
    }
  }
  EXPECT_EQ(0, audio_device_->StopPlayout());
}

TEST_F(AudioDeviceAPITest, InitRecording) {
  // check initial state
  EXPECT_FALSE(audio_device_->RecordingIsInitialized());

  // ensure that device must be set before we can initialize
  EXPECT_EQ(-1, audio_device_->InitRecording());
  EXPECT_EQ(0, audio_device_->SetRecordingDevice(MACRO_DEFAULT_DEVICE));
  EXPECT_EQ(0, audio_device_->InitRecording());
  EXPECT_TRUE(audio_device_->RecordingIsInitialized());

  // bulk tests
  bool available;
  EXPECT_EQ(0, audio_device_->RecordingIsAvailable(&available));
  if (available) {
    EXPECT_EQ(0, audio_device_->InitRecording());
    EXPECT_TRUE(audio_device_->RecordingIsInitialized());
    EXPECT_EQ(0, audio_device_->InitRecording());
    EXPECT_EQ(-1,
        audio_device_->SetRecordingDevice(MACRO_DEFAULT_COMMUNICATION_DEVICE));
    EXPECT_EQ(0, audio_device_->StopRecording());
    EXPECT_FALSE(audio_device_->RecordingIsInitialized());
  }

  EXPECT_EQ(0,
      audio_device_->SetRecordingDevice(MACRO_DEFAULT_COMMUNICATION_DEVICE));
  EXPECT_EQ(0, audio_device_->RecordingIsAvailable(&available));
  if (available) {
    EXPECT_EQ(0, audio_device_->InitRecording());
    SleepMs(50);
    EXPECT_TRUE(audio_device_->RecordingIsInitialized());
  }

  int16_t no_devices = audio_device_->RecordingDevices();
  for (int i = 0; i < no_devices; i++) {
    EXPECT_EQ(0, audio_device_->RecordingIsAvailable(&available));
    if (available) {
      EXPECT_EQ(0, audio_device_->StopRecording());
      EXPECT_FALSE(audio_device_->RecordingIsInitialized());
      EXPECT_EQ(0, audio_device_->SetRecordingDevice(i));
      EXPECT_EQ(0, audio_device_->RecordingIsAvailable(&available));
      if (available) {
        EXPECT_EQ(0, audio_device_->InitRecording());
        EXPECT_TRUE(audio_device_->RecordingIsInitialized());
      }
    }
  }
  EXPECT_EQ(0, audio_device_->StopRecording());
}

TEST_F(AudioDeviceAPITest, StartAndStopPlayout) {
  bool available;
  EXPECT_EQ(0, audio_device_->RegisterAudioCallback(NULL));

  CheckInitialPlayoutStates();

  EXPECT_EQ(-1, audio_device_->StartPlayout());
  EXPECT_EQ(0, audio_device_->StopPlayout());

#ifdef _WIN32
  // kDefaultCommunicationDevice
  EXPECT_TRUE(audio_device_->SetPlayoutDevice(
          AudioDeviceModule::kDefaultCommunicationDevice) == 0);
  EXPECT_EQ(0, audio_device_->PlayoutIsAvailable(&available));
  if (available)
  {
    EXPECT_FALSE(audio_device_->PlayoutIsInitialized());
    EXPECT_EQ(0, audio_device_->InitPlayout());
    EXPECT_EQ(0, audio_device_->StartPlayout());
    EXPECT_TRUE(audio_device_->Playing());
    EXPECT_EQ(0, audio_device_->RegisterAudioCallback(audio_transport_));
    EXPECT_EQ(0, audio_device_->StopPlayout());
    EXPECT_FALSE(audio_device_->Playing());
    EXPECT_EQ(0, audio_device_->RegisterAudioCallback(NULL));
  }
#endif

  // repeat test but for kDefaultDevice
  EXPECT_EQ(0, audio_device_->SetPlayoutDevice(MACRO_DEFAULT_DEVICE));
  EXPECT_EQ(0, audio_device_->PlayoutIsAvailable(&available));
  if (available) {
    EXPECT_FALSE(audio_device_->PlayoutIsInitialized());
    EXPECT_EQ(0, audio_device_->InitPlayout());
    EXPECT_EQ(0, audio_device_->StartPlayout());
    EXPECT_TRUE(audio_device_->Playing());
    EXPECT_EQ(0, audio_device_->RegisterAudioCallback(audio_transport_));
    EXPECT_EQ(0, audio_device_->StopPlayout());
    EXPECT_FALSE(audio_device_->Playing());
  }

  // repeat test for all devices
  int16_t no_devices = audio_device_->PlayoutDevices();
  for (int i = 0; i < no_devices; i++) {
    EXPECT_EQ(0, audio_device_->SetPlayoutDevice(i));
    EXPECT_EQ(0, audio_device_->PlayoutIsAvailable(&available));
    if (available) {
      EXPECT_FALSE(audio_device_->PlayoutIsInitialized());
      EXPECT_EQ(0, audio_device_->InitPlayout());
      EXPECT_EQ(0, audio_device_->StartPlayout());
      EXPECT_TRUE(audio_device_->Playing());
      EXPECT_EQ(0, audio_device_->RegisterAudioCallback(audio_transport_));
      EXPECT_EQ(0, audio_device_->StopPlayout());
      EXPECT_FALSE(audio_device_->Playing());
    }
  }
}

TEST_F(AudioDeviceAPITest, StartAndStopRecording) {
  bool available;
  EXPECT_EQ(0, audio_device_->RegisterAudioCallback(NULL));

  CheckInitialRecordingStates();

  EXPECT_EQ(-1, audio_device_->StartRecording());
  EXPECT_EQ(0, audio_device_->StopRecording());

#ifdef _WIN32
  // kDefaultCommunicationDevice
  EXPECT_TRUE(audio_device_->SetRecordingDevice(
          AudioDeviceModule::kDefaultCommunicationDevice) == 0);
  EXPECT_EQ(0, audio_device_->RecordingIsAvailable(&available));
  if (available)
  {
    EXPECT_FALSE(audio_device_->RecordingIsInitialized());
    EXPECT_EQ(0, audio_device_->InitRecording());
    EXPECT_EQ(0, audio_device_->StartRecording());
    EXPECT_TRUE(audio_device_->Recording());
    EXPECT_EQ(0, audio_device_->RegisterAudioCallback(audio_transport_));
    EXPECT_EQ(0, audio_device_->StopRecording());
    EXPECT_FALSE(audio_device_->Recording());
    EXPECT_EQ(0, audio_device_->RegisterAudioCallback(NULL));
  }
#endif

  // repeat test but for kDefaultDevice
  EXPECT_EQ(0, audio_device_->SetRecordingDevice(MACRO_DEFAULT_DEVICE));
  EXPECT_EQ(0, audio_device_->RecordingIsAvailable(&available));
  if (available) {
    EXPECT_FALSE(audio_device_->RecordingIsInitialized());
    EXPECT_EQ(0, audio_device_->InitRecording());
    EXPECT_EQ(0, audio_device_->StartRecording());
    EXPECT_TRUE(audio_device_->Recording());
    EXPECT_EQ(0, audio_device_->RegisterAudioCallback(audio_transport_));
    EXPECT_EQ(0, audio_device_->StopRecording());
    EXPECT_FALSE(audio_device_->Recording());
  }

  // repeat test for all devices
  int16_t no_devices = audio_device_->RecordingDevices();
  for (int i = 0; i < no_devices; i++) {
    EXPECT_EQ(0, audio_device_->SetRecordingDevice(i));
    EXPECT_EQ(0, audio_device_->RecordingIsAvailable(&available));
    if (available) {
      EXPECT_FALSE(audio_device_->RecordingIsInitialized());
      EXPECT_EQ(0, audio_device_->InitRecording());
      EXPECT_EQ(0, audio_device_->StartRecording());
      EXPECT_TRUE(audio_device_->Recording());
      EXPECT_EQ(0, audio_device_->RegisterAudioCallback(audio_transport_));
      EXPECT_EQ(0, audio_device_->StopRecording());
      EXPECT_FALSE(audio_device_->Recording());
    }
  }
}

#if defined(_WIN32) && !defined(WEBRTC_WINDOWS_CORE_AUDIO_BUILD)
TEST_F(AudioDeviceAPITest, SetAndGetWaveOutVolume) {
  uint32_t vol(0);
  // NOTE 1: Windows Wave only!
  // NOTE 2: It seems like the waveOutSetVolume API returns
  // MMSYSERR_NOTSUPPORTED on some Vista machines!
  const uint16_t maxVol(0xFFFF);
  uint16_t volL, volR;

  CheckInitialPlayoutStates();

  // make dummy test to see if this API is supported
  int32_t works = audio_device_->SetWaveOutVolume(vol, vol);
  WARNING(works == 0);

  if (works == 0)
  {
    // set volume without open playout device
    for (vol = 0; vol <= maxVol; vol += (maxVol/5))
    {
      EXPECT_EQ(0, audio_device_->SetWaveOutVolume(vol, vol));
      EXPECT_EQ(0, audio_device_->WaveOutVolume(volL, volR));
      EXPECT_TRUE((volL == vol) && (volR == vol));
    }

    // repeat test but this time with an open (default) output device
    EXPECT_EQ(0, audio_device_->SetPlayoutDevice(
        AudioDeviceModule::kDefaultDevice));
    EXPECT_EQ(0, audio_device_->InitPlayout());
    EXPECT_TRUE(audio_device_->PlayoutIsInitialized());
    for (vol = 0; vol <= maxVol; vol += (maxVol/5))
    {
      EXPECT_EQ(0, audio_device_->SetWaveOutVolume(vol, vol));
      EXPECT_EQ(0, audio_device_->WaveOutVolume(volL, volR));
      EXPECT_TRUE((volL == vol) && (volR == vol));
    }

    // as above but while playout is active
    EXPECT_EQ(0, audio_device_->StartPlayout());
    EXPECT_TRUE(audio_device_->Playing());
    for (vol = 0; vol <= maxVol; vol += (maxVol/5))
    {
      EXPECT_EQ(0, audio_device_->SetWaveOutVolume(vol, vol));
      EXPECT_EQ(0, audio_device_->WaveOutVolume(volL, volR));
      EXPECT_TRUE((volL == vol) && (volR == vol));
    }
  }

  EXPECT_EQ(0, audio_device_->StopPlayout());
  EXPECT_FALSE(audio_device_->Playing());
}
#endif  // defined(_WIN32) && !defined(WEBRTC_WINDOWS_CORE_AUDIO_BUILD)

TEST_F(AudioDeviceAPITest, SpeakerIsAvailable) {
  bool available;
  CheckInitialPlayoutStates();

#ifdef _WIN32
  // check the kDefaultCommunicationDevice
  EXPECT_TRUE(audio_device_->SetPlayoutDevice(
          AudioDeviceModule::kDefaultCommunicationDevice) == 0);
  EXPECT_EQ(0, audio_device_->SpeakerIsAvailable(&available));
  // check for availability should not lead to initialization
  EXPECT_FALSE(audio_device_->SpeakerIsInitialized());
#endif

  // check the kDefaultDevice
  EXPECT_EQ(0, audio_device_->SetPlayoutDevice(MACRO_DEFAULT_DEVICE));
  EXPECT_EQ(0, audio_device_->SpeakerIsAvailable(&available));
  EXPECT_FALSE(audio_device_->SpeakerIsInitialized());

  // check all availiable devices
  int16_t no_devices = audio_device_->PlayoutDevices();
  for (int i = 0; i < no_devices; i++) {
    EXPECT_EQ(0, audio_device_->SetPlayoutDevice(i));
    EXPECT_EQ(0, audio_device_->SpeakerIsAvailable(&available));
    EXPECT_FALSE(audio_device_->SpeakerIsInitialized());
  }
}

TEST_F(AudioDeviceAPITest, InitSpeaker) {
  // NOTE: By calling Terminate (in TearDown) followed by Init (in SetUp) we
  // ensure that any existing output mixer handle is set to NULL.
  // The mixer handle is closed and reopened again for each call to
  // SetPlayoutDevice.
  CheckInitialPlayoutStates();

  // kDefaultCommunicationDevice
  EXPECT_EQ(0, audio_device_->SetPlayoutDevice(
      MACRO_DEFAULT_COMMUNICATION_DEVICE));
  bool available;
  EXPECT_EQ(0, audio_device_->SpeakerIsAvailable(&available));
  if (available) {
    EXPECT_EQ(0, audio_device_->InitSpeaker());
  }

  // fail tests
  EXPECT_EQ(0, audio_device_->PlayoutIsAvailable(&available));
  if (available) {
    EXPECT_EQ(0, audio_device_->InitPlayout());
    EXPECT_EQ(0, audio_device_->StartPlayout());
    EXPECT_EQ(-1, audio_device_->InitSpeaker());
    EXPECT_EQ(0, audio_device_->StopPlayout());
  }

  // kDefaultDevice
  EXPECT_EQ(0, audio_device_->SetPlayoutDevice(MACRO_DEFAULT_DEVICE));
  EXPECT_EQ(0, audio_device_->SpeakerIsAvailable(&available));
  if (available) {
    EXPECT_EQ(0, audio_device_->InitSpeaker());
  }

  // repeat test for all devices
  int16_t no_devices = audio_device_->PlayoutDevices();
  for (int i = 0; i < no_devices; i++) {
    EXPECT_EQ(0, audio_device_->SetPlayoutDevice(i));
    EXPECT_EQ(0, audio_device_->SpeakerIsAvailable(&available));
    if (available) {
      EXPECT_EQ(0, audio_device_->InitSpeaker());
    }
  }
}

TEST_F(AudioDeviceAPITest, MicrophoneIsAvailable) {
  CheckInitialRecordingStates();
  bool available;
#ifdef _WIN32
  // check the kDefaultCommunicationDevice
  EXPECT_TRUE(audio_device_->SetRecordingDevice(
          AudioDeviceModule::kDefaultCommunicationDevice) == 0);
  EXPECT_EQ(0, audio_device_->MicrophoneIsAvailable(&available));
  // check for availability should not lead to initialization
  EXPECT_FALSE(audio_device_->MicrophoneIsInitialized());
#endif

  // check the kDefaultDevice
  EXPECT_EQ(0, audio_device_->SetRecordingDevice(MACRO_DEFAULT_DEVICE));
  EXPECT_EQ(0, audio_device_->MicrophoneIsAvailable(&available));
  EXPECT_FALSE(audio_device_->MicrophoneIsInitialized());

  // check all availiable devices
  int16_t no_devices = audio_device_->RecordingDevices();
  for (int i = 0; i < no_devices; i++) {
    EXPECT_EQ(0, audio_device_->SetRecordingDevice(i));
    EXPECT_EQ(0, audio_device_->MicrophoneIsAvailable(&available));
    EXPECT_FALSE(audio_device_->MicrophoneIsInitialized());
  }
}

TEST_F(AudioDeviceAPITest, InitMicrophone) {
  // NOTE: By calling Terminate (in TearDown) followed by Init (in SetUp) we
  // ensure that any existing output mixer handle is set to NULL.
  // The mixer handle is closed and reopened again for each call to
  // SetRecordingDevice.
  CheckInitialRecordingStates();

  // kDefaultCommunicationDevice
  EXPECT_EQ(0,
      audio_device_->SetRecordingDevice(MACRO_DEFAULT_COMMUNICATION_DEVICE));
  bool available;
  EXPECT_EQ(0, audio_device_->MicrophoneIsAvailable(&available));
  if (available) {
    EXPECT_EQ(0, audio_device_->InitMicrophone());
  }

  // fail tests
  EXPECT_EQ(0, audio_device_->RecordingIsAvailable(&available));
  if (available) {
    EXPECT_EQ(0, audio_device_->InitRecording());
    EXPECT_EQ(0, audio_device_->StartRecording());
    EXPECT_EQ(-1, audio_device_->InitMicrophone());
    EXPECT_EQ(0, audio_device_->StopRecording());
  }

  // kDefaultDevice
  EXPECT_EQ(0, audio_device_->SetRecordingDevice(MACRO_DEFAULT_DEVICE));
  EXPECT_EQ(0, audio_device_->MicrophoneIsAvailable(&available));
  if (available) {
    EXPECT_EQ(0, audio_device_->InitMicrophone());
  }

  // repeat test for all devices
  int16_t no_devices = audio_device_->RecordingDevices();
  for (int i = 0; i < no_devices; i++) {
    EXPECT_EQ(0, audio_device_->SetRecordingDevice(i));
    EXPECT_EQ(0, audio_device_->MicrophoneIsAvailable(&available));
    if (available) {
      EXPECT_EQ(0, audio_device_->InitMicrophone());
    }
  }
}

TEST_F(AudioDeviceAPITest, SpeakerVolumeIsAvailable) {
  CheckInitialPlayoutStates();
  bool available;

#ifdef _WIN32
  // check the kDefaultCommunicationDevice
  EXPECT_TRUE(audio_device_->SetPlayoutDevice(
          AudioDeviceModule::kDefaultCommunicationDevice) == 0);
  EXPECT_EQ(0, audio_device_->SpeakerVolumeIsAvailable(&available));
  // check for availability should not lead to initialization
  EXPECT_FALSE(audio_device_->SpeakerIsInitialized());
#endif

  // check the kDefaultDevice
  EXPECT_EQ(0, audio_device_->SetPlayoutDevice(MACRO_DEFAULT_DEVICE));
  EXPECT_EQ(0, audio_device_->SpeakerVolumeIsAvailable(&available));
  EXPECT_FALSE(audio_device_->SpeakerIsInitialized());

  // check all availiable devices
  int16_t no_devices = audio_device_->PlayoutDevices();
  for (int i = 0; i < no_devices; i++) {
    EXPECT_EQ(0, audio_device_->SetPlayoutDevice(i));
    EXPECT_EQ(0, audio_device_->SpeakerVolumeIsAvailable(&available));
    EXPECT_FALSE(audio_device_->SpeakerIsInitialized());
  }
}

// Tests the following methods:
// SetSpeakerVolume
// SpeakerVolume
// MaxSpeakerVolume
// MinSpeakerVolume
// NOTE: Disabled on mac due to issue 257.
#ifndef WEBRTC_MAC
TEST_F(AudioDeviceAPITest, SpeakerVolumeTests) {
  uint32_t vol(0);
  uint32_t volume(0);
  uint32_t maxVolume(0);
  uint32_t minVolume(0);
  uint16_t stepSize(0);
  bool available;
  CheckInitialPlayoutStates();

  // fail tests
  EXPECT_EQ(-1, audio_device_->SetSpeakerVolume(0));
  // speaker must be initialized first
  EXPECT_EQ(-1, audio_device_->SpeakerVolume(&volume));
  EXPECT_EQ(-1, audio_device_->MaxSpeakerVolume(&maxVolume));
  EXPECT_EQ(-1, audio_device_->MinSpeakerVolume(&minVolume));
  EXPECT_EQ(-1, audio_device_->SpeakerVolumeStepSize(&stepSize));

#if defined(_WIN32) && !defined(WEBRTC_WINDOWS_CORE_AUDIO_BUILD)
  // test for warning (can e.g. happen on Vista with Wave API)
  EXPECT_EQ(0,
            audio_device_->SetPlayoutDevice(AudioDeviceModule::kDefaultDevice));
  EXPECT_EQ(0, audio_device_->SpeakerVolumeIsAvailable(&available));
  if (available) {
    EXPECT_EQ(0, audio_device_->InitSpeaker());
    EXPECT_EQ(0, audio_device_->SetSpeakerVolume(19001));
    EXPECT_EQ(0, audio_device_->SpeakerVolume(&volume));
    WARNING(volume == 19001);
  }
#endif

#ifdef _WIN32
  // use kDefaultCommunicationDevice and modify/retrieve the volume
  EXPECT_TRUE(audio_device_->SetPlayoutDevice(
          AudioDeviceModule::kDefaultCommunicationDevice) == 0);
  EXPECT_EQ(0, audio_device_->SpeakerVolumeIsAvailable(&available));
  if (available) {
    EXPECT_EQ(0, audio_device_->InitSpeaker());
    EXPECT_EQ(0, audio_device_->MaxSpeakerVolume(&maxVolume));
    EXPECT_EQ(0, audio_device_->MinSpeakerVolume(&minVolume));
    EXPECT_EQ(0, audio_device_->SpeakerVolumeStepSize(&stepSize));
    for (vol = minVolume; vol < (int)maxVolume; vol += 20*stepSize) {
      EXPECT_EQ(0, audio_device_->SetSpeakerVolume(vol));
      EXPECT_EQ(0, audio_device_->SpeakerVolume(&volume));
      CheckVolume(volume, vol);
    }
  }
#endif

  // use kDefaultDevice and modify/retrieve the volume
  EXPECT_EQ(0, audio_device_->SetPlayoutDevice(MACRO_DEFAULT_DEVICE));
  EXPECT_EQ(0, audio_device_->SpeakerVolumeIsAvailable(&available));
  if (available) {
    EXPECT_EQ(0, audio_device_->InitSpeaker());
    EXPECT_EQ(0, audio_device_->MaxSpeakerVolume(&maxVolume));
    EXPECT_EQ(0, audio_device_->MinSpeakerVolume(&minVolume));
    EXPECT_EQ(0, audio_device_->SpeakerVolumeStepSize(&stepSize));
    uint32_t step = (maxVolume - minVolume) / 10;
    step = (step < stepSize ? stepSize : step);
    for (vol = minVolume; vol <= maxVolume; vol += step) {
      EXPECT_EQ(0, audio_device_->SetSpeakerVolume(vol));
      EXPECT_EQ(0, audio_device_->SpeakerVolume(&volume));
      CheckVolume(volume, vol);
    }
  }

  // use all (indexed) devices and modify/retrieve the volume
  int16_t no_devices = audio_device_->PlayoutDevices();
  for (int i = 0; i < no_devices; i++) {
    EXPECT_EQ(0, audio_device_->SetPlayoutDevice(i));
    EXPECT_EQ(0, audio_device_->SpeakerVolumeIsAvailable(&available));
    if (available) {
      EXPECT_EQ(0, audio_device_->InitSpeaker());
      EXPECT_EQ(0, audio_device_->MaxSpeakerVolume(&maxVolume));
      EXPECT_EQ(0, audio_device_->MinSpeakerVolume(&minVolume));
      EXPECT_EQ(0, audio_device_->SpeakerVolumeStepSize(&stepSize));
      uint32_t step = (maxVolume - minVolume) / 10;
      step = (step < stepSize ? stepSize : step);
      for (vol = minVolume; vol <= maxVolume; vol += step) {
        EXPECT_EQ(0, audio_device_->SetSpeakerVolume(vol));
        EXPECT_EQ(0, audio_device_->SpeakerVolume(&volume));
        CheckVolume(volume, vol);
      }
    }
  }

  // restore reasonable level
  EXPECT_EQ(0, audio_device_->SetPlayoutDevice(MACRO_DEFAULT_DEVICE));
  EXPECT_EQ(0, audio_device_->SpeakerVolumeIsAvailable(&available));
  if (available) {
    EXPECT_EQ(0, audio_device_->InitSpeaker());
    EXPECT_EQ(0, audio_device_->MaxSpeakerVolume(&maxVolume));
    EXPECT_TRUE(audio_device_->SetSpeakerVolume(maxVolume < 10 ?
        maxVolume/3 : maxVolume/10) == 0);
  }
}
#endif  // !WEBRTC_MAC

TEST_F(AudioDeviceAPITest, AGC) {
  // NOTE: The AGC API only enables/disables the AGC. To ensure that it will
  // have an effect, use it in combination with MicrophoneVolumeIsAvailable.
  CheckInitialRecordingStates();
  EXPECT_FALSE(audio_device_->AGC());

  // set/get tests
  EXPECT_EQ(0, audio_device_->SetAGC(true));
  EXPECT_TRUE(audio_device_->AGC());
  EXPECT_EQ(0, audio_device_->SetAGC(false));
  EXPECT_FALSE(audio_device_->AGC());
}

TEST_F(AudioDeviceAPITest, MicrophoneVolumeIsAvailable) {
  CheckInitialRecordingStates();
  bool available;

#ifdef _WIN32
  // check the kDefaultCommunicationDevice
  EXPECT_TRUE(audio_device_->SetRecordingDevice(
          AudioDeviceModule::kDefaultCommunicationDevice) == 0);
  EXPECT_EQ(0, audio_device_->MicrophoneVolumeIsAvailable(&available));
  // check for availability should not lead to initialization
  EXPECT_FALSE(audio_device_->MicrophoneIsInitialized());
#endif

  // check the kDefaultDevice
  EXPECT_EQ(0, audio_device_->SetRecordingDevice(MACRO_DEFAULT_DEVICE));
  EXPECT_EQ(0, audio_device_->MicrophoneVolumeIsAvailable(&available));
  EXPECT_FALSE(audio_device_->MicrophoneIsInitialized());

  // check all availiable devices
  int16_t no_devices = audio_device_->RecordingDevices();
  for (int i = 0; i < no_devices; i++) {
    EXPECT_EQ(0, audio_device_->SetRecordingDevice(i));
    EXPECT_EQ(0, audio_device_->MicrophoneVolumeIsAvailable(&available));
    EXPECT_FALSE(audio_device_->MicrophoneIsInitialized());
  }
}

// Tests the methods:
// SetMicrophoneVolume
// MicrophoneVolume
// MaxMicrophoneVolume
// MinMicrophoneVolume
// NOTE: Disabled on mac due to issue 257.
#ifndef WEBRTC_MAC
TEST_F(AudioDeviceAPITest, MicrophoneVolumeTests) {
  uint32_t vol(0);
  uint32_t volume(0);
  uint32_t maxVolume(0);
  uint32_t minVolume(0);
  uint16_t stepSize(0);
  bool available;
  CheckInitialRecordingStates();

  // fail tests
  EXPECT_EQ(-1, audio_device_->SetMicrophoneVolume(0));
  // must be initialized first
  EXPECT_EQ(-1, audio_device_->MicrophoneVolume(&volume));
  EXPECT_EQ(-1, audio_device_->MaxMicrophoneVolume(&maxVolume));
  EXPECT_EQ(-1, audio_device_->MinMicrophoneVolume(&minVolume));
  EXPECT_EQ(-1, audio_device_->MicrophoneVolumeStepSize(&stepSize));

#if defined(_WIN32) && !defined(WEBRTC_WINDOWS_CORE_AUDIO_BUILD)
  // test for warning (can e.g. happen on Vista with Wave API)
  EXPECT_EQ(0, audio_device_->SetRecordingDevice(
      AudioDeviceModule::kDefaultDevice));
  EXPECT_EQ(0, audio_device_->MicrophoneVolumeIsAvailable(&available));
  if (available)
  {
    EXPECT_EQ(0, audio_device_->InitMicrophone());
    EXPECT_EQ(0, audio_device_->SetMicrophoneVolume(19001));
    EXPECT_EQ(0, audio_device_->MicrophoneVolume(&volume));
    WARNING(volume == 19001);
  }
#endif

#ifdef _WIN32
  // initialize kDefaultCommunicationDevice and modify/retrieve the volume
  EXPECT_TRUE(audio_device_->SetRecordingDevice(
          AudioDeviceModule::kDefaultCommunicationDevice) == 0);
  EXPECT_EQ(0, audio_device_->MicrophoneVolumeIsAvailable(&available));
  if (available)
  {
    EXPECT_EQ(0, audio_device_->InitMicrophone());
    EXPECT_EQ(0, audio_device_->MaxMicrophoneVolume(&maxVolume));
    EXPECT_EQ(0, audio_device_->MinMicrophoneVolume(&minVolume));
    EXPECT_EQ(0, audio_device_->MicrophoneVolumeStepSize(&stepSize));
    for (vol = minVolume; vol < (int)maxVolume; vol += 10*stepSize)
    {
      EXPECT_EQ(0, audio_device_->SetMicrophoneVolume(vol));
      EXPECT_EQ(0, audio_device_->MicrophoneVolume(&volume));
      CheckVolume(volume, vol);
    }
  }
#endif

  // reinitialize kDefaultDevice and modify/retrieve the volume
  EXPECT_EQ(0, audio_device_->SetRecordingDevice(MACRO_DEFAULT_DEVICE));
  EXPECT_EQ(0, audio_device_->MicrophoneVolumeIsAvailable(&available));
  if (available) {
    EXPECT_EQ(0, audio_device_->InitMicrophone());
    EXPECT_EQ(0, audio_device_->MaxMicrophoneVolume(&maxVolume));
    EXPECT_EQ(0, audio_device_->MinMicrophoneVolume(&minVolume));
    EXPECT_EQ(0, audio_device_->MicrophoneVolumeStepSize(&stepSize));
    for (vol = minVolume; vol < maxVolume; vol += 10 * stepSize) {
      EXPECT_EQ(0, audio_device_->SetMicrophoneVolume(vol));
      EXPECT_EQ(0, audio_device_->MicrophoneVolume(&volume));
      CheckVolume(volume, vol);
    }
  }

  // use all (indexed) devices and modify/retrieve the volume
  int16_t no_devices = audio_device_->RecordingDevices();
  for (int i = 0; i < no_devices; i++) {
    EXPECT_EQ(0, audio_device_->SetRecordingDevice(i));
    EXPECT_EQ(0, audio_device_->MicrophoneVolumeIsAvailable(&available));
    if (available) {
      EXPECT_EQ(0, audio_device_->InitMicrophone());
      EXPECT_EQ(0, audio_device_->MaxMicrophoneVolume(&maxVolume));
      EXPECT_EQ(0, audio_device_->MinMicrophoneVolume(&minVolume));
      EXPECT_EQ(0, audio_device_->MicrophoneVolumeStepSize(&stepSize));
      for (vol = minVolume; vol < maxVolume; vol += 20 * stepSize) {
        EXPECT_EQ(0, audio_device_->SetMicrophoneVolume(vol));
        EXPECT_EQ(0, audio_device_->MicrophoneVolume(&volume));
        CheckVolume(volume, vol);
      }
    }
  }

  // restore reasonable level
  EXPECT_EQ(0, audio_device_->SetRecordingDevice(MACRO_DEFAULT_DEVICE));
  EXPECT_EQ(0, audio_device_->MicrophoneVolumeIsAvailable(&available));
  if (available) {
    EXPECT_EQ(0, audio_device_->InitMicrophone());
    EXPECT_EQ(0, audio_device_->MaxMicrophoneVolume(&maxVolume));
    EXPECT_EQ(0, audio_device_->SetMicrophoneVolume(maxVolume/10));
  }
}
#endif  // !WEBRTC_MAC

TEST_F(AudioDeviceAPITest, SpeakerMuteIsAvailable) {
  bool available;
  CheckInitialPlayoutStates();
#ifdef _WIN32
  // check the kDefaultCommunicationDevice
  EXPECT_TRUE(audio_device_->SetPlayoutDevice(
          AudioDeviceModule::kDefaultCommunicationDevice) == 0);
  EXPECT_EQ(0, audio_device_->SpeakerMuteIsAvailable(&available));
  // check for availability should not lead to initialization
  EXPECT_FALSE(audio_device_->SpeakerIsInitialized());
#endif

  // check the kDefaultDevice
  EXPECT_EQ(0, audio_device_->SetPlayoutDevice(MACRO_DEFAULT_DEVICE));
  EXPECT_EQ(0, audio_device_->SpeakerMuteIsAvailable(&available));
  EXPECT_FALSE(audio_device_->SpeakerIsInitialized());

  // check all availiable devices
  int16_t no_devices = audio_device_->PlayoutDevices();
  for (int i = 0; i < no_devices; i++) {
    EXPECT_EQ(0, audio_device_->SetPlayoutDevice(i));
    EXPECT_EQ(0, audio_device_->SpeakerMuteIsAvailable(&available));
    EXPECT_FALSE(audio_device_->SpeakerIsInitialized());
  }
}

TEST_F(AudioDeviceAPITest, MicrophoneMuteIsAvailable) {
  bool available;
  CheckInitialRecordingStates();
#ifdef _WIN32
  // check the kDefaultCommunicationDevice
  EXPECT_TRUE(audio_device_->SetRecordingDevice(
          AudioDeviceModule::kDefaultCommunicationDevice) == 0);
  EXPECT_EQ(0, audio_device_->MicrophoneMuteIsAvailable(&available));
  // check for availability should not lead to initialization
#endif
  EXPECT_FALSE(audio_device_->MicrophoneIsInitialized());

  // check the kDefaultDevice
  EXPECT_EQ(0, audio_device_->SetRecordingDevice(MACRO_DEFAULT_DEVICE));
  EXPECT_EQ(0, audio_device_->MicrophoneMuteIsAvailable(&available));
  EXPECT_FALSE(audio_device_->MicrophoneIsInitialized());

  // check all availiable devices
  int16_t no_devices = audio_device_->RecordingDevices();
  for (int i = 0; i < no_devices; i++) {
    EXPECT_EQ(0, audio_device_->SetRecordingDevice(i));
    EXPECT_EQ(0, audio_device_->MicrophoneMuteIsAvailable(&available));
    EXPECT_FALSE(audio_device_->MicrophoneIsInitialized());
  }
}

TEST_F(AudioDeviceAPITest, MicrophoneBoostIsAvailable) {
  bool available;
  CheckInitialRecordingStates();
#ifdef _WIN32
  // check the kDefaultCommunicationDevice
  EXPECT_TRUE(audio_device_->SetRecordingDevice(
          AudioDeviceModule::kDefaultCommunicationDevice) == 0);
  EXPECT_EQ(0, audio_device_->MicrophoneBoostIsAvailable(&available));
  // check for availability should not lead to initialization
  EXPECT_FALSE(audio_device_->MicrophoneIsInitialized());
#endif

  // check the kDefaultDevice
  EXPECT_EQ(0, audio_device_->SetRecordingDevice(MACRO_DEFAULT_DEVICE));
  EXPECT_EQ(0, audio_device_->MicrophoneBoostIsAvailable(&available));
  EXPECT_FALSE(audio_device_->MicrophoneIsInitialized());

  // check all availiable devices
  int16_t no_devices = audio_device_->RecordingDevices();
  for (int i = 0; i < no_devices; i++) {
    EXPECT_EQ(0, audio_device_->SetRecordingDevice(i));
    EXPECT_EQ(0, audio_device_->MicrophoneBoostIsAvailable(&available));
    EXPECT_FALSE(audio_device_->MicrophoneIsInitialized());
  }
}

TEST_F(AudioDeviceAPITest, SpeakerMuteTests) {
  bool available;
  bool enabled;
  CheckInitialPlayoutStates();
  // fail tests
  EXPECT_EQ(-1, audio_device_->SetSpeakerMute(true));
  // requires initialization
  EXPECT_EQ(-1, audio_device_->SpeakerMute(&enabled));

#ifdef _WIN32
  // initialize kDefaultCommunicationDevice and modify/retrieve the mute state
  EXPECT_EQ(0, audio_device_->SetPlayoutDevice(
      AudioDeviceModule::kDefaultCommunicationDevice));
  EXPECT_EQ(0, audio_device_->SpeakerMuteIsAvailable(&available));
  if (available)
  {
    EXPECT_EQ(0, audio_device_->InitSpeaker());
    EXPECT_EQ(0, audio_device_->SetSpeakerMute(true));
    EXPECT_EQ(0, audio_device_->SpeakerMute(&enabled));
    EXPECT_TRUE(enabled);
    EXPECT_EQ(0, audio_device_->SetSpeakerMute(false));
    EXPECT_EQ(0, audio_device_->SpeakerMute(&enabled));
    EXPECT_FALSE(enabled);
  }
#endif

  // reinitialize kDefaultDevice and modify/retrieve the mute state
  EXPECT_EQ(0, audio_device_->SetPlayoutDevice(MACRO_DEFAULT_DEVICE));
  EXPECT_EQ(0, audio_device_->SpeakerMuteIsAvailable(&available));
  if (available) {
    EXPECT_EQ(0, audio_device_->InitSpeaker());
    EXPECT_EQ(0, audio_device_->SetSpeakerMute(true));
    EXPECT_EQ(0, audio_device_->SpeakerMute(&enabled));
    EXPECT_TRUE(enabled);
    EXPECT_EQ(0, audio_device_->SetSpeakerMute(false));
    EXPECT_EQ(0, audio_device_->SpeakerMute(&enabled));
    EXPECT_FALSE(enabled);
  }

  // reinitialize the default device (0) and modify/retrieve the mute state
  EXPECT_EQ(0, audio_device_->SetPlayoutDevice(0));
  EXPECT_EQ(0, audio_device_->SpeakerMuteIsAvailable(&available));
  if (available) {
    EXPECT_EQ(0, audio_device_->InitSpeaker());
    EXPECT_EQ(0, audio_device_->SetSpeakerMute(true));
    EXPECT_EQ(0, audio_device_->SpeakerMute(&enabled));
    EXPECT_TRUE(enabled);
    EXPECT_EQ(0, audio_device_->SetSpeakerMute(false));
    EXPECT_EQ(0, audio_device_->SpeakerMute(&enabled));
    EXPECT_FALSE(enabled);
  }
}

TEST_F(AudioDeviceAPITest, MicrophoneMuteTests) {
  CheckInitialRecordingStates();

  // fail tests
  EXPECT_EQ(-1, audio_device_->SetMicrophoneMute(true));
  // requires initialization
  bool available;
  bool enabled;
  EXPECT_EQ(-1, audio_device_->MicrophoneMute(&enabled));

#ifdef _WIN32
  // initialize kDefaultCommunicationDevice and modify/retrieve the mute
  EXPECT_TRUE(audio_device_->SetRecordingDevice(
          AudioDeviceModule::kDefaultCommunicationDevice) == 0);
  EXPECT_EQ(0, audio_device_->MicrophoneMuteIsAvailable(&available));
  if (available)
  {
    EXPECT_EQ(0, audio_device_->InitMicrophone());
    EXPECT_EQ(0, audio_device_->SetMicrophoneMute(true));
    EXPECT_EQ(0, audio_device_->MicrophoneMute(&enabled));
    EXPECT_TRUE(enabled);
    EXPECT_EQ(0, audio_device_->SetMicrophoneMute(false));
    EXPECT_EQ(0, audio_device_->MicrophoneMute(&enabled));
    EXPECT_FALSE(enabled);
  }
#endif

  // reinitialize kDefaultDevice and modify/retrieve the mute
  EXPECT_EQ(0, audio_device_->SetRecordingDevice(MACRO_DEFAULT_DEVICE));
  EXPECT_EQ(0, audio_device_->MicrophoneMuteIsAvailable(&available));
  if (available) {
    EXPECT_EQ(0, audio_device_->InitMicrophone());
    EXPECT_EQ(0, audio_device_->SetMicrophoneMute(true));
    EXPECT_EQ(0, audio_device_->MicrophoneMute(&enabled));
    EXPECT_TRUE(enabled);
    EXPECT_EQ(0, audio_device_->SetMicrophoneMute(false));
    EXPECT_EQ(0, audio_device_->MicrophoneMute(&enabled));
    EXPECT_FALSE(enabled);
  }

  // reinitialize the default device (0) and modify/retrieve the Mute
  EXPECT_EQ(0, audio_device_->SetRecordingDevice(0));
  EXPECT_EQ(0, audio_device_->MicrophoneMuteIsAvailable(&available));
  if (available) {
    EXPECT_EQ(0, audio_device_->InitMicrophone());
    EXPECT_EQ(0, audio_device_->SetMicrophoneMute(true));
    EXPECT_EQ(0, audio_device_->MicrophoneMute(&enabled));
    EXPECT_TRUE(enabled);
    EXPECT_EQ(0, audio_device_->SetMicrophoneMute(false));
    EXPECT_EQ(0, audio_device_->MicrophoneMute(&enabled));
    EXPECT_FALSE(enabled);
  }
}

TEST_F(AudioDeviceAPITest, MicrophoneBoostTests) {
  bool available;
  bool enabled;
  CheckInitialRecordingStates();

  // fail tests
  EXPECT_EQ(-1, audio_device_->SetMicrophoneBoost(true));
  // requires initialization
  EXPECT_EQ(-1, audio_device_->MicrophoneBoost(&enabled));

#ifdef _WIN32
  // initialize kDefaultCommunicationDevice and modify/retrieve the boost
  EXPECT_TRUE(audio_device_->SetRecordingDevice(
          AudioDeviceModule::kDefaultCommunicationDevice) == 0);
  EXPECT_EQ(0, audio_device_->MicrophoneBoostIsAvailable(&available));
  if (available)
  {
    EXPECT_EQ(0, audio_device_->InitMicrophone());
    EXPECT_EQ(0, audio_device_->SetMicrophoneBoost(true));
    EXPECT_EQ(0, audio_device_->MicrophoneBoost(&enabled));
    EXPECT_TRUE(enabled);
    EXPECT_EQ(0, audio_device_->SetMicrophoneBoost(false));
    EXPECT_EQ(0, audio_device_->MicrophoneBoost(&enabled));
    EXPECT_FALSE(enabled);
  }
#endif

  // reinitialize kDefaultDevice and modify/retrieve the boost
  EXPECT_EQ(0, audio_device_->SetRecordingDevice(MACRO_DEFAULT_DEVICE));
  EXPECT_EQ(0, audio_device_->MicrophoneBoostIsAvailable(&available));
  if (available) {
    EXPECT_EQ(0, audio_device_->InitMicrophone());
    EXPECT_EQ(0, audio_device_->SetMicrophoneBoost(true));
    EXPECT_EQ(0, audio_device_->MicrophoneBoost(&enabled));
    EXPECT_TRUE(enabled);
    EXPECT_EQ(0, audio_device_->SetMicrophoneBoost(false));
    EXPECT_EQ(0, audio_device_->MicrophoneBoost(&enabled));
    EXPECT_FALSE(enabled);
  }

  // reinitialize the default device (0) and modify/retrieve the boost
  EXPECT_EQ(0, audio_device_->SetRecordingDevice(0));
  EXPECT_EQ(0, audio_device_->MicrophoneBoostIsAvailable(&available));
  if (available) {
    EXPECT_EQ(0, audio_device_->InitMicrophone());
    EXPECT_EQ(0, audio_device_->SetMicrophoneBoost(true));
    EXPECT_EQ(0, audio_device_->MicrophoneBoost(&enabled));
    EXPECT_TRUE(enabled);
    EXPECT_EQ(0, audio_device_->SetMicrophoneBoost(false));
    EXPECT_EQ(0, audio_device_->MicrophoneBoost(&enabled));
    EXPECT_FALSE(enabled);
  }
}

TEST_F(AudioDeviceAPITest, StereoPlayoutTests) {
  CheckInitialPlayoutStates();

  // fail tests
  EXPECT_EQ(-1, audio_device_->InitPlayout());
  EXPECT_EQ(0, audio_device_->SetPlayoutDevice(
      MACRO_DEFAULT_COMMUNICATION_DEVICE));

  // TODO(kjellander): Fix so these tests pass on Mac.
#if !defined(WEBRTC_MAC)
  EXPECT_EQ(0, audio_device_->InitPlayout());
  EXPECT_TRUE(audio_device_->PlayoutIsInitialized());
  // must be performed before initialization
  EXPECT_EQ(-1, audio_device_->SetStereoPlayout(true));
#endif

  // ensure that we can set the stereo mode for playout
  EXPECT_EQ(0, audio_device_->StopPlayout());
  EXPECT_FALSE(audio_device_->PlayoutIsInitialized());

  // initialize kDefaultCommunicationDevice and modify/retrieve stereo support
  EXPECT_EQ(0, audio_device_->SetPlayoutDevice(
      MACRO_DEFAULT_COMMUNICATION_DEVICE));
  bool available;
  bool enabled;
  EXPECT_EQ(0, audio_device_->StereoPlayoutIsAvailable(&available));
  if (available) {
    EXPECT_EQ(0, audio_device_->SetStereoPlayout(true));
    EXPECT_EQ(0, audio_device_->StereoPlayout(&enabled));
    EXPECT_TRUE(enabled);
    EXPECT_EQ(0, audio_device_->SetStereoPlayout(false));
    EXPECT_EQ(0, audio_device_->StereoPlayout(&enabled));
    EXPECT_FALSE(enabled);
    EXPECT_EQ(0, audio_device_->SetStereoPlayout(true));
    EXPECT_EQ(0, audio_device_->StereoPlayout(&enabled));
    EXPECT_TRUE(enabled);
  }

  // initialize kDefaultDevice and modify/retrieve stereo support
  EXPECT_EQ(0, audio_device_->SetPlayoutDevice(MACRO_DEFAULT_DEVICE));
  EXPECT_EQ(0, audio_device_->StereoPlayoutIsAvailable(&available));
  if (available) {
    EXPECT_EQ(0, audio_device_->SetStereoPlayout(true));
    EXPECT_EQ(0, audio_device_->StereoPlayout(&enabled));
    EXPECT_TRUE(enabled);
    EXPECT_EQ(0, audio_device_->SetStereoPlayout(false));
    EXPECT_EQ(0, audio_device_->StereoPlayout(&enabled));
    EXPECT_FALSE(enabled);
    EXPECT_EQ(0, audio_device_->SetStereoPlayout(true));
    EXPECT_EQ(0, audio_device_->StereoPlayout(&enabled));
    EXPECT_TRUE(enabled);
  }

  // initialize default device (0) and modify/retrieve stereo support
  EXPECT_EQ(0, audio_device_->SetPlayoutDevice(0));
  EXPECT_EQ(0, audio_device_->StereoPlayoutIsAvailable(&available));
  if (available) {
    EXPECT_EQ(0, audio_device_->SetStereoPlayout(true));
    EXPECT_EQ(0, audio_device_->StereoPlayout(&enabled));
    EXPECT_TRUE(enabled);
    EXPECT_EQ(0, audio_device_->SetStereoPlayout(false));
    EXPECT_EQ(0, audio_device_->StereoPlayout(&enabled));
    EXPECT_FALSE(enabled);
    EXPECT_EQ(0, audio_device_->SetStereoPlayout(true));
    EXPECT_EQ(0, audio_device_->StereoPlayout(&enabled));
    EXPECT_TRUE(enabled);
  }
}

TEST_F(AudioDeviceAPITest, StereoRecordingTests) {
  CheckInitialRecordingStates();
  EXPECT_FALSE(audio_device_->Playing());

  // fail tests
  EXPECT_EQ(-1, audio_device_->InitRecording());
  EXPECT_EQ(0, audio_device_->SetRecordingDevice(
      MACRO_DEFAULT_COMMUNICATION_DEVICE));

  // TODO(kjellander): Fix so these tests pass on Mac.
#if !defined(WEBRTC_MAC)
  EXPECT_EQ(0, audio_device_->InitRecording());
  EXPECT_TRUE(audio_device_->RecordingIsInitialized());
  // must be performed before initialization
  EXPECT_EQ(-1, audio_device_->SetStereoRecording(true));
#endif
  // ensures that we can set the stereo mode for recording
  EXPECT_EQ(0, audio_device_->StopRecording());
  EXPECT_FALSE(audio_device_->RecordingIsInitialized());

  // initialize kDefaultCommunicationDevice and modify/retrieve stereo support
  EXPECT_EQ(0, audio_device_->SetRecordingDevice(
      MACRO_DEFAULT_COMMUNICATION_DEVICE));
  bool available;
  bool enabled;
  EXPECT_EQ(0, audio_device_->StereoRecordingIsAvailable(&available));
  if (available) {
    EXPECT_EQ(0, audio_device_->SetStereoRecording(true));
    EXPECT_EQ(0, audio_device_->StereoRecording(&enabled));
    EXPECT_TRUE(enabled);
    EXPECT_EQ(0, audio_device_->SetStereoRecording(false));
    EXPECT_EQ(0, audio_device_->StereoRecording(&enabled));
    EXPECT_FALSE(enabled);
  }

  // initialize kDefaultDevice and modify/retrieve stereo support
  EXPECT_EQ(0, audio_device_->SetRecordingDevice(MACRO_DEFAULT_DEVICE));
  EXPECT_EQ(0, audio_device_->StereoRecordingIsAvailable(&available));
  if (available) {
    EXPECT_EQ(0, audio_device_->SetStereoRecording(true));
    EXPECT_EQ(0, audio_device_->StereoRecording(&enabled));
    EXPECT_TRUE(enabled);
    EXPECT_EQ(0, audio_device_->SetStereoRecording(false));
    EXPECT_EQ(0, audio_device_->StereoRecording(&enabled));
    EXPECT_FALSE(enabled);
  }

  // initialize default device (0) and modify/retrieve stereo support
  EXPECT_EQ(0, audio_device_->SetRecordingDevice(0));
  EXPECT_EQ(0, audio_device_->StereoRecordingIsAvailable(&available));
  if (available) {
    EXPECT_EQ(0, audio_device_->SetStereoRecording(true));
    EXPECT_EQ(0, audio_device_->StereoRecording(&enabled));
    EXPECT_TRUE(enabled);
    EXPECT_EQ(0, audio_device_->SetStereoRecording(false));
    EXPECT_EQ(0, audio_device_->StereoRecording(&enabled));
    EXPECT_FALSE(enabled);
  }
}

TEST_F(AudioDeviceAPITest, RecordingChannelTests) {
  // the user in Win Core Audio
  AudioDeviceModule::ChannelType channelType(AudioDeviceModule::kChannelBoth);
  CheckInitialRecordingStates();
  EXPECT_FALSE(audio_device_->Playing());

  // fail tests
  EXPECT_EQ(0, audio_device_->SetStereoRecording(false));
  EXPECT_EQ(-1, audio_device_->SetRecordingChannel(
      AudioDeviceModule::kChannelBoth));

  // initialize kDefaultCommunicationDevice and modify/retrieve stereo support
  EXPECT_EQ(0, audio_device_->SetRecordingDevice(
      MACRO_DEFAULT_COMMUNICATION_DEVICE));
  bool available;
  EXPECT_EQ(0, audio_device_->StereoRecordingIsAvailable(&available));
  if (available) {
    EXPECT_EQ(0, audio_device_->SetStereoRecording(true));
    EXPECT_EQ(0, audio_device_->SetRecordingChannel(
        AudioDeviceModule::kChannelBoth));
    EXPECT_EQ(0, audio_device_->RecordingChannel(&channelType));
    EXPECT_EQ(AudioDeviceModule::kChannelBoth, channelType);
    EXPECT_EQ(0, audio_device_->SetRecordingChannel(
        AudioDeviceModule::kChannelLeft));
    EXPECT_EQ(0, audio_device_->RecordingChannel(&channelType));
    EXPECT_EQ(AudioDeviceModule::kChannelLeft, channelType);
    EXPECT_EQ(0, audio_device_->SetRecordingChannel(
        AudioDeviceModule::kChannelRight));
    EXPECT_EQ(0, audio_device_->RecordingChannel(&channelType));
    EXPECT_EQ(AudioDeviceModule::kChannelRight, channelType);
  }
}

TEST_F(AudioDeviceAPITest, PlayoutBufferTests) {
  AudioDeviceModule::BufferType bufferType;
  uint16_t sizeMS(0);

  CheckInitialPlayoutStates();
  EXPECT_EQ(0, audio_device_->PlayoutBuffer(&bufferType, &sizeMS));
#if defined(_WIN32) || defined(ANDROID) || defined(WEBRTC_IOS)
  EXPECT_EQ(AudioDeviceModule::kAdaptiveBufferSize, bufferType);
#else
  EXPECT_EQ(AudioDeviceModule::kFixedBufferSize, bufferType);
#endif

  // fail tests
  EXPECT_EQ(-1, audio_device_->InitPlayout());
  // must set device first
  EXPECT_EQ(0, audio_device_->SetPlayoutDevice(
      MACRO_DEFAULT_COMMUNICATION_DEVICE));

  // TODO(kjellander): Fix so these tests pass on Mac.
#if !defined(WEBRTC_MAC)
  EXPECT_EQ(0, audio_device_->InitPlayout());
  EXPECT_TRUE(audio_device_->PlayoutIsInitialized());
#endif
  EXPECT_TRUE(audio_device_->SetPlayoutBuffer(
      AudioDeviceModule::kAdaptiveBufferSize, 100) == -1);
  EXPECT_EQ(0, audio_device_->StopPlayout());
  EXPECT_TRUE(audio_device_->SetPlayoutBuffer(
      AudioDeviceModule::kFixedBufferSize, kAdmMinPlayoutBufferSizeMs-1) == -1);
  EXPECT_TRUE(audio_device_->SetPlayoutBuffer(
      AudioDeviceModule::kFixedBufferSize, kAdmMaxPlayoutBufferSizeMs+1) == -1);

  // bulk tests (all should be successful)
  EXPECT_FALSE(audio_device_->PlayoutIsInitialized());
#ifdef _WIN32
  EXPECT_EQ(0, audio_device_->SetPlayoutBuffer(
      AudioDeviceModule::kAdaptiveBufferSize, 0));
  EXPECT_EQ(0, audio_device_->PlayoutBuffer(&bufferType, &sizeMS));
  EXPECT_EQ(AudioDeviceModule::kAdaptiveBufferSize, bufferType);
  EXPECT_EQ(0, audio_device_->SetPlayoutBuffer(
      AudioDeviceModule::kAdaptiveBufferSize, 10000));
  EXPECT_EQ(0, audio_device_->PlayoutBuffer(&bufferType, &sizeMS));
  EXPECT_EQ(AudioDeviceModule::kAdaptiveBufferSize, bufferType);
#endif
#if defined(ANDROID) || defined(WEBRTC_IOS)
  EXPECT_EQ(-1,
            audio_device_->SetPlayoutBuffer(AudioDeviceModule::kFixedBufferSize,
                                          kAdmMinPlayoutBufferSizeMs));
#else
  EXPECT_EQ(0, audio_device_->SetPlayoutBuffer(
      AudioDeviceModule::kFixedBufferSize, kAdmMinPlayoutBufferSizeMs));
  EXPECT_EQ(0, audio_device_->PlayoutBuffer(&bufferType, &sizeMS));
  EXPECT_EQ(AudioDeviceModule::kFixedBufferSize, bufferType);
  EXPECT_EQ(kAdmMinPlayoutBufferSizeMs, sizeMS);
  EXPECT_EQ(0, audio_device_->SetPlayoutBuffer(
      AudioDeviceModule::kFixedBufferSize, kAdmMaxPlayoutBufferSizeMs));
  EXPECT_EQ(0, audio_device_->PlayoutBuffer(&bufferType, &sizeMS));
  EXPECT_EQ(AudioDeviceModule::kFixedBufferSize, bufferType);
  EXPECT_EQ(kAdmMaxPlayoutBufferSizeMs, sizeMS);
  EXPECT_EQ(0, audio_device_->SetPlayoutBuffer(
      AudioDeviceModule::kFixedBufferSize, 100));
  EXPECT_EQ(0, audio_device_->PlayoutBuffer(&bufferType, &sizeMS));
  EXPECT_EQ(AudioDeviceModule::kFixedBufferSize, bufferType);
  EXPECT_EQ(100, sizeMS);
#endif

#ifdef _WIN32
  // restore default
  EXPECT_EQ(0, audio_device_->SetPlayoutBuffer(
      AudioDeviceModule::kAdaptiveBufferSize, 0));
  EXPECT_EQ(0, audio_device_->PlayoutBuffer(&bufferType, &sizeMS));
#endif
}

TEST_F(AudioDeviceAPITest, PlayoutDelay) {
  // NOTE: this API is better tested in a functional test
  uint16_t sizeMS(0);
  CheckInitialPlayoutStates();
  // bulk tests
  EXPECT_EQ(0, audio_device_->PlayoutDelay(&sizeMS));
  EXPECT_EQ(0, audio_device_->PlayoutDelay(&sizeMS));
}

TEST_F(AudioDeviceAPITest, RecordingDelay) {
  // NOTE: this API is better tested in a functional test
  uint16_t sizeMS(0);
  CheckInitialRecordingStates();

  // bulk tests
  EXPECT_EQ(0, audio_device_->RecordingDelay(&sizeMS));
  EXPECT_EQ(0, audio_device_->RecordingDelay(&sizeMS));
}

TEST_F(AudioDeviceAPITest, CPULoad) {
  // NOTE: this API is better tested in a functional test
  uint16_t load(0);

  // bulk tests
#ifdef _WIN32
  EXPECT_EQ(0, audio_device_->CPULoad(&load));
  EXPECT_EQ(0, load);
#else
  EXPECT_EQ(-1, audio_device_->CPULoad(&load));
#endif
}

// TODO(kjellander): Fix flakiness causing failures on Windows.
// TODO(phoglund):  Fix flakiness causing failures on Linux.
#if !defined(_WIN32) && !defined(WEBRTC_LINUX)
TEST_F(AudioDeviceAPITest, StartAndStopRawOutputFileRecording) {
  // NOTE: this API is better tested in a functional test
  CheckInitialPlayoutStates();

  // fail tests
  EXPECT_EQ(-1, audio_device_->StartRawOutputFileRecording(NULL));

  // bulk tests
  EXPECT_EQ(0, audio_device_->StartRawOutputFileRecording(
      GetFilename("raw_output_not_playing.pcm")));
  EXPECT_EQ(0, audio_device_->StopRawOutputFileRecording());
  EXPECT_EQ(0, audio_device_->SetPlayoutDevice(
      MACRO_DEFAULT_COMMUNICATION_DEVICE));

  // TODO(kjellander): Fix so these tests pass on Mac.
#if !defined(WEBRTC_MAC)
  EXPECT_EQ(0, audio_device_->InitPlayout());
  EXPECT_EQ(0, audio_device_->StartPlayout());
#endif

  EXPECT_EQ(0, audio_device_->StartRawOutputFileRecording(
      GetFilename("raw_output_playing.pcm")));
  SleepMs(100);
  EXPECT_EQ(0, audio_device_->StopRawOutputFileRecording());
  EXPECT_EQ(0, audio_device_->StopPlayout());
  EXPECT_EQ(0, audio_device_->StartRawOutputFileRecording(
      GetFilename("raw_output_not_playing.pcm")));
  EXPECT_EQ(0, audio_device_->StopRawOutputFileRecording());

  // results after this test:
  //
  // - size of raw_output_not_playing.pcm shall be 0
  // - size of raw_output_playing.pcm shall be > 0
}

TEST_F(AudioDeviceAPITest, StartAndStopRawInputFileRecording) {
  // NOTE: this API is better tested in a functional test
  CheckInitialRecordingStates();
  EXPECT_FALSE(audio_device_->Playing());

  // fail tests
  EXPECT_EQ(-1, audio_device_->StartRawInputFileRecording(NULL));

  // bulk tests
  EXPECT_EQ(0, audio_device_->StartRawInputFileRecording(
      GetFilename("raw_input_not_recording.pcm")));
  EXPECT_EQ(0, audio_device_->StopRawInputFileRecording());
  EXPECT_EQ(0, audio_device_->SetRecordingDevice(MACRO_DEFAULT_DEVICE));

  // TODO(kjellander): Fix so these tests pass on Mac.
#if !defined(WEBRTC_MAC)
  EXPECT_EQ(0, audio_device_->InitRecording());
  EXPECT_EQ(0, audio_device_->StartRecording());
#endif
  EXPECT_EQ(0, audio_device_->StartRawInputFileRecording(
      GetFilename("raw_input_recording.pcm")));
  SleepMs(100);
  EXPECT_EQ(0, audio_device_->StopRawInputFileRecording());
  EXPECT_EQ(0, audio_device_->StopRecording());
  EXPECT_EQ(0, audio_device_->StartRawInputFileRecording(
      GetFilename("raw_input_not_recording.pcm")));
  EXPECT_EQ(0, audio_device_->StopRawInputFileRecording());

  // results after this test:
  //
  // - size of raw_input_not_recording.pcm shall be 0
  // - size of raw_input_not_recording.pcm shall be > 0
}
#endif  // !WIN32 && !WEBRTC_LINUX

TEST_F(AudioDeviceAPITest, RecordingSampleRate) {
  uint32_t sampleRate(0);

  // bulk tests
  EXPECT_EQ(0, audio_device_->RecordingSampleRate(&sampleRate));
#if defined(_WIN32) && !defined(WEBRTC_WINDOWS_CORE_AUDIO_BUILD)
  EXPECT_EQ(48000, sampleRate);
#elif defined(ANDROID)
  TEST_LOG("Recording sample rate is %u\n\n", sampleRate);
  EXPECT_TRUE((sampleRate == 44000) || (sampleRate == 16000));
#elif defined(WEBRTC_IOS)
  TEST_LOG("Recording sample rate is %u\n\n", sampleRate);
  EXPECT_TRUE((sampleRate == 44000) || (sampleRate == 16000) ||
              (sampleRate == 8000));
#endif

  // @TODO(xians) - add tests for all platforms here...
}

TEST_F(AudioDeviceAPITest, PlayoutSampleRate) {
  uint32_t sampleRate(0);

  // bulk tests
  EXPECT_EQ(0, audio_device_->PlayoutSampleRate(&sampleRate));
#if defined(_WIN32) && !defined(WEBRTC_WINDOWS_CORE_AUDIO_BUILD)
  EXPECT_EQ(48000, sampleRate);
#elif defined(ANDROID)
  TEST_LOG("Playout sample rate is %u\n\n", sampleRate);
  EXPECT_TRUE((sampleRate == 44000) || (sampleRate == 16000));
#elif defined(WEBRTC_IOS)
  TEST_LOG("Playout sample rate is %u\n\n", sampleRate);
  EXPECT_TRUE((sampleRate == 44000) || (sampleRate == 16000) ||
              (sampleRate == 8000));
#endif
}

TEST_F(AudioDeviceAPITest, ResetAudioDevice) {
  CheckInitialPlayoutStates();
  CheckInitialRecordingStates();
  EXPECT_EQ(0, audio_device_->SetPlayoutDevice(MACRO_DEFAULT_DEVICE));
  EXPECT_EQ(0, audio_device_->SetRecordingDevice(MACRO_DEFAULT_DEVICE));

#if defined(WEBRTC_IOS)
  // Not playing or recording, should just return 0
  EXPECT_EQ(0, audio_device_->ResetAudioDevice());

  EXPECT_EQ(0, audio_device_->InitRecording());
  EXPECT_EQ(0, audio_device_->StartRecording());
  EXPECT_EQ(0, audio_device_->InitPlayout());
  EXPECT_EQ(0, audio_device_->StartPlayout());
  for (int l=0; l<20; ++l)
  {
    TEST_LOG("Resetting sound device several time with pause %d ms\n", l);
    EXPECT_EQ(0, audio_device_->ResetAudioDevice());
    SleepMs(l);
  }
#else
  // Fail tests
  EXPECT_EQ(-1, audio_device_->ResetAudioDevice());

  // TODO(kjellander): Fix so these tests pass on Mac.
#if !defined(WEBRTC_MAC)
  EXPECT_EQ(0, audio_device_->InitRecording());
  EXPECT_EQ(0, audio_device_->StartRecording());
  EXPECT_EQ(0, audio_device_->InitPlayout());
  EXPECT_EQ(0, audio_device_->StartPlayout());
#endif
  EXPECT_EQ(-1, audio_device_->ResetAudioDevice());
#endif
  EXPECT_EQ(0, audio_device_->StopRecording());
  EXPECT_EQ(0, audio_device_->StopPlayout());
}

TEST_F(AudioDeviceAPITest, SetPlayoutSpeaker) {
  CheckInitialPlayoutStates();
  EXPECT_EQ(0, audio_device_->SetPlayoutDevice(MACRO_DEFAULT_DEVICE));

  bool loudspeakerOn(false);
#if defined(WEBRTC_IOS)
  // Not playing or recording, should just return a success
  EXPECT_EQ(0, audio_device_->SetLoudspeakerStatus(true));
  EXPECT_EQ(0, audio_device_->GetLoudspeakerStatus(loudspeakerOn));
  EXPECT_TRUE(loudspeakerOn);
  EXPECT_EQ(0, audio_device_->SetLoudspeakerStatus(false));
  EXPECT_EQ(0, audio_device_->GetLoudspeakerStatus(loudspeakerOn));
  EXPECT_FALSE(loudspeakerOn);

  EXPECT_EQ(0, audio_device_->InitPlayout());
  EXPECT_EQ(0, audio_device_->StartPlayout());
  EXPECT_EQ(0, audio_device_->SetLoudspeakerStatus(true));
  EXPECT_EQ(0, audio_device_->GetLoudspeakerStatus(loudspeakerOn));
  EXPECT_TRUE(loudspeakerOn);
  EXPECT_EQ(0, audio_device_->SetLoudspeakerStatus(false));
  EXPECT_EQ(0, audio_device_->GetLoudspeakerStatus(loudspeakerOn));
  EXPECT_FALSE(loudspeakerOn);

#else
  // Fail tests
  EXPECT_EQ(-1, audio_device_->SetLoudspeakerStatus(true));
  EXPECT_EQ(-1, audio_device_->SetLoudspeakerStatus(false));
  EXPECT_EQ(-1, audio_device_->SetLoudspeakerStatus(true));
  EXPECT_EQ(-1, audio_device_->SetLoudspeakerStatus(false));

  // TODO(kjellander): Fix so these tests pass on Mac.
#if !defined(WEBRTC_MAC)
  EXPECT_EQ(0, audio_device_->InitPlayout());
  EXPECT_EQ(0, audio_device_->StartPlayout());
#endif

  EXPECT_EQ(-1, audio_device_->GetLoudspeakerStatus(&loudspeakerOn));
#endif
  EXPECT_EQ(0, audio_device_->StopPlayout());
}

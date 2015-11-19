/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <list>
#include <numeric>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webrtc/base/criticalsection.h"
#include "webrtc/base/scoped_ptr.h"
#include "webrtc/modules/audio_device/android/ensure_initialized.h"
#include "webrtc/modules/audio_device/audio_device_impl.h"
#include "webrtc/modules/audio_device/include/audio_device.h"
#include "webrtc/system_wrappers/interface/clock.h"
#include "webrtc/system_wrappers/interface/event_wrapper.h"
#include "webrtc/system_wrappers/interface/scoped_refptr.h"
#include "webrtc/system_wrappers/interface/sleep.h"
#include "webrtc/test/testsupport/fileutils.h"

using std::cout;
using std::endl;
using ::testing::_;
using ::testing::AtLeast;
using ::testing::Gt;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::NotNull;
using ::testing::Return;
using ::testing::TestWithParam;

// #define ENABLE_DEBUG_PRINTF
#ifdef ENABLE_DEBUG_PRINTF
#define PRINTD(...) fprintf(stderr, __VA_ARGS__);
#else
#define PRINTD(...) ((void)0)
#endif
#define PRINT(...) fprintf(stderr, __VA_ARGS__);

namespace webrtc {

// Perform all tests for the different audio layers listed in this array.
// See the INSTANTIATE_TEST_CASE_P statement for details.
// TODO(henrika): the test framework supports both Java and OpenSL ES based
// audio backends but there are currently some issues (crashes) in the
// OpenSL ES implementation, hence it is not added to kAudioLayers yet.
static const AudioDeviceModule::AudioLayer kAudioLayers[] = {
    AudioDeviceModule::kAndroidJavaAudio
    /*, AudioDeviceModule::kAndroidOpenSLESAudio */};
// Number of callbacks (input or output) the tests waits for before we set
// an event indicating that the test was OK.
static const int kNumCallbacks = 10;
// Max amount of time we wait for an event to be set while counting callbacks.
static const int kTestTimeOutInMilliseconds = 10 * 1000;
// Average number of audio callbacks per second assuming 10ms packet size.
static const int kNumCallbacksPerSecond = 100;
// Play out a test file during this time (unit is in seconds).
static const int kFilePlayTimeInSec = 5;
// Fixed value for the recording delay using Java based audio backend.
// TODO(henrika): harmonize with OpenSL ES and look for possible improvements.
static const uint32_t kFixedRecordingDelay = 100;
static const int kBitsPerSample = 16;
static const int kBytesPerSample = kBitsPerSample / 8;
// Run the full-duplex test during this time (unit is in seconds).
// Note that first |kNumIgnoreFirstCallbacks| are ignored.
static const int kFullDuplexTimeInSec = 5;
// Wait for the callback sequence to stabilize by ignoring this amount of the
// initial callbacks (avoids initial FIFO access).
// Only used in the RunPlayoutAndRecordingInFullDuplex test.
static const int kNumIgnoreFirstCallbacks = 50;
// Sets the number of impulses per second in the latency test.
static const int kImpulseFrequencyInHz = 1;
// Length of round-trip latency measurements. Number of transmitted impulses
// is kImpulseFrequencyInHz * kMeasureLatencyTimeInSec - 1.
static const int kMeasureLatencyTimeInSec = 11;
// Utilized in round-trip latency measurements to avoid capturing noise samples.
static const int kImpulseThreshold = 500;
static const char kTag[] = "[..........] ";

enum TransportType {
  kPlayout = 0x1,
  kRecording = 0x2,
};

// Simple helper struct for device specific audio parameters.
struct AudioParameters {
  int playout_frames_per_buffer() const {
    return playout_sample_rate / 100;  // WebRTC uses 10 ms as buffer size.
  }
  int recording_frames_per_buffer() const {
    return recording_sample_rate / 100;
  }
  int playout_sample_rate;
  int recording_sample_rate;
  int playout_channels;
  int recording_channels;
};

// Interface for processing the audio stream. Real implementations can e.g.
// run audio in loopback, read audio from a file or perform latency
// measurements.
class AudioStreamInterface {
 public:
  virtual void Write(const void* source, int num_frames) = 0;
  virtual void Read(void* destination, int num_frames) = 0;
 protected:
  virtual ~AudioStreamInterface() {}
};

// Reads audio samples from a PCM file where the file is stored in memory at
// construction.
class FileAudioStream : public AudioStreamInterface {
 public:
  FileAudioStream(
      int num_callbacks, const std::string& file_name, int sample_rate)
      : file_size_in_bytes_(0),
        sample_rate_(sample_rate),
        file_pos_(0) {
    file_size_in_bytes_ = test::GetFileSize(file_name);
    sample_rate_ = sample_rate;
    EXPECT_GE(file_size_in_callbacks(), num_callbacks)
        << "Size of test file is not large enough to last during the test.";
    const int num_16bit_samples =
        test::GetFileSize(file_name) / kBytesPerSample;
    file_.reset(new int16_t[num_16bit_samples]);
    FILE* audio_file = fopen(file_name.c_str(), "rb");
    EXPECT_NE(audio_file, nullptr);
    int num_samples_read = fread(
        file_.get(), sizeof(int16_t), num_16bit_samples, audio_file);
    EXPECT_EQ(num_samples_read, num_16bit_samples);
    fclose(audio_file);
  }

  // AudioStreamInterface::Write() is not implemented.
  virtual void Write(const void* source, int num_frames) override {}

  // Read samples from file stored in memory (at construction) and copy
  // |num_frames| (<=> 10ms) to the |destination| byte buffer.
  virtual void Read(void* destination, int num_frames) override {
    memcpy(destination,
           static_cast<int16_t*> (&file_[file_pos_]),
           num_frames * sizeof(int16_t));
    file_pos_ += num_frames;
  }

  int file_size_in_seconds() const {
    return (file_size_in_bytes_ / (kBytesPerSample * sample_rate_));
  }
  int file_size_in_callbacks() const {
    return file_size_in_seconds() * kNumCallbacksPerSecond;
  }

 private:
  int file_size_in_bytes_;
  int sample_rate_;
  rtc::scoped_ptr<int16_t[]> file_;
  int file_pos_;
};

// Simple first in first out (FIFO) class that wraps a list of 16-bit audio
// buffers of fixed size and allows Write and Read operations. The idea is to
// store recorded audio buffers (using Write) and then read (using Read) these
// stored buffers with as short delay as possible when the audio layer needs
// data to play out. The number of buffers in the FIFO will stabilize under
// normal conditions since there will be a balance between Write and Read calls.
// The container is a std::list container and access is protected with a lock
// since both sides (playout and recording) are driven by its own thread.
class FifoAudioStream : public AudioStreamInterface {
 public:
  explicit FifoAudioStream(int frames_per_buffer)
      : frames_per_buffer_(frames_per_buffer),
        bytes_per_buffer_(frames_per_buffer_ * sizeof(int16_t)),
        fifo_(new AudioBufferList),
        largest_size_(0),
        total_written_elements_(0),
        write_count_(0) {
    EXPECT_NE(fifo_.get(), nullptr);
  }

  ~FifoAudioStream() {
    Flush();
    PRINTD("[%4.3f]\n", average_size());
  }

  // Allocate new memory, copy |num_frames| samples from |source| into memory
  // and add pointer to the memory location to end of the list.
  // Increases the size of the FIFO by one element.
  virtual void Write(const void* source, int num_frames) override {
    ASSERT_EQ(num_frames, frames_per_buffer_);
    PRINTD("+");
    if (write_count_++ < kNumIgnoreFirstCallbacks) {
      return;
    }
    int16_t* memory = new int16_t[frames_per_buffer_];
    memcpy(static_cast<int16_t*> (&memory[0]),
           source,
           bytes_per_buffer_);
    rtc::CritScope lock(&lock_);
    fifo_->push_back(memory);
    const int size = fifo_->size();
    if (size > largest_size_) {
      largest_size_ = size;
      PRINTD("(%d)", largest_size_);
    }
    total_written_elements_ += size;
  }

  // Read pointer to data buffer from front of list, copy |num_frames| of stored
  // data into |destination| and delete the utilized memory allocation.
  // Decreases the size of the FIFO by one element.
  virtual void Read(void* destination, int num_frames) override {
    ASSERT_EQ(num_frames, frames_per_buffer_);
    PRINTD("-");
    rtc::CritScope lock(&lock_);
    if (fifo_->empty()) {
      memset(destination, 0, bytes_per_buffer_);
    } else {
      int16_t* memory = fifo_->front();
      fifo_->pop_front();
      memcpy(destination,
             static_cast<int16_t*> (&memory[0]),
             bytes_per_buffer_);
      delete memory;
    }
  }

  int size() const {
    return fifo_->size();
  }

  int largest_size() const {
    return largest_size_;
  }

  int average_size() const {
    return (total_written_elements_ == 0) ? 0.0 : 0.5 + static_cast<float> (
      total_written_elements_) / (write_count_ - kNumIgnoreFirstCallbacks);
  }

 private:
  void Flush() {
    for (auto it = fifo_->begin(); it != fifo_->end(); ++it) {
      delete *it;
    }
    fifo_->clear();
  }

  using AudioBufferList = std::list<int16_t*>;
  rtc::CriticalSection lock_;
  const int frames_per_buffer_;
  const int bytes_per_buffer_;
  rtc::scoped_ptr<AudioBufferList> fifo_;
  int largest_size_;
  int total_written_elements_;
  int write_count_;
};

// Inserts periodic impulses and measures the latency between the time of
// transmission and time of receiving the same impulse.
// Usage requires a special hardware called Audio Loopback Dongle.
// See http://source.android.com/devices/audio/loopback.html for details.
class LatencyMeasuringAudioStream : public AudioStreamInterface {
 public:
  explicit LatencyMeasuringAudioStream(int frames_per_buffer)
      : clock_(Clock::GetRealTimeClock()),
        frames_per_buffer_(frames_per_buffer),
        bytes_per_buffer_(frames_per_buffer_ * sizeof(int16_t)),
        play_count_(0),
        rec_count_(0),
        pulse_time_(0) {
  }

  // Insert periodic impulses in first two samples of |destination|.
  virtual void Read(void* destination, int num_frames) override {
    ASSERT_EQ(num_frames, frames_per_buffer_);
    if (play_count_ == 0) {
      PRINT("[");
    }
    play_count_++;
    memset(destination, 0, bytes_per_buffer_);
    if (play_count_ % (kNumCallbacksPerSecond / kImpulseFrequencyInHz) == 0) {
      if (pulse_time_ == 0) {
        pulse_time_ = clock_->TimeInMilliseconds();
      }
      PRINT(".");
      const int16_t impulse = std::numeric_limits<int16_t>::max();
      int16_t* ptr16 = static_cast<int16_t*> (destination);
      for (int i = 0; i < 2; ++i) {
        *ptr16++ = impulse;
      }
    }
  }

  // Detect received impulses in |source|, derive time between transmission and
  // detection and add the calculated delay to list of latencies.
  virtual void Write(const void* source, int num_frames) override {
    ASSERT_EQ(num_frames, frames_per_buffer_);
    rec_count_++;
    if (pulse_time_ == 0) {
      // Avoid detection of new impulse response until a new impulse has
      // been transmitted (sets |pulse_time_| to value larger than zero).
      return;
    }
    const int16_t* ptr16 = static_cast<const int16_t*> (source);
    std::vector<int16_t> vec(ptr16, ptr16 + num_frames);
    // Find max value in the audio buffer.
    int max = *std::max_element(vec.begin(), vec.end());
    // Find index (element position in vector) of the max element.
    int index_of_max = std::distance(vec.begin(),
                                     std::find(vec.begin(), vec.end(),
                                     max));
    if (max > kImpulseThreshold) {
      PRINTD("(%d,%d)", max, index_of_max);
      int64_t now_time = clock_->TimeInMilliseconds();
      int extra_delay = IndexToMilliseconds(static_cast<double> (index_of_max));
      PRINTD("[%d]", static_cast<int> (now_time - pulse_time_));
      PRINTD("[%d]", extra_delay);
      // Total latency is the difference between transmit time and detection
      // tome plus the extra delay within the buffer in which we detected the
      // received impulse. It is transmitted at sample 0 but can be received
      // at sample N where N > 0. The term |extra_delay| accounts for N and it
      // is a value between 0 and 10ms.
      latencies_.push_back(now_time - pulse_time_ + extra_delay);
      pulse_time_ = 0;
    } else {
      PRINTD("-");
    }
  }

  int num_latency_values() const {
    return latencies_.size();
  }

  int min_latency() const {
    if (latencies_.empty())
      return 0;
    return *std::min_element(latencies_.begin(), latencies_.end());
  }

  int max_latency() const {
    if (latencies_.empty())
      return 0;
    return *std::max_element(latencies_.begin(), latencies_.end());
  }

  int average_latency() const {
    if (latencies_.empty())
      return 0;
    return 0.5 + static_cast<double> (
        std::accumulate(latencies_.begin(), latencies_.end(), 0)) /
        latencies_.size();
  }

  void PrintResults() const {
    PRINT("] ");
    for (auto it = latencies_.begin(); it != latencies_.end(); ++it) {
      PRINT("%d ", *it);
    }
    PRINT("\n");
    PRINT("%s[min, max, avg]=[%d, %d, %d] ms\n", kTag,
        min_latency(), max_latency(), average_latency());
  }

  int IndexToMilliseconds(double index) const {
    return 10.0 * (index / frames_per_buffer_) + 0.5;
  }

 private:
  Clock* clock_;
  const int frames_per_buffer_;
  const int bytes_per_buffer_;
  int play_count_;
  int rec_count_;
  int64_t pulse_time_;
  std::vector<int> latencies_;
};

// Mocks the AudioTransport object and proxies actions for the two callbacks
// (RecordedDataIsAvailable and NeedMorePlayData) to different implementations
// of AudioStreamInterface.
class MockAudioTransport : public AudioTransport {
 public:
  explicit MockAudioTransport(int type)
      : num_callbacks_(0),
        type_(type),
        play_count_(0),
        rec_count_(0),
        audio_stream_(nullptr) {}

  virtual ~MockAudioTransport() {}

  MOCK_METHOD10(RecordedDataIsAvailable,
                int32_t(const void* audioSamples,
                        const uint32_t nSamples,
                        const uint8_t nBytesPerSample,
                        const uint8_t nChannels,
                        const uint32_t samplesPerSec,
                        const uint32_t totalDelayMS,
                        const int32_t clockDrift,
                        const uint32_t currentMicLevel,
                        const bool keyPressed,
                        uint32_t& newMicLevel));
  MOCK_METHOD8(NeedMorePlayData,
               int32_t(const uint32_t nSamples,
                       const uint8_t nBytesPerSample,
                       const uint8_t nChannels,
                       const uint32_t samplesPerSec,
                       void* audioSamples,
                       uint32_t& nSamplesOut,
                       int64_t* elapsed_time_ms,
                       int64_t* ntp_time_ms));

  // Set default actions of the mock object. We are delegating to fake
  // implementations (of AudioStreamInterface) here.
  void HandleCallbacks(EventWrapper* test_is_done,
                       AudioStreamInterface* audio_stream,
                       int num_callbacks) {
    test_is_done_ = test_is_done;
    audio_stream_ = audio_stream;
    num_callbacks_ = num_callbacks;
    if (play_mode()) {
      ON_CALL(*this, NeedMorePlayData(_, _, _, _, _, _, _, _))
          .WillByDefault(
              Invoke(this, &MockAudioTransport::RealNeedMorePlayData));
    }
    if (rec_mode()) {
      ON_CALL(*this, RecordedDataIsAvailable(_, _, _, _, _, _, _, _, _, _))
          .WillByDefault(
              Invoke(this, &MockAudioTransport::RealRecordedDataIsAvailable));
    }
  }

  int32_t RealRecordedDataIsAvailable(const void* audioSamples,
                                      const uint32_t nSamples,
                                      const uint8_t nBytesPerSample,
                                      const uint8_t nChannels,
                                      const uint32_t samplesPerSec,
                                      const uint32_t totalDelayMS,
                                      const int32_t clockDrift,
                                      const uint32_t currentMicLevel,
                                      const bool keyPressed,
                                      uint32_t& newMicLevel) {
    EXPECT_TRUE(rec_mode()) << "No test is expecting these callbacks.";
    rec_count_++;
    // Process the recorded audio stream if an AudioStreamInterface
    // implementation exists.
    if (audio_stream_) {
      audio_stream_->Write(audioSamples, nSamples);
    }
    if (ReceivedEnoughCallbacks()) {
      test_is_done_->Set();
    }
    return 0;
  }

  int32_t RealNeedMorePlayData(const uint32_t nSamples,
                               const uint8_t nBytesPerSample,
                               const uint8_t nChannels,
                               const uint32_t samplesPerSec,
                               void* audioSamples,
                               uint32_t& nSamplesOut,
                               int64_t* elapsed_time_ms,
                               int64_t* ntp_time_ms) {
    EXPECT_TRUE(play_mode()) << "No test is expecting these callbacks.";
    play_count_++;
    nSamplesOut = nSamples;
    // Read (possibly processed) audio stream samples to be played out if an
    // AudioStreamInterface implementation exists.
    if (audio_stream_) {
      audio_stream_->Read(audioSamples, nSamples);
    }
    if (ReceivedEnoughCallbacks()) {
      test_is_done_->Set();
    }
    return 0;
  }

  bool ReceivedEnoughCallbacks() {
    bool recording_done = false;
    if (rec_mode())
      recording_done = rec_count_ >= num_callbacks_;
    else
      recording_done = true;

    bool playout_done = false;
    if (play_mode())
      playout_done = play_count_ >= num_callbacks_;
    else
      playout_done = true;

    return recording_done && playout_done;
  }

  bool play_mode() const { return type_ & kPlayout; }
  bool rec_mode() const { return type_ & kRecording; }

 private:
  EventWrapper* test_is_done_;
  int num_callbacks_;
  int type_;
  int play_count_;
  int rec_count_;
  AudioStreamInterface* audio_stream_;
  rtc::scoped_ptr<LatencyMeasuringAudioStream> latency_audio_stream_;
};

// AudioDeviceTest is a value-parameterized test.
class AudioDeviceTest
    : public testing::TestWithParam<AudioDeviceModule::AudioLayer> {
 protected:
  AudioDeviceTest()
      : test_is_done_(EventWrapper::Create()) {
    // One-time initialization of JVM and application context. Ensures that we
    // can do calls between C++ and Java. Initializes both Java and OpenSL ES
    // implementations.
    webrtc::audiodevicemodule::EnsureInitialized();
    // Creates an audio device based on the test parameter. See
    // INSTANTIATE_TEST_CASE_P() for details.
    audio_device_ = CreateAudioDevice();
    EXPECT_NE(audio_device_.get(), nullptr);
    EXPECT_EQ(0, audio_device_->Init());
    CacheAudioParameters();
  }
  virtual ~AudioDeviceTest() {
    EXPECT_EQ(0, audio_device_->Terminate());
  }

  int playout_sample_rate() const {
    return parameters_.playout_sample_rate;
  }
  int recording_sample_rate() const {
    return parameters_.recording_sample_rate;
  }
  int playout_channels() const {
    return parameters_.playout_channels;
  }
  int recording_channels() const {
    return parameters_.playout_channels;
  }
  int playout_frames_per_buffer() const {
    return parameters_.playout_frames_per_buffer();
  }
  int recording_frames_per_buffer() const {
    return parameters_.recording_frames_per_buffer();
  }

  scoped_refptr<AudioDeviceModule> audio_device() const {
    return audio_device_;
  }

  scoped_refptr<AudioDeviceModule> CreateAudioDevice() {
    scoped_refptr<AudioDeviceModule> module(
        AudioDeviceModuleImpl::Create(0, GetParam()));
    return module;
  }

  void CacheAudioParameters() {
    AudioDeviceBuffer* audio_buffer =
        static_cast<AudioDeviceModuleImpl*> (
            audio_device_.get())->GetAudioDeviceBuffer();
    parameters_.playout_sample_rate = audio_buffer->PlayoutSampleRate();
    parameters_.recording_sample_rate = audio_buffer->RecordingSampleRate();
    parameters_.playout_channels = audio_buffer->PlayoutChannels();
    parameters_.recording_channels = audio_buffer->RecordingChannels();
  }

  // Returns file name relative to the resource root given a sample rate.
  std::string GetFileName(int sample_rate) {
    EXPECT_TRUE(sample_rate == 48000 || sample_rate == 44100);
    char fname[64];
    snprintf(fname,
             sizeof(fname),
             "audio_device/audio_short%d",
             sample_rate / 1000);
    std::string file_name(webrtc::test::ResourcePath(fname, "pcm"));
    EXPECT_TRUE(test::FileExists(file_name));
#ifdef ENABLE_PRINTF
    PRINT("file name: %s\n", file_name.c_str());
    const int bytes = test::GetFileSize(file_name);
    PRINT("file size: %d [bytes]\n", bytes);
    PRINT("file size: %d [samples]\n", bytes / kBytesPerSample);
    const int seconds = bytes / (sample_rate * kBytesPerSample);
    PRINT("file size: %d [secs]\n", seconds);
    PRINT("file size: %d [callbacks]\n", seconds * kNumCallbacksPerSecond);
#endif
    return file_name;
  }

  void SetMaxPlayoutVolume() {
    uint32_t max_volume;
    EXPECT_EQ(0, audio_device()->MaxSpeakerVolume(&max_volume));
    EXPECT_EQ(0, audio_device()->SetSpeakerVolume(max_volume));
  }

  void StartPlayout() {
    EXPECT_FALSE(audio_device()->PlayoutIsInitialized());
    EXPECT_FALSE(audio_device()->Playing());
    EXPECT_EQ(0, audio_device()->InitPlayout());
    EXPECT_TRUE(audio_device()->PlayoutIsInitialized());
    EXPECT_EQ(0, audio_device()->StartPlayout());
    EXPECT_TRUE(audio_device()->Playing());
  }

  void StopPlayout() {
    EXPECT_EQ(0, audio_device()->StopPlayout());
    EXPECT_FALSE(audio_device()->Playing());
  }

  void StartRecording() {
    EXPECT_FALSE(audio_device()->RecordingIsInitialized());
    EXPECT_FALSE(audio_device()->Recording());
    EXPECT_EQ(0, audio_device()->InitRecording());
    EXPECT_TRUE(audio_device()->RecordingIsInitialized());
    EXPECT_EQ(0, audio_device()->StartRecording());
    EXPECT_TRUE(audio_device()->Recording());
  }

  void StopRecording() {
    EXPECT_EQ(0, audio_device()->StopRecording());
    EXPECT_FALSE(audio_device()->Recording());
  }

  int GetMaxSpeakerVolume() const {
    uint32_t max_volume(0);
    EXPECT_EQ(0, audio_device()->MaxSpeakerVolume(&max_volume));
    return max_volume;
  }

  int GetMinSpeakerVolume() const {
    uint32_t min_volume(0);
    EXPECT_EQ(0, audio_device()->MinSpeakerVolume(&min_volume));
    return min_volume;
  }

  int GetSpeakerVolume() const {
    uint32_t volume(0);
    EXPECT_EQ(0, audio_device()->SpeakerVolume(&volume));
    return volume;
  }

  rtc::scoped_ptr<EventWrapper> test_is_done_;
  scoped_refptr<AudioDeviceModule> audio_device_;
  AudioParameters parameters_;
};

TEST_P(AudioDeviceTest, ConstructDestruct) {
  // Using the test fixture to create and destruct the audio device module.
}

// Create an audio device instance and print out the native audio parameters.
TEST_P(AudioDeviceTest, AudioParameters) {
  EXPECT_NE(0, playout_sample_rate());
  PRINT("%splayout_sample_rate: %d\n", kTag, playout_sample_rate());
  EXPECT_NE(0, recording_sample_rate());
  PRINT("%srecording_sample_rate: %d\n", kTag, recording_sample_rate());
  EXPECT_NE(0, playout_channels());
  PRINT("%splayout_channels: %d\n", kTag, playout_channels());
  EXPECT_NE(0, recording_channels());
  PRINT("%srecording_channels: %d\n", kTag, recording_channels());
}

TEST_P(AudioDeviceTest, InitTerminate) {
  // Initialization is part of the test fixture.
  EXPECT_TRUE(audio_device()->Initialized());
  EXPECT_EQ(0, audio_device()->Terminate());
  EXPECT_FALSE(audio_device()->Initialized());
}

TEST_P(AudioDeviceTest, Devices) {
  // Device enumeration is not supported. Verify fixed values only.
  EXPECT_EQ(1, audio_device()->PlayoutDevices());
  EXPECT_EQ(1, audio_device()->RecordingDevices());
}

TEST_P(AudioDeviceTest, BuiltInAECIsAvailable) {
  PRINT("%sBuiltInAECIsAvailable: %s\n",
      kTag, audio_device()->BuiltInAECIsAvailable() ? "true" : "false");
}

TEST_P(AudioDeviceTest, SpeakerVolumeShouldBeAvailable) {
  bool available;
  EXPECT_EQ(0, audio_device()->SpeakerVolumeIsAvailable(&available));
  EXPECT_TRUE(available);
}

TEST_P(AudioDeviceTest, MaxSpeakerVolumeIsPositive) {
  EXPECT_GT(GetMaxSpeakerVolume(), 0);
}

TEST_P(AudioDeviceTest, MinSpeakerVolumeIsZero) {
  EXPECT_EQ(GetMinSpeakerVolume(), 0);
}

TEST_P(AudioDeviceTest, DefaultSpeakerVolumeIsWithinMinMax) {
  const int default_volume = GetSpeakerVolume();
  EXPECT_GE(default_volume, GetMinSpeakerVolume());
  EXPECT_LE(default_volume, GetMaxSpeakerVolume());
}

TEST_P(AudioDeviceTest, SetSpeakerVolumeActuallySetsVolume) {
  const int default_volume = GetSpeakerVolume();
  const int max_volume = GetMaxSpeakerVolume();
  EXPECT_EQ(0, audio_device()->SetSpeakerVolume(max_volume));
  int new_volume = GetSpeakerVolume();
  EXPECT_EQ(new_volume, max_volume);
  EXPECT_EQ(0, audio_device()->SetSpeakerVolume(default_volume));
}

// Tests that playout can be initiated, started and stopped.
TEST_P(AudioDeviceTest, StartStopPlayout) {
  StartPlayout();
  StopPlayout();
}

// Start playout and verify that the native audio layer starts asking for real
// audio samples to play out using the NeedMorePlayData callback.
TEST_P(AudioDeviceTest, StartPlayoutVerifyCallbacks) {
  MockAudioTransport mock(kPlayout);
  mock.HandleCallbacks(test_is_done_.get(), nullptr, kNumCallbacks);
  EXPECT_CALL(mock, NeedMorePlayData(playout_frames_per_buffer(),
                                     kBytesPerSample,
                                     playout_channels(),
                                     playout_sample_rate(),
                                     NotNull(),
                                     _, _, _))
      .Times(AtLeast(kNumCallbacks));
  EXPECT_EQ(0, audio_device()->RegisterAudioCallback(&mock));
  StartPlayout();
  test_is_done_->Wait(kTestTimeOutInMilliseconds);
  StopPlayout();
}

// Start recording and verify that the native audio layer starts feeding real
// audio samples via the RecordedDataIsAvailable callback.
TEST_P(AudioDeviceTest, StartRecordingVerifyCallbacks) {
  MockAudioTransport mock(kRecording);
  mock.HandleCallbacks(test_is_done_.get(), nullptr, kNumCallbacks);
  EXPECT_CALL(mock, RecordedDataIsAvailable(NotNull(),
                                            recording_frames_per_buffer(),
                                            kBytesPerSample,
                                            recording_channels(),
                                            recording_sample_rate(),
                                            kFixedRecordingDelay,
                                            0,
                                            0,
                                            false,
                                            _))
      .Times(AtLeast(kNumCallbacks));

  EXPECT_EQ(0, audio_device()->RegisterAudioCallback(&mock));
  StartRecording();
  test_is_done_->Wait(kTestTimeOutInMilliseconds);
  StopRecording();
}


// Start playout and recording (full-duplex audio) and verify that audio is
// active in both directions.
TEST_P(AudioDeviceTest, StartPlayoutAndRecordingVerifyCallbacks) {
  MockAudioTransport mock(kPlayout | kRecording);
  mock.HandleCallbacks(test_is_done_.get(), nullptr,  kNumCallbacks);
  EXPECT_CALL(mock, NeedMorePlayData(playout_frames_per_buffer(),
                                     kBytesPerSample,
                                     playout_channels(),
                                     playout_sample_rate(),
                                     NotNull(),
                                     _, _, _))
      .Times(AtLeast(kNumCallbacks));
  EXPECT_CALL(mock, RecordedDataIsAvailable(NotNull(),
                                            recording_frames_per_buffer(),
                                            kBytesPerSample,
                                            recording_channels(),
                                            recording_sample_rate(),
                                            Gt(kFixedRecordingDelay),
                                            0,
                                            0,
                                            false,
                                            _))
      .Times(AtLeast(kNumCallbacks));
  EXPECT_EQ(0, audio_device()->RegisterAudioCallback(&mock));
  StartPlayout();
  StartRecording();
  test_is_done_->Wait(kTestTimeOutInMilliseconds);
  StopRecording();
  StopPlayout();
}

// Start playout and read audio from an external PCM file when the audio layer
// asks for data to play out. Real audio is played out in this test but it does
// not contain any explicit verification that the audio quality is perfect.
TEST_P(AudioDeviceTest, RunPlayoutWithFileAsSource) {
  // TODO(henrika): extend test when mono output is supported.
  EXPECT_EQ(1, playout_channels());
  NiceMock<MockAudioTransport> mock(kPlayout);
  const int num_callbacks = kFilePlayTimeInSec * kNumCallbacksPerSecond;
  std::string file_name = GetFileName(playout_sample_rate());
  rtc::scoped_ptr<FileAudioStream> file_audio_stream(
      new FileAudioStream(num_callbacks, file_name, playout_sample_rate()));
  mock.HandleCallbacks(test_is_done_.get(),
                       file_audio_stream.get(),
                       num_callbacks);
  SetMaxPlayoutVolume();
  EXPECT_EQ(0, audio_device()->RegisterAudioCallback(&mock));
  StartPlayout();
  test_is_done_->Wait(kTestTimeOutInMilliseconds);
  StopPlayout();
}

// Start playout and recording and store recorded data in an intermediate FIFO
// buffer from which the playout side then reads its samples in the same order
// as they were stored. Under ideal circumstances, a callback sequence would
// look like: ...+-+-+-+-+-+-+-..., where '+' means 'packet recorded' and '-'
// means 'packet played'. Under such conditions, the FIFO would only contain
// one packet on average. However, under more realistic conditions, the size
// of the FIFO will vary more due to an unbalance between the two sides.
// This test tries to verify that the device maintains a balanced callback-
// sequence by running in loopback for ten seconds while measuring the size
// (max and average) of the FIFO. The size of the FIFO is increased by the
// recording side and decreased by the playout side.
// TODO(henrika): tune the final test parameters after running tests on several
// different devices.
TEST_P(AudioDeviceTest, RunPlayoutAndRecordingInFullDuplex) {
  EXPECT_EQ(recording_channels(), playout_channels());
  EXPECT_EQ(recording_sample_rate(), playout_sample_rate());
  NiceMock<MockAudioTransport> mock(kPlayout | kRecording);
  rtc::scoped_ptr<FifoAudioStream> fifo_audio_stream(
      new FifoAudioStream(playout_frames_per_buffer()));
  mock.HandleCallbacks(test_is_done_.get(),
                       fifo_audio_stream.get(),
                       kFullDuplexTimeInSec * kNumCallbacksPerSecond);
  SetMaxPlayoutVolume();
  EXPECT_EQ(0, audio_device()->RegisterAudioCallback(&mock));
  StartRecording();
  StartPlayout();
  test_is_done_->Wait(std::max(kTestTimeOutInMilliseconds,
                               1000 * kFullDuplexTimeInSec));
  StopPlayout();
  StopRecording();
  EXPECT_LE(fifo_audio_stream->average_size(), 10);
  EXPECT_LE(fifo_audio_stream->largest_size(), 20);
}

// Measures loopback latency and reports the min, max and average values for
// a full duplex audio session.
// The latency is measured like so:
// - Insert impulses periodically on the output side.
// - Detect the impulses on the input side.
// - Measure the time difference between the transmit time and receive time.
// - Store time differences in a vector and calculate min, max and average.
// This test requires a special hardware called Audio Loopback Dongle.
// See http://source.android.com/devices/audio/loopback.html for details.
TEST_P(AudioDeviceTest, DISABLED_MeasureLoopbackLatency) {
  EXPECT_EQ(recording_channels(), playout_channels());
  EXPECT_EQ(recording_sample_rate(), playout_sample_rate());
  NiceMock<MockAudioTransport> mock(kPlayout | kRecording);
  rtc::scoped_ptr<LatencyMeasuringAudioStream> latency_audio_stream(
      new LatencyMeasuringAudioStream(playout_frames_per_buffer()));
  mock.HandleCallbacks(test_is_done_.get(),
                       latency_audio_stream.get(),
                       kMeasureLatencyTimeInSec * kNumCallbacksPerSecond);
  EXPECT_EQ(0, audio_device()->RegisterAudioCallback(&mock));
  SetMaxPlayoutVolume();
  StartRecording();
  StartPlayout();
  test_is_done_->Wait(std::max(kTestTimeOutInMilliseconds,
                               1000 * kMeasureLatencyTimeInSec));
  StopPlayout();
  StopRecording();
  // Verify that the correct number of transmitted impulses are detected.
  EXPECT_EQ(latency_audio_stream->num_latency_values(),
            kImpulseFrequencyInHz * kMeasureLatencyTimeInSec - 1);
  latency_audio_stream->PrintResults();
}

INSTANTIATE_TEST_CASE_P(AudioDeviceTest, AudioDeviceTest,
  ::testing::ValuesIn(kAudioLayers));

}  // namespace webrtc

/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_device/android/single_rw_fifo.h"

#include <list>

#include "testing/gtest/include/gtest/gtest.h"
#include "webrtc/system_wrappers/interface/scoped_ptr.h"

namespace webrtc {

class SingleRwFifoTest : public testing::Test {
 public:
  enum {
    // Uninteresting as it does not affect test
    kBufferSize = 8,
    kCapacity = 6,
  };

  SingleRwFifoTest() : fifo_(kCapacity), pushed_(0), available_(0) {
  }
  virtual ~SingleRwFifoTest() {}

  void SetUp() {
    for (int8_t i = 0; i < kCapacity; ++i) {
      // Create memory area.
      buffer_[i].reset(new int8_t[kBufferSize]);
      // Set the first byte in the buffer to the order in which it was created
      // this allows us to e.g. check that the buffers don't re-arrange.
      buffer_[i][0] = i;
      // Queue used by test.
      memory_queue_.push_back(buffer_[i].get());
    }
    available_ = kCapacity;
    VerifySizes();
  }

  void Push(int number_of_buffers) {
    for (int8_t i = 0; i < number_of_buffers; ++i) {
      int8_t* data = memory_queue_.front();
      memory_queue_.pop_front();
      fifo_.Push(data);
      --available_;
      ++pushed_;
    }
    VerifySizes();
    VerifyOrdering();
  }
  void Pop(int number_of_buffers) {
    for (int8_t i = 0; i < number_of_buffers; ++i) {
      int8_t* data = fifo_.Pop();
      memory_queue_.push_back(data);
      ++available_;
      --pushed_;
    }
    VerifySizes();
    VerifyOrdering();
  }

  void VerifyOrdering() const {
    std::list<int8_t*>::const_iterator iter = memory_queue_.begin();
    if (iter == memory_queue_.end()) {
      return;
    }
    int8_t previous_index = DataToElementIndex(*iter);
    ++iter;
    for (; iter != memory_queue_.end(); ++iter) {
      int8_t current_index = DataToElementIndex(*iter);
      EXPECT_EQ(current_index, ++previous_index % kCapacity);
    }
  }

  void VerifySizes() {
    EXPECT_EQ(available_, static_cast<int>(memory_queue_.size()));
    EXPECT_EQ(pushed_, fifo_.size());
  }

  int8_t DataToElementIndex(int8_t* data) const {
    return data[0];
  }

 protected:
  SingleRwFifo fifo_;
  // Memory area for proper de-allocation.
  scoped_ptr<int8_t[]> buffer_[kCapacity];
  std::list<int8_t*> memory_queue_;

  int pushed_;
  int available_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SingleRwFifoTest);
};

TEST_F(SingleRwFifoTest, Construct) {
  // All verifications are done in SetUp.
}

TEST_F(SingleRwFifoTest, Push) {
  Push(kCapacity);
}

TEST_F(SingleRwFifoTest, Pop) {
  // Push all available.
  Push(available_);

  // Test border cases:
  // At capacity
  Pop(1);
  Push(1);

  // At minimal capacity
  Pop(pushed_);
  Push(1);
  Pop(1);
}

}  // namespace webrtc

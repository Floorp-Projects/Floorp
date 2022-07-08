/*
 *  Copyright 2022 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_TASK_QUEUE_TEST_MOCK_TASK_QUEUE_BASE_H_
#define API_TASK_QUEUE_TEST_MOCK_TASK_QUEUE_BASE_H_

#include <memory>

#include "api/task_queue/task_queue_base.h"
#include "test/gmock.h"

namespace webrtc {

class MockTaskQueueBase : public TaskQueueBase {
 public:
  MOCK_METHOD0(Delete, void());
  MOCK_METHOD1(PostTask, void(std::unique_ptr<QueuedTask>));
  MOCK_METHOD2(PostDelayedTask, void(std::unique_ptr<QueuedTask>, uint32_t));
  MOCK_METHOD2(PostDelayedHighPrecisionTask,
               void(std::unique_ptr<QueuedTask>, uint32_t));
};

}  // namespace webrtc

#endif  // API_TASK_QUEUE_TEST_MOCK_TASK_QUEUE_BASE_H_

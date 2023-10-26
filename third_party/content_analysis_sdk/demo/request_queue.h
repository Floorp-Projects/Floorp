// Copyright 2022 The Chromium Authors.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_ANALYSIS_DEMO_REQUST_QUEUE_H_
#define CONTENT_ANALYSIS_DEMO_REQUST_QUEUE_H_

#include <condition_variable>
#include <memory>
#include <mutex>
#include <queue>

#include "content_analysis/sdk/analysis_agent.h"

// This class maintains a list of outstanding content analysis requests to
// process.  Each request is encapsulated in one ContentAnalysisEvent.
// Requests are handled in FIFO order.
class RequestQueue {
 public:
  using Event = content_analysis::sdk::ContentAnalysisEvent;

  RequestQueue() = default;
  virtual ~RequestQueue() = default;

  // Push a new content analysis event into the queue.
  void push(std::unique_ptr<Event> event) {
    std::lock_guard<std::mutex> lock(mutex_);

    events_.push(std::move(event));

    // Wake before leaving to prevent unpredicatable scheduling.
    cv_.notify_one();
  }

  // Pop the next request from the queue, blocking if necessary until an event
  // is available.  Returns a nullptr if the queue will produce no more
  // events.
  std::unique_ptr<Event> pop() {
    std::unique_lock<std::mutex> lock(mutex_);

    while (!abort_ && events_.size() == 0)
      cv_.wait(lock);

    std::unique_ptr<Event> event;
    if (!abort_) {
      event = std::move(events_.front());
      events_.pop();
    }

    return event;
  }

  // Marks the queue as aborted.  pop() will now return nullptr.
  void abort() {
    std::lock_guard<std::mutex> lg(mutex_);

    abort_ = true;

    // Wake before leaving to prevent unpredicatable scheduling.
    cv_.notify_all();
  }

 private:
  std::queue<std::unique_ptr<Event>> events_;
  std::mutex mutex_;
  std::condition_variable cv_;
  bool abort_ = false;
};

#endif  // CONTENT_ANALYSIS_DEMO_REQUST_QUEUE_H_

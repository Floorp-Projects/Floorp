/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef LOGGING_RTC_EVENT_LOG_OUTPUT_RTC_EVENT_LOG_OUTPUT_FILE_H_
#define LOGGING_RTC_EVENT_LOG_OUTPUT_RTC_EVENT_LOG_OUTPUT_FILE_H_

#include <stddef.h>

#include <memory>
#include <string>

#include "api/rtceventlogoutput.h"
#include "rtc_base/platform_file.h"  // Can't neatly forward PlatformFile.

namespace webrtc {

class FileWrapper;

class RtcEventLogOutputFile final : public RtcEventLogOutput {
 public:
  static const size_t kMaxReasonableFileSize;  // Explanation at declaration.

  // Unlimited/limited-size output file (by filename).
  explicit RtcEventLogOutputFile(const std::string& file_name);
  RtcEventLogOutputFile(const std::string& file_name, size_t max_size_bytes);

  // Unlimited/limited-size output file (by file handle).
  explicit RtcEventLogOutputFile(rtc::PlatformFile file);
  RtcEventLogOutputFile(rtc::PlatformFile file, size_t max_size_bytes);

  ~RtcEventLogOutputFile() override;

  bool IsActive() const override;

  bool Write(const std::string& output) override;

 private:
  // IsActive() can be called either from outside or from inside, but we don't
  // want to incur the overhead of a virtual function call if called from inside
  // some other function of this class.
  inline bool IsActiveInternal() const;

  // TODO(eladalon): We're still discussing whether to use FileWrapper or not.
  // If we end up keeping FileWrapper, we should use its own max-size logic,
  // rather than duplicate it.
  const size_t max_size_bytes_;
  size_t written_bytes_{0};
  std::unique_ptr<FileWrapper> file_;
};

}  // namespace webrtc

#endif  // LOGGING_RTC_EVENT_LOG_OUTPUT_RTC_EVENT_LOG_OUTPUT_FILE_H_

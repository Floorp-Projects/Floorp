/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_MESSAGE_HANDLER_H_
#define RTC_BASE_MESSAGE_HANDLER_H_

#include <utility>

#include "api/function_view.h"
#include "rtc_base/constructor_magic.h"
#include "rtc_base/system/rtc_export.h"

namespace rtc {

struct Message;

// MessageQueue/Thread Messages get dispatched to a MessageHandler via the
// |OnMessage()| callback method.
//
// Note: Besides being an interface, the class can perform automatic cleanup
// in the destructor.
// TODO(bugs.webrtc.org/11908): The |auto_cleanup| parameter and associated
// logic is a temporary step while changing the MessageHandler class to be
// a pure virtual interface. The automatic cleanup step involves a number of
// complex operations and as part of this interface, can easily go by unnoticed
// and bundled into situations where it's not needed.
class RTC_EXPORT MessageHandler {
 public:
  virtual ~MessageHandler();
  virtual void OnMessage(Message* msg) = 0;

 protected:
  // TODO(bugs.webrtc.org/11908): Remove this ctor.
  explicit MessageHandler(bool auto_cleanup);

 private:
  RTC_DISALLOW_COPY_AND_ASSIGN(MessageHandler);
};

class RTC_EXPORT MessageHandlerAutoCleanup : public MessageHandler {
 public:
  ~MessageHandlerAutoCleanup() override;

 protected:
  MessageHandlerAutoCleanup();

 private:
  RTC_DISALLOW_COPY_AND_ASSIGN(MessageHandlerAutoCleanup);
};

}  // namespace rtc

#endif  // RTC_BASE_MESSAGE_HANDLER_H_

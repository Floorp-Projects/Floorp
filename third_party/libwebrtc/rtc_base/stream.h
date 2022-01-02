/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_STREAM_H_
#define RTC_BASE_STREAM_H_

#include <memory>

#include "rtc_base/buffer.h"
#include "rtc_base/constructor_magic.h"
#include "rtc_base/message_handler.h"
#include "rtc_base/system/rtc_export.h"
#include "rtc_base/third_party/sigslot/sigslot.h"
#include "rtc_base/thread.h"

namespace rtc {

///////////////////////////////////////////////////////////////////////////////
// StreamInterface is a generic asynchronous stream interface, supporting read,
// write, and close operations, and asynchronous signalling of state changes.
// The interface is designed with file, memory, and socket implementations in
// mind.  Some implementations offer extended operations, such as seeking.
///////////////////////////////////////////////////////////////////////////////

// The following enumerations are declared outside of the StreamInterface
// class for brevity in use.

// The SS_OPENING state indicates that the stream will signal open or closed
// in the future.
enum StreamState { SS_CLOSED, SS_OPENING, SS_OPEN };

// Stream read/write methods return this value to indicate various success
// and failure conditions described below.
enum StreamResult { SR_ERROR, SR_SUCCESS, SR_BLOCK, SR_EOS };

// StreamEvents are used to asynchronously signal state transitionss.  The flags
// may be combined.
//  SE_OPEN: The stream has transitioned to the SS_OPEN state
//  SE_CLOSE: The stream has transitioned to the SS_CLOSED state
//  SE_READ: Data is available, so Read is likely to not return SR_BLOCK
//  SE_WRITE: Data can be written, so Write is likely to not return SR_BLOCK
enum StreamEvent { SE_OPEN = 1, SE_READ = 2, SE_WRITE = 4, SE_CLOSE = 8 };

struct StreamEventData : public MessageData {
  int events, error;
  StreamEventData(int ev, int er) : events(ev), error(er) {}
};

class RTC_EXPORT StreamInterface : public MessageHandlerAutoCleanup {
 public:
  enum { MSG_POST_EVENT = 0xF1F1, MSG_MAX = MSG_POST_EVENT };

  ~StreamInterface() override;

  virtual StreamState GetState() const = 0;

  // Read attempts to fill buffer of size buffer_len.  Write attempts to send
  // data_len bytes stored in data.  The variables read and write are set only
  // on SR_SUCCESS (see below).  Likewise, error is only set on SR_ERROR.
  // Read and Write return a value indicating:
  //  SR_ERROR: an error occurred, which is returned in a non-null error
  //    argument.  Interpretation of the error requires knowledge of the
  //    stream's concrete type, which limits its usefulness.
  //  SR_SUCCESS: some number of bytes were successfully written, which is
  //    returned in a non-null read/write argument.
  //  SR_BLOCK: the stream is in non-blocking mode, and the operation would
  //    block, or the stream is in SS_OPENING state.
  //  SR_EOS: the end-of-stream has been reached, or the stream is in the
  //    SS_CLOSED state.
  virtual StreamResult Read(void* buffer,
                            size_t buffer_len,
                            size_t* read,
                            int* error) = 0;
  virtual StreamResult Write(const void* data,
                             size_t data_len,
                             size_t* written,
                             int* error) = 0;
  // Attempt to transition to the SS_CLOSED state.  SE_CLOSE will not be
  // signalled as a result of this call.
  virtual void Close() = 0;

  // Streams may signal one or more StreamEvents to indicate state changes.
  // The first argument identifies the stream on which the state change occured.
  // The second argument is a bit-wise combination of StreamEvents.
  // If SE_CLOSE is signalled, then the third argument is the associated error
  // code.  Otherwise, the value is undefined.
  // Note: Not all streams will support asynchronous event signalling.  However,
  // SS_OPENING and SR_BLOCK returned from stream member functions imply that
  // certain events will be raised in the future.
  sigslot::signal3<StreamInterface*, int, int> SignalEvent;

  // Like calling SignalEvent, but posts a message to the specified thread,
  // which will call SignalEvent.  This helps unroll the stack and prevent
  // re-entrancy.
  void PostEvent(Thread* t, int events, int err);
  // Like the aforementioned method, but posts to the current thread.
  void PostEvent(int events, int err);

  // Return true if flush is successful.
  virtual bool Flush();

  //
  // CONVENIENCE METHODS
  //
  // These methods are implemented in terms of other methods, for convenience.
  //

  // WriteAll is a helper function which repeatedly calls Write until all the
  // data is written, or something other than SR_SUCCESS is returned.  Note that
  // unlike Write, the argument 'written' is always set, and may be non-zero
  // on results other than SR_SUCCESS.  The remaining arguments have the
  // same semantics as Write.
  StreamResult WriteAll(const void* data,
                        size_t data_len,
                        size_t* written,
                        int* error);

 protected:
  StreamInterface();

  // MessageHandler Interface
  void OnMessage(Message* msg) override;

 private:
  RTC_DISALLOW_COPY_AND_ASSIGN(StreamInterface);
};

///////////////////////////////////////////////////////////////////////////////
// StreamAdapterInterface is a convenient base-class for adapting a stream.
// By default, all operations are pass-through.  Override the methods that you
// require adaptation.  Streams should really be upgraded to reference-counted.
// In the meantime, use the owned flag to indicate whether the adapter should
// own the adapted stream.
///////////////////////////////////////////////////////////////////////////////

class StreamAdapterInterface : public StreamInterface,
                               public sigslot::has_slots<> {
 public:
  explicit StreamAdapterInterface(StreamInterface* stream, bool owned = true);

  // Core Stream Interface
  StreamState GetState() const override;
  StreamResult Read(void* buffer,
                    size_t buffer_len,
                    size_t* read,
                    int* error) override;
  StreamResult Write(const void* data,
                     size_t data_len,
                     size_t* written,
                     int* error) override;
  void Close() override;

  bool Flush() override;

  void Attach(StreamInterface* stream, bool owned = true);
  StreamInterface* Detach();

 protected:
  ~StreamAdapterInterface() override;

  // Note that the adapter presents itself as the origin of the stream events,
  // since users of the adapter may not recognize the adapted object.
  virtual void OnEvent(StreamInterface* stream, int events, int err);
  StreamInterface* stream() { return stream_; }

 private:
  StreamInterface* stream_;
  bool owned_;
  RTC_DISALLOW_COPY_AND_ASSIGN(StreamAdapterInterface);
};

}  // namespace rtc

#endif  // RTC_BASE_STREAM_H_

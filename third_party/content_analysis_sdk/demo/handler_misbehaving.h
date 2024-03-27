/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CONTENT_ANALYSIS_DEMO_HANDLER_MISBEHAVING_H_
#define CONTENT_ANALYSIS_DEMO_HANDLER_MISBEHAVING_H_

#include <time.h>

#include <algorithm>
#include <chrono>
#include <fstream>
#include <map>
#include <iostream>
#include <utility>
#include <vector>
#include <regex>
#include <windows.h>

#include "content_analysis/sdk/analysis.pb.h"
#include "content_analysis/sdk/analysis_agent.h"
#include "agent/src/event_win.h"
#include "handler.h"

enum class Mode {
// Have to use a "Mode_" prefix to avoid preprocessing problems in StringToMode
#define AGENT_MODE(name) Mode_##name,
#include "modes.h"
#undef AGENT_MODE
};

extern std::map<std::string, Mode> sStringToMode;
extern std::map<Mode, std::string> sModeToString;

// Writes a string to the pipe. Returns ERROR_SUCCESS if successful, else
// returns GetLastError() of the write.  This function does not return until
// the entire message has been sent (or an error occurs).
static DWORD WriteBigMessageToPipe(HANDLE pipe, const std::string& message) {
  std::cout << "[demo] WriteBigMessageToPipe top, message size is "
            << message.size() << std::endl;
  if (message.empty()) {
    return ERROR_SUCCESS;
  }

  OVERLAPPED overlapped;
  memset(&overlapped, 0, sizeof(overlapped));
  overlapped.hEvent = CreateEvent(/*securityAttr=*/nullptr,
                                  /*manualReset=*/TRUE,
                                  /*initialState=*/FALSE,
                                  /*name=*/nullptr);
  if (overlapped.hEvent == nullptr) {
    return GetLastError();
  }

  DWORD err = ERROR_SUCCESS;
  const char* cursor = message.data();
  for (DWORD size = message.length(); size > 0;) {
    std::cout << "[demo] WriteBigMessageToPipe top of loop, remaining size "
              << size << std::endl;
    if (WriteFile(pipe, cursor, size, /*written=*/nullptr, &overlapped)) {
      std::cout << "[demo] WriteBigMessageToPipe: success" << std::endl;
      err = ERROR_SUCCESS;
      break;
    }

    // If an I/O is not pending, return the error.
    err = GetLastError();
    if (err != ERROR_IO_PENDING) {
      std::cout
          << "[demo] WriteBigMessageToPipe: returning error from WriteFile "
          << err << std::endl;
      break;
    }

    DWORD written;
    if (!GetOverlappedResult(pipe, &overlapped, &written, /*wait=*/TRUE)) {
      err = GetLastError();
      std::cout << "[demo] WriteBigMessageToPipe: returning error from "
                   "GetOverlappedREsult "
                << err << std::endl;
      break;
    }

    // reset err for the next loop iteration
    err = ERROR_SUCCESS;
    std::cout << "[demo] WriteBigMessageToPipe: bottom of loop, wrote "
              << written << std::endl;
    cursor += written;
    size -= written;
  }

  CloseHandle(overlapped.hEvent);
  return err;
}

// An AgentEventHandler that does various misbehaving things
class MisbehavingHandler final : public Handler {
 public:
  using Event = content_analysis::sdk::ContentAnalysisEvent;

  static
  std::unique_ptr<AgentEventHandler> Create(
                                  const std::string& modeStr,
                                  std::vector<unsigned long>&& delays,
                                  const std::string& print_data_file_path,
                                  RegexArray&& toBlock = RegexArray(),
                                  RegexArray&& toWarn = RegexArray(),
                                  RegexArray&& toReport = RegexArray()) {
    auto it = sStringToMode.find(modeStr);
    if (it == sStringToMode.end()) {
      std::cout << "\"" << modeStr << "\""
                << " is not a valid mode!" << std::endl;
      return nullptr;
    }

    return std::unique_ptr<AgentEventHandler>(new MisbehavingHandler(it->second, std::move(delays), print_data_file_path, std::move(toBlock), std::move(toWarn), std::move(toReport)));
  }

 private:
  MisbehavingHandler(Mode mode, std::vector<unsigned long>&& delays, const std::string& print_data_file_path,
          RegexArray&& toBlock = RegexArray(),
          RegexArray&& toWarn = RegexArray(),
          RegexArray&& toReport = RegexArray()) :
           Handler(std::move(delays), print_data_file_path, std::move(toBlock), std::move(toWarn), std::move(toReport)),
           mode_(mode) {}


  template <size_t N>
  DWORD SendBytesOverPipe(const unsigned char (&bytes)[N],
                          const std::unique_ptr<Event>& event) {
    content_analysis::sdk::ContentAnalysisEventWin* eventWin =
        static_cast<content_analysis::sdk::ContentAnalysisEventWin*>(
            event.get());
    HANDLE pipe = eventWin->Pipe();
    std::string s(reinterpret_cast<const char*>(bytes), N);
    return WriteBigMessageToPipe(pipe, s);
  }

  bool SetCustomResponse(AtomicCout& aout, std::unique_ptr<Event>& event) override {
    std::cout << std::endl << "----------" << std::endl << std::endl;
    std::cout << "Mode is " << sModeToString[mode_] << std::endl;

    bool handled = true;
    if (mode_ == Mode::Mode_largeResponse) {
      for (size_t i = 0; i < 1000; ++i) {
        content_analysis::sdk::ContentAnalysisResponse_Result* result =
            event->GetResponse().add_results();
        result->set_tag("someTag");
        content_analysis::sdk::ContentAnalysisResponse_Result_TriggeredRule*
            triggeredRule = result->add_triggered_rules();
        triggeredRule->set_rule_id("some_id");
        triggeredRule->set_rule_name("some_name");
      }
    } else if (mode_ ==
               Mode::Mode_invalidUtf8StringStartByteIsContinuationByte) {
      // protobuf docs say
      // "A string must always contain UTF-8 encoded text."
      // So let's try something invalid
      // Anything with bits 10xxxxxx is only a continuation code point
      event->GetResponse().set_request_token("\x80\x41\x41\x41");
    } else if (mode_ ==
               Mode::Mode_invalidUtf8StringEndsInMiddleOfMultibyteSequence) {
      // f0 byte indicates there should be 3 bytes following it, but here
      // there are only 2
      event->GetResponse().set_request_token("\x41\xf0\x90\x8d");
    } else if (mode_ == Mode::Mode_invalidUtf8StringOverlongEncoding) {
      // codepoint U+20AC, should be encoded in 3 bytes (E2 82 AC)
      // instead of 4
      event->GetResponse().set_request_token("\xf0\x82\x82\xac");
    } else if (mode_ == Mode::Mode_invalidUtf8StringMultibyteSequenceTooShort) {
      // f0 byte indicates there should be 3 bytes following it, but here
      // there are only 2 (\x41 is not a continuation byte)
      event->GetResponse().set_request_token("\xf0\x90\x8d\x41");
    } else if (mode_ == Mode::Mode_invalidUtf8StringDecodesToInvalidCodePoint) {
      // decodes to U+1FFFFF, but only up to U+10FFFF is a valid code point
      event->GetResponse().set_request_token("\xf7\xbf\xbf\xbf");
    } else if (mode_ == Mode::Mode_stringWithEmbeddedNull) {
      event->GetResponse().set_request_token("\x41\x00\x41");
    } else if (mode_ == Mode::Mode_zeroResults) {
      event->GetResponse().clear_results();
    } else if (mode_ == Mode::Mode_resultWithInvalidStatus) {
      // This causes an assertion failure and the process exits
      // So we just serialize this ourselves in SendCustomResponse()
      /*content_analysis::sdk::ContentAnalysisResponse_Result* result =
          event->GetResponse().mutable_results(0);
      result->set_status(
          static_cast<
              ::content_analysis::sdk::ContentAnalysisResponse_Result_Status>(
              100));*/
    } else {
      handled = false;
    }
    return handled;
  }

  bool SendCustomResponse(std::unique_ptr<Event>& event) override {
    if (mode_ == Mode::Mode_largeResponse) {
      content_analysis::sdk::ContentAnalysisEventWin* eventWin =
          static_cast<content_analysis::sdk::ContentAnalysisEventWin*>(
              event.get());
      HANDLE pipe = eventWin->Pipe();
      std::cout << "largeResponse about to write" << std::endl;
      DWORD result = WriteBigMessageToPipe(
          pipe, eventWin->SerializeStringToSendToBrowser());
      std::cout << "largeResponse done writing with error " << result
                << std::endl;
      eventWin->SetResponseSent();
    } else if (mode_ == Mode::Mode_resultWithInvalidStatus) {
      content_analysis::sdk::ContentAnalysisEventWin* eventWin =
          static_cast<content_analysis::sdk::ContentAnalysisEventWin*>(
              event.get());
      HANDLE pipe = eventWin->Pipe();
      std::string serializedString = eventWin->SerializeStringToSendToBrowser();
      // The last byte is the status value. Set it to 100
      serializedString[serializedString.length() - 1] = 100;
      WriteBigMessageToPipe(pipe, serializedString);
    } else if (mode_ == Mode::Mode_messageTruncatedInMiddleOfString) {
      unsigned char bytes[5];
      bytes[0] = 10;  // field 1 (request_token), LEN encoding
      bytes[1] = 13;  // length 13
      bytes[2] = 65;  // "A"
      bytes[3] = 66;  // "B"
      bytes[4] = 67;  // "C"
      SendBytesOverPipe(bytes, event);
    } else if (mode_ == Mode::Mode_messageWithInvalidWireType) {
      unsigned char bytes[5];
      bytes[0] = 15;  // field 1 (request_token), "7" encoding (invalid value)
      bytes[1] = 3;   // length 3
      bytes[2] = 65;  // "A"
      bytes[3] = 66;  // "B"
      bytes[4] = 67;  // "C"
      SendBytesOverPipe(bytes, event);
    } else if (mode_ == Mode::Mode_messageWithUnusedFieldNumber) {
      unsigned char bytes[5];
      bytes[0] = 82;  // field 10 (this is invalid), LEN encoding
      bytes[1] = 3;   // length 3
      bytes[2] = 65;  // "A"
      bytes[3] = 66;  // "B"
      bytes[4] = 67;  // "C"
      SendBytesOverPipe(bytes, event);
    } else if (mode_ == Mode::Mode_messageWithWrongStringWireType) {
      unsigned char bytes[2];
      bytes[0] = 10;  // field 1 (request_token), VARINT encoding (but should be
                      // a string/LEN)
      bytes[1] = 42;  // value 42
      SendBytesOverPipe(bytes, event);
    } else if (mode_ == Mode::Mode_messageWithZeroTag) {
      unsigned char bytes[1];
      // The protobuf deserialization code seems to handle this
      // in a special case.
      bytes[0] = 0;
      SendBytesOverPipe(bytes, event);
    } else if (mode_ == Mode::Mode_messageWithZeroFieldButNonzeroWireType) {
      // The protobuf deserialization code seems to handle this
      // in a special case.
      unsigned char bytes[5];
      bytes[0] = 2;   // field 0 (invalid), LEN encoding
      bytes[1] = 3;   // length 13
      bytes[2] = 65;  // "A"
      bytes[3] = 66;  // "B"
      bytes[4] = 67;  // "C"
      SendBytesOverPipe(bytes, event);
    } else if (mode_ == Mode::Mode_messageWithGroupEnd) {
      // GROUP_ENDs are obsolete and the deserialization code
      // handles them in a special case.
      unsigned char bytes[1];
      bytes[0] = 12;  // field 1 (request_token), GROUP_END encoding
      SendBytesOverPipe(bytes, event);
    } else if (mode_ == Mode::Mode_messageTruncatedInMiddleOfVarint) {
      unsigned char bytes[2];
      bytes[0] = 16;   // field 2 (status), VARINT encoding
      bytes[1] = 128;  // high bit is set, indicating there
                       // should be a byte after this
      SendBytesOverPipe(bytes, event);
    } else if (mode_ == Mode::Mode_messageTruncatedInMiddleOfTag) {
      unsigned char bytes[1];
      bytes[0] = 128;  // tag is actually encoded as a VARINT, so set the high
                       // bit, indicating there should be a byte after this
      SendBytesOverPipe(bytes, event);
    } else {
      return false;
    }
    return true;
  }

 private:
  Mode mode_;
};

#endif  // CONTENT_ANALYSIS_DEMO_HANDLER_MISBEHAVING_H_

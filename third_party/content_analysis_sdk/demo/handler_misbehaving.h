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
class MisbehavingHandler final : public content_analysis::sdk::AgentEventHandler {
 public:
  using Event = content_analysis::sdk::ContentAnalysisEvent;

  static
  std::unique_ptr<AgentEventHandler> Create(unsigned long delay,
                                  const std::string& modeStr) {
    auto it = sStringToMode.find(modeStr);
    if (it == sStringToMode.end()) {
      std::cout << "\"" << modeStr << "\""
                << " is not a valid mode!" << std::endl;
      return nullptr;
    }

    return std::unique_ptr<AgentEventHandler>(new MisbehavingHandler(delay, it->second));
  }

 private:
  MisbehavingHandler(unsigned long delay, Mode mode) : delay_(delay), mode_(mode) {}

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

  // Analyzes one request from Google Chrome and responds back to the browser
  // with either an allow or block verdict.
  void AnalyzeContent(std::unique_ptr<Event> event) {
    // An event represents one content analysis request and response triggered
    // by a user action in Google Chrome.  The agent determines whether the
    // user is allowed to perform the action by examining event->GetRequest().
    // The verdict, which can be "allow" or "block" is written into
    // event->GetResponse().

    std::cout << std::endl << "----------" << std::endl << std::endl;

    DumpRequest(event->GetRequest());
    std::cout << "Mode is " << sModeToString[mode_] << std::endl;

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
      // So we just serialize this ourselves below
      /*content_analysis::sdk::ContentAnalysisResponse_Result* result =
          event->GetResponse().mutable_results(0);
      result->set_status(
          static_cast<
              ::content_analysis::sdk::ContentAnalysisResponse_Result_Status>(
              100));*/
    } else {
      bool block = false;

      if (event->GetRequest().has_text_content()) {
        block = ShouldBlockRequest(event->GetRequest().text_content());
      } else if (event->GetRequest().has_file_path()) {
        block = ShouldBlockRequest(event->GetRequest().file_path());
      }

      if (block) {
        auto rc = content_analysis::sdk::SetEventVerdictToBlock(event.get());
        std::cout << "  Verdict: block";
        if (rc != content_analysis::sdk::ResultCode::OK) {
          std::cout << " error: "
                    << content_analysis::sdk::ResultCodeToString(rc)
                    << std::endl;
          std::cout << "  " << event->DebugString() << std::endl;
        }
        std::cout << std::endl;
      } else {
        std::cout << "  Verdict: allow" << std::endl;
      }
    }

    std::cout << std::endl;

    // If a delay is specified, wait that much.
    if (delay_ > 0) {
      std::cout << "[Demo] delaying request processing for " << delay_ << "s"
                << std::endl;
      std::this_thread::sleep_for(std::chrono::seconds(delay_));
    }

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
      std::cout << "(misbehaving) Handler::AnalyzeContent() about to call "
                   "event->Send(), mode is "
                << sModeToString[mode_] << std::endl;
      // Send the response back to Google Chrome.
      auto rc = event->Send();
      if (rc != content_analysis::sdk::ResultCode::OK) {
        std::cout << "[Demo] Error sending response: "
                  << content_analysis::sdk::ResultCodeToString(rc) << std::endl;
        std::cout << event->DebugString() << std::endl;
      }
    }
  }

 private:
  void OnBrowserConnected(
      const content_analysis::sdk::BrowserInfo& info) override {
    std::cout << std::endl << "==========" << std::endl;
    std::cout << "Browser connected pid=" << info.pid << std::endl;
  }

  void OnBrowserDisconnected(
      const content_analysis::sdk::BrowserInfo& info) override {
    std::cout << std::endl
              << "Browser disconnected pid=" << info.pid << std::endl;
    std::cout << "==========" << std::endl;
  }

  void OnAnalysisRequested(std::unique_ptr<Event> event) override {
    // If the agent is capable of analyzing content in the background, the
    // events may be handled in background threads.  Having said that, a
    // event should not be assumed to be thread safe, that is, it should not
    // be accessed by more than one thread concurrently.
    //
    // In this example code, the event is handled synchronously.
    AnalyzeContent(std::move(event));
  }
  void OnResponseAcknowledged(
      const content_analysis::sdk::ContentAnalysisAcknowledgement& ack)
      override {
    const char* final_action = "<Unknown>";
    if (ack.has_final_action()) {
      switch (ack.final_action()) {
        case content_analysis::sdk::ContentAnalysisAcknowledgement::
            ACTION_UNSPECIFIED:
          final_action = "<Unspecified>";
          break;
        case content_analysis::sdk::ContentAnalysisAcknowledgement::ALLOW:
          final_action = "Allow";
          break;
        case content_analysis::sdk::ContentAnalysisAcknowledgement::REPORT_ONLY:
          final_action = "Report only";
          break;
        case content_analysis::sdk::ContentAnalysisAcknowledgement::WARN:
          final_action = "Warn";
          break;
        case content_analysis::sdk::ContentAnalysisAcknowledgement::BLOCK:
          final_action = "Block";
          break;
      }
    }

    std::cout << "Ack: " << ack.request_token() << std::endl;
    std::cout << "  Final action: " << final_action << std::endl;
  }
  void OnCancelRequests(
      const content_analysis::sdk::ContentAnalysisCancelRequests& cancel)
      override {
    std::cout << "Cancel: " << std::endl;
    std::cout << "  User action ID: " << cancel.user_action_id() << std::endl;
  }

  void OnInternalError(const char* context,
                       content_analysis::sdk::ResultCode error) override {
    std::cout << std::endl
              << "*ERROR*: context=\"" << context << "\" "
              << content_analysis::sdk::ResultCodeToString(error) << std::endl;
  }

  void DumpRequest(
      const content_analysis::sdk::ContentAnalysisRequest& request) {
    std::string connector = "<Unknown>";
    if (request.has_analysis_connector()) {
      switch (request.analysis_connector()) {
        case content_analysis::sdk::FILE_DOWNLOADED:
          connector = "download";
          break;
        case content_analysis::sdk::FILE_ATTACHED:
          connector = "attach";
          break;
        case content_analysis::sdk::BULK_DATA_ENTRY:
          connector = "bulk-data-entry";
          break;
        case content_analysis::sdk::PRINT:
          connector = "print";
          break;
        case content_analysis::sdk::FILE_TRANSFER:
          connector = "file-transfer";
          break;
        default:
          break;
      }
    }

    std::string url =
        request.has_request_data() && request.request_data().has_url()
            ? request.request_data().url()
            : "<No URL>";

    std::string tab_title =
        request.has_request_data() && request.request_data().has_tab_title()
            ? request.request_data().tab_title()
            : "<No tab title>";

    std::string filename =
        request.has_request_data() && request.request_data().has_filename()
            ? request.request_data().filename()
            : "<No filename>";

    std::string digest =
        request.has_request_data() && request.request_data().has_digest()
            ? request.request_data().digest()
            : "<No digest>";

    std::string file_path =
        request.has_file_path() ? request.file_path() : "<none>";

    std::string text_content =
        request.has_text_content() ? request.text_content() : "<none>";

    std::string machine_user =
        request.has_client_metadata() &&
                request.client_metadata().has_browser() &&
                request.client_metadata().browser().has_machine_user()
            ? request.client_metadata().browser().machine_user()
            : "<No machine user>";

    std::string email =
        request.has_request_data() && request.request_data().has_email()
            ? request.request_data().email()
            : "<No email>";

    time_t t = request.expires_at();

    std::string user_action_id = request.has_user_action_id()
                                     ? request.user_action_id()
                                     : "<No user action id>";

    std::cout << "Request: " << request.request_token() << std::endl;
    std::cout << "  User action ID: " << user_action_id << std::endl;
    std::cout << "  Expires at: " << ctime(&t);  // Returned string includes \n.
    std::cout << "  Connector: " << connector << std::endl;
    std::cout << "  URL: " << url << std::endl;
    std::cout << "  Tab title: " << tab_title << std::endl;
    std::cout << "  Filename: " << filename << std::endl;
    std::cout << "  Digest: " << digest << std::endl;
    std::cout << "  Filepath: " << file_path << std::endl;
    std::cout << "  Text content: '" << text_content << "'" << std::endl;
    std::cout << "  Machine user: " << machine_user << std::endl;
    std::cout << "  Email: " << email << std::endl;
  }

  bool ReadContentFromFile(const std::string& file_path, std::string* content) {
    std::ifstream file(file_path,
                       std::ios::in | std::ios::binary | std::ios::ate);
    if (!file.is_open()) return false;

    // Get file size.  This example does not handle files larger than 1MB.
    // Make sure content string can hold the contents of the file.
    int size = file.tellg();
    if (size > 1024 * 1024) return false;

    content->resize(size + 1);

    // Read file into string.
    file.seekg(0, std::ios::beg);
    file.read(&(*content)[0], size);
    content->at(size) = 0;
    return true;
  }

  bool ShouldBlockRequest(const std::string& content) {
    // Determines if the request should be blocked. (not needed for the
    // misbehaving agent)
    std::cout << "'" << content << "' was not blocked\n";
    return false;
  }

  unsigned long delay_;
  Mode mode_;
};

#endif  // CONTENT_ANALYSIS_DEMO_HANDLER_MISBEHAVING_H_

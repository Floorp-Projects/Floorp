// Copyright 2022 The Chromium Authors.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <time.h>

#include <iostream>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "content_analysis/sdk/analysis_client.h"
#include "demo/atomic_output.h"

using content_analysis::sdk::Client;
using content_analysis::sdk::ContentAnalysisRequest;
using content_analysis::sdk::ContentAnalysisResponse;
using content_analysis::sdk::ContentAnalysisAcknowledgement;

// Different paths are used depending on whether this agent should run as a
// use specific agent or not.  These values are chosen to match the test
// values in chrome browser.
constexpr char kPathUser[] = "path_user";
constexpr char kPathSystem[] = "brcm_chrm_cas";

// Global app config.
std::string path = kPathSystem;
bool user_specific = false;
bool group = false;
std::unique_ptr<Client> client;

// Paramters used to build the request.
content_analysis::sdk::AnalysisConnector connector =
    content_analysis::sdk::FILE_ATTACHED;
time_t request_token_number = time(nullptr);
std::string request_token;
std::string tag = "dlp";
bool threaded = false;
std::string digest = "sha256-123456";
std::string url = "https://upload.example.com";
std::string email = "me@example.com";
std::string machine_user = "DOMAIN\\me";
std::vector<std::string> datas;

// When grouping, remember the tokens of all requests/responses in order to
// acknowledge them all with the same final action.
//
// This global state.  It may be access from multiple thread so must be
// accessed from a critical section.
std::mutex global_mutex;
ContentAnalysisAcknowledgement::FinalAction global_final_action =
    ContentAnalysisAcknowledgement::ALLOW;
std::vector<std::string> request_tokens;

// Command line parameters.
constexpr const char* kArgConnector = "--connector=";
constexpr const char* kArgDigest = "--digest=";
constexpr const char* kArgEmail = "--email=";
constexpr const char* kArgGroup = "--group";
constexpr const char* kArgMachineUser = "--machine-user=";
constexpr const char* kArgPath = "--path=";
constexpr const char* kArgRequestToken = "--request-token=";
constexpr const char* kArgTag = "--tag=";
constexpr const char* kArgThreaded = "--threaded";
constexpr const char* kArgUrl = "--url=";
constexpr const char* kArgUserSpecific = "--user";
constexpr const char* kArgHelp = "--help";

bool ParseCommandLine(int argc, char* argv[]) {
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg.find(kArgConnector) == 0) {
      std::string connector_str = arg.substr(strlen(kArgConnector));
      if (connector_str == "download") {
        connector = content_analysis::sdk::FILE_DOWNLOADED;
      } else if (connector_str == "attach") {
        connector = content_analysis::sdk::FILE_ATTACHED;
      } else if (connector_str == "bulk-data-entry") {
        connector = content_analysis::sdk::BULK_DATA_ENTRY;
      } else if (connector_str == "print") {
        connector = content_analysis::sdk::PRINT;
      } else if (connector_str == "file-transfer") {
        connector = content_analysis::sdk::FILE_TRANSFER;
      } else {
        std::cout << "[Demo] Incorrect command line arg: " << arg << std::endl;
        return false;
      }
    } else if (arg.find(kArgRequestToken) == 0) {
      request_token = arg.substr(strlen(kArgRequestToken));
    } else if (arg.find(kArgTag) == 0) {
      tag = arg.substr(strlen(kArgTag));
    } else if (arg.find(kArgThreaded) == 0) {
      threaded = true;
    } else if (arg.find(kArgDigest) == 0) {
      digest = arg.substr(strlen(kArgDigest));
    } else if (arg.find(kArgUrl) == 0) {
      url = arg.substr(strlen(kArgUrl));
    } else if (arg.find(kArgMachineUser) == 0) {
      machine_user = arg.substr(strlen(kArgMachineUser));
    } else if (arg.find(kArgEmail) == 0) {
      email = arg.substr(strlen(kArgEmail));
    } else if (arg.find(kArgPath) == 0) {
      path = arg.substr(strlen(kArgPath));
    } else if (arg.find(kArgUserSpecific) == 0) {
      // If kArgPath was already used, abort.
      if (path != kPathSystem) {
        std::cout << std::endl << "ERROR: use --path=<path> after --user";
        return false;
      }
      path = kPathUser;
      user_specific = true;
    } else if (arg.find(kArgGroup) == 0) {
      group = true;
    } else if (arg.find(kArgHelp) == 0) {
      return false;
    } else {
      datas.push_back(arg);
    }
  }

  return true;
}

void PrintHelp() {
  std::cout
    << std::endl << std::endl
    << "Usage: client [OPTIONS] [@]content_or_file ..." << std::endl
    << "A simple client to send content analysis requests to a running agent." << std::endl
    << "Without @ the content to analyze is the argument itself." << std::endl
    << "Otherwise the content is read from a file called 'content_or_file'." << std::endl
    << "Multiple [@]content_or_file arguments may be specified, each generates one request." << std::endl
    << std::endl << "Options:"  << std::endl
    << kArgConnector << "<connector> : one of 'download', 'attach' (default), 'bulk-data-entry', 'print', or 'file-transfer'" << std::endl
    << kArgRequestToken << "<unique-token> : defaults to 'req-<number>' which auto increments" << std::endl
    << kArgTag << "<tag> : defaults to 'dlp'" << std::endl
    << kArgThreaded << " : handled multiple requests using threads" << std::endl
    << kArgUrl << "<url> : defaults to 'https://upload.example.com'" << std::endl
    << kArgMachineUser << "<machine-user> : defaults to 'DOMAIN\\me'" << std::endl
    << kArgEmail << "<email> : defaults to 'me@example.com'" << std::endl
    << kArgPath << " <path> : Used the specified path instead of default. Must come after --user." << std::endl
    << kArgUserSpecific << " : Connects to an OS user specific agent" << std::endl
    << kArgDigest << "<digest> : defaults to 'sha256-123456'" << std::endl
    << kArgGroup << " : Generate the same final action for all requests" << std::endl
    << kArgHelp << " : prints this help message" << std::endl;
}

std::string GenerateRequestToken() {
  std::stringstream stm;
  stm << "req-" << request_token_number++;
  return stm.str();
}

ContentAnalysisRequest BuildRequest(const std::string& data) {
  std::string filepath;
  std::string filename;
  if (data[0] == '@') {
    filepath = data.substr(1);
    filename = filepath.substr(filepath.find_last_of("/\\") + 1);
  }

  ContentAnalysisRequest request;

  // Set request to expire 5 minutes into the future.
  request.set_expires_at(time(nullptr) + 5 * 60);
  request.set_analysis_connector(connector);
  request.set_request_token(!request_token.empty()
      ? request_token : GenerateRequestToken());
  *request.add_tags() = tag;

  auto request_data = request.mutable_request_data();
  request_data->set_url(url);
  request_data->set_email(email);
  request_data->set_digest(digest);
  if (!filename.empty()) {
    request_data->set_filename(filename);
  }

  auto client_metadata = request.mutable_client_metadata();
  auto browser = client_metadata->mutable_browser();
  browser->set_machine_user(machine_user);

  if (!filepath.empty()) {
    request.set_file_path(filepath);
  } else if (!data.empty()) {
    request.set_text_content(data);
  } else {
    std::cout << "[Demo] Specify text content or a file path." << std::endl;
    PrintHelp();
    exit(1);
  }

  return request;
}

// Gets the most severe action within the result.
ContentAnalysisResponse::Result::TriggeredRule::Action
GetActionFromResult(const ContentAnalysisResponse::Result& result) {
  auto action =
    ContentAnalysisResponse::Result::TriggeredRule::ACTION_UNSPECIFIED;
  for (auto rule : result.triggered_rules()) {
    if (rule.has_action() && rule.action() > action)
      action = rule.action();
  }
  return action;
}

// Gets the most severe action within all the the results of a response.
ContentAnalysisResponse::Result::TriggeredRule::Action
GetActionFromResponse(const ContentAnalysisResponse& response) {
  auto action =
    ContentAnalysisResponse::Result::TriggeredRule::ACTION_UNSPECIFIED;
  for (auto result : response.results()) {
    auto action2 = GetActionFromResult(result);
    if (action2 > action)
      action = action2;
  }
  return action;
}

void DumpResponse(
    std::stringstream& stream,
    const ContentAnalysisResponse& response) {
  for (auto result : response.results()) {
    auto tag = result.has_tag() ? result.tag() : "<no-tag>";

    auto status = result.has_status()
      ? result.status()
      : ContentAnalysisResponse::Result::STATUS_UNKNOWN;
    std::string status_str;
    switch (status) {
    case ContentAnalysisResponse::Result::STATUS_UNKNOWN:
      status_str = "Unknown";
      break;
    case ContentAnalysisResponse::Result::SUCCESS:
      status_str = "Success";
      break;
    case ContentAnalysisResponse::Result::FAILURE:
      status_str = "Failure";
      break;
    default:
      status_str = "<Uknown>";
      break;
    }

    auto action = GetActionFromResult(result);
    std::string action_str;
    switch (action) {
    case ContentAnalysisResponse::Result::TriggeredRule::ACTION_UNSPECIFIED:
      action_str = "allowed";
      break;
    case ContentAnalysisResponse::Result::TriggeredRule::REPORT_ONLY:
      action_str = "reported only";
      break;
    case ContentAnalysisResponse::Result::TriggeredRule::WARN:
      action_str = "warned";
      break;
    case ContentAnalysisResponse::Result::TriggeredRule::BLOCK:
      action_str = "blocked";
      break;
    }

    time_t now = time(nullptr);
    stream << "[Demo] Request " << response.request_token()  << " is " << action_str
           << " after " << tag
           << " analysis, status=" << status_str
           << " at " << ctime(&now);
  }
}

ContentAnalysisAcknowledgement BuildAcknowledgement(
    const std::string& request_token,
    ContentAnalysisAcknowledgement::FinalAction final_action) {
  ContentAnalysisAcknowledgement ack;
  ack.set_request_token(request_token);
  ack.set_status(ContentAnalysisAcknowledgement::SUCCESS);
  ack.set_final_action(final_action);
  return ack;
}

void HandleRequest(const ContentAnalysisRequest& request) {
  AtomicCout aout;
  ContentAnalysisResponse response;
  int err = client->Send(request, &response);
  if (err != 0) {
    aout.stream() << "[Demo] Error sending request " << request.request_token()
           << std::endl;
  } else if (response.results_size() == 0) {
    aout.stream() << "[Demo] Response " << request.request_token() << " is missing a result"
                  << std::endl;
  } else {
    DumpResponse(aout.stream(), response);

    auto final_action = ContentAnalysisAcknowledgement::ALLOW;
    switch (GetActionFromResponse(response)) {
    case ContentAnalysisResponse::Result::TriggeredRule::ACTION_UNSPECIFIED:
      break;
    case ContentAnalysisResponse::Result::TriggeredRule::REPORT_ONLY:
      final_action = ContentAnalysisAcknowledgement::REPORT_ONLY;
      break;
    case ContentAnalysisResponse::Result::TriggeredRule::WARN:
      final_action = ContentAnalysisAcknowledgement::WARN;
      break;
    case ContentAnalysisResponse::Result::TriggeredRule::BLOCK:
      final_action = ContentAnalysisAcknowledgement::BLOCK;
      break;
    }

    // If grouping, remember the request's token in order to ack the response
    // later.
    if (group) {
      std::unique_lock<std::mutex> lock(global_mutex);
      request_tokens.push_back(request.request_token());
      if (final_action > global_final_action)
        global_final_action = final_action;
    } else {
      int err = client->Acknowledge(
        BuildAcknowledgement(request.request_token(), final_action));
      if (err != 0) {
        aout.stream() << "[Demo] Error sending ack " << request.request_token()
                      << std::endl;
      }
    }
  }
}

void ProcessRequest(size_t i) {
  auto request = BuildRequest(datas[i]);

  {
    AtomicCout aout;
    aout.stream() << "[Demo] Sending request " << request.request_token() << std::endl;
  }

  HandleRequest(request);
}

int main(int argc, char* argv[]) {
  if (!ParseCommandLine(argc, argv)) {
    PrintHelp();
    return 1;
  }

  // Each client uses a unique name to identify itself with Google Chrome.
  client = Client::Create({path, user_specific});
  if (!client) {
    std::cout << "[Demo] Error starting client" << std::endl;
    return 1;
  };

  auto info = client->GetAgentInfo();
  std::cout << "Agent pid=" << info.pid
            << " path=" << info.binary_path << std::endl;

  if (threaded) {
    std::vector<std::unique_ptr<std::thread>> threads;
    for (int i = 0; i < datas.size(); ++i) {
      AtomicCout aout;
      aout.stream() << "Start thread " << i << std::endl;
      threads.emplace_back(std::make_unique<std::thread>(ProcessRequest, i));
    }

    // Make sure all threads have terminated.
    for (auto& thread : threads) {
      thread->join();
    }
  }
  else {
    for (size_t i = 0; i < datas.size(); ++i) {
      ProcessRequest(i);
    }
  }
  // It's safe to access global state beyond this point without locking since
  // all no more responses will be touching them.

  if (group) {
    std::cout << std::endl;
    std::cout << "[Demo] Final action for all requests is ";
    switch (global_final_action) {
    // Google Chrome fails open, so if no action is specified that is the same
    // as ALLOW.
    case ContentAnalysisAcknowledgement::ACTION_UNSPECIFIED:
    case ContentAnalysisAcknowledgement::ALLOW:
      std::cout << "allowed";
      break;
    case ContentAnalysisAcknowledgement::REPORT_ONLY:
      std::cout << "reported only";
      break;
    case ContentAnalysisAcknowledgement::WARN:
      std::cout << "warned";
      break;
    case ContentAnalysisAcknowledgement::BLOCK:
      std::cout << "blocked";
      break;
    }
    std::cout << std::endl << std::endl;

    for (auto token : request_tokens) {
      std::cout << "[Demo] Sending group Ack" << std::endl;
      int err = client->Acknowledge(
        BuildAcknowledgement(token, global_final_action));
      if (err != 0) {
        std::cout << "[Demo] Error sending ack for " << token << std::endl;
      }
    }
  }

  return 0;
};

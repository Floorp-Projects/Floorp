// Copyright 2022 The Chromium Authors.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <fstream>
#include <iostream>
#include <string>
#include <regex>
#include <vector>

#include "content_analysis/sdk/analysis_agent.h"
#include "demo/handler.h"
#include "demo/handler_misbehaving.h"

using namespace content_analysis::sdk;

// Different paths are used depending on whether this agent should run as a
// use specific agent or not.  These values are chosen to match the test
// values in chrome browser.
constexpr char kPathUser[] = "path_user";
constexpr char kPathSystem[] = "brcm_chrm_cas";

// Global app config.
std::string path = kPathSystem;
bool use_queue = false;
bool user_specific = false;
std::vector<unsigned long> delays = {0};  // In seconds.
unsigned long num_threads = 8u;
std::string save_print_data_path = "";
RegexArray toBlock, toWarn, toReport;
static bool useMisbehavingHandler = false;
static std::string modeStr;

// Command line parameters.
constexpr const char* kArgDelaySpecific = "--delays=";
constexpr const char* kArgPath = "--path=";
constexpr const char* kArgQueued = "--queued";
constexpr const char* kArgThreads = "--threads=";
constexpr const char* kArgUserSpecific = "--user";
constexpr const char* kArgToBlock = "--toblock=";
constexpr const char* kArgToWarn = "--towarn=";
constexpr const char* kArgToReport = "--toreport=";
constexpr const char* kArgMisbehave = "--misbehave=";
constexpr const char* kArgHelp = "--help";
constexpr const char* kArgSavePrintRequestDataTo = "--save-print-request-data-to=";

std::map<std::string, Mode> sStringToMode = {
#define AGENT_MODE(name) {#name, Mode::Mode_##name},
#include "modes.h"
#undef AGENT_MODE
};

std::map<Mode, std::string> sModeToString = {
#define AGENT_MODE(name) {Mode::Mode_##name, #name},
#include "modes.h"
#undef AGENT_MODE
};

std::vector<std::pair<std::string, std::regex>>
ParseRegex(const std::string str) {
  std::vector<std::pair<std::string, std::regex>> ret;
  for (auto it = str.begin(); it != str.end(); /* nop */) {
    auto it2 = std::find(it, str.end(), ',');
    ret.push_back(std::make_pair(std::string(it, it2), std::regex(it, it2)));
    it = it2 == str.end() ? it2 : it2 + 1;
  }

  return ret;
}

bool ParseCommandLine(int argc, char* argv[]) {
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg.find(kArgUserSpecific) == 0) {
      // If kArgPath was already used, abort.
      if (path != kPathSystem) {
        std::cout << std::endl << "ERROR: use --path=<path> after --user";
        return false;
      }
      path = kPathUser;
      user_specific = true;
    } else if (arg.find(kArgDelaySpecific) == 0) {
      std::string delaysStr = arg.substr(strlen(kArgDelaySpecific));
      delays.clear();
      size_t posStart = 0, posEnd;
      unsigned long delay;
      while ((posEnd = delaysStr.find(',', posStart)) != std::string::npos) {
        delay = std::stoul(delaysStr.substr(posStart, posEnd - posStart));
        if (delay > 30) {
            delay = 30;
        }
        delays.push_back(delay);
        posStart = posEnd + 1;
      }
      delay = std::stoul(delaysStr.substr(posStart));
      if (delay > 30) {
          delay = 30;
      }
      delays.push_back(delay);
    } else if (arg.find(kArgPath) == 0) {
      path = arg.substr(strlen(kArgPath));
    } else if (arg.find(kArgQueued) == 0) {
      use_queue = true;
    } else if (arg.find(kArgThreads) == 0) {
      num_threads = std::stoul(arg.substr(strlen(kArgThreads)));
    } else if (arg.find(kArgToBlock) == 0) {
      toBlock = ParseRegex(arg.substr(strlen(kArgToBlock)));
    } else if (arg.find(kArgToWarn) == 0) {
      toWarn = ParseRegex(arg.substr(strlen(kArgToWarn)));
    } else if (arg.find(kArgToReport) == 0) {
      toReport = ParseRegex(arg.substr(strlen(kArgToReport)));
    } else if (arg.find(kArgMisbehave) == 0) {
      modeStr = arg.substr(strlen(kArgMisbehave));
      useMisbehavingHandler = true;
    } else if (arg.find(kArgHelp) == 0) {
      return false;
    } else if (arg.find(kArgSavePrintRequestDataTo) == 0) {
      int arg_len = strlen(kArgSavePrintRequestDataTo);
      save_print_data_path = arg.substr(arg_len);
    }
  }

  return true;
}

void PrintHelp() {
  std::cout
    << std::endl << std::endl
    << "Usage: agent [OPTIONS]" << std::endl
    << "A simple agent to process content analysis requests." << std::endl
    << "Data containing the string 'block' blocks the request data from being used." << std::endl
    << std::endl << "Options:"  << std::endl
    << kArgDelaySpecific << "<delay1,delay2,...> : Add delays to request processing in seconds. Delays are limited to 30 seconds and are applied round-robin to requests. Default is 0." << std::endl
    << kArgPath << " <path> : Used the specified path instead of default. Must come after --user." << std::endl
    << kArgQueued << " : Queue requests for processing in a background thread" << std::endl
    << kArgThreads << " : When queued, number of threads in the request processing thread pool" << std::endl
    << kArgUserSpecific << " : Make agent OS user specific." << std::endl
    << kArgSavePrintRequestDataTo << " : saves the PDF data to the given file path for print requests" << std::endl
    << kArgToBlock << "<regex> : Regular expression matching file and text content to block." << std::endl
    << kArgToWarn << "<regex> : Regular expression matching file and text content to warn about." << std::endl
    << kArgToReport << "<regex> : Regular expression matching file and text content to report." << std::endl
    << kArgMisbehave << "<mode> : Use 'misbehaving' agent in given mode for testing purposes." << std::endl
    << kArgHelp << " : prints this help message" << std::endl;
}

int main(int argc, char* argv[]) {
  if (!ParseCommandLine(argc, argv)) {
    PrintHelp();
    return 1;
  }

  // TODO: Add toBlock, toWarn, toReport to QueueingHandler
  auto handler =
    useMisbehavingHandler
      ? MisbehavingHandler::Create(delays[0], modeStr)
      : use_queue
        ? std::make_unique<QueuingHandler>(num_threads, std::move(delays), save_print_data_path, std::move(toBlock), std::move(toWarn), std::move(toReport))
        : std::make_unique<Handler>(std::move(delays), save_print_data_path, std::move(toBlock), std::move(toWarn), std::move(toReport));

  if (!handler) {
    std::cout << "[Demo] Failed to construct handler." << std::endl;
    return 1;
  }

  // Each agent uses a unique name to identify itself with Google Chrome.
  content_analysis::sdk::ResultCode rc;
  auto agent = content_analysis::sdk::Agent::Create(
      {path, user_specific}, std::move(handler), &rc);
  if (!agent || rc != content_analysis::sdk::ResultCode::OK) {
    std::cout << "[Demo] Error starting agent: "
              << content_analysis::sdk::ResultCodeToString(rc)
              << std::endl;
    return 1;
  };

  std::cout << "[Demo] " << agent->DebugString() << std::endl;

  // Blocks, sending events to the handler until agent->Stop() is called.
  rc = agent->HandleEvents();
  if (rc != content_analysis::sdk::ResultCode::OK) {
    std::cout << "[Demo] Error from handling events: "
              << content_analysis::sdk::ResultCodeToString(rc)
              << std::endl;
    std::cout << "[Demo] " << agent->DebugString() << std::endl;
  }

  return 0;
}

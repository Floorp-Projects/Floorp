// Copyright 2022 The Chromium Authors.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fstream>
#include <iostream>
#include <string>

#include "content_analysis/sdk/analysis_agent.h"
#include "demo/handler.h"

// Different paths are used depending on whether this agent should run as a
// use specific agent or not.  These values are chosen to match the test
// values in chrome browser.
constexpr char kPathUser[] = "path_user";
constexpr char kPathSystem[] = "brcm_chrm_cas";

// Global app config.
std::string path = kPathSystem;
bool use_queue = false;
bool user_specific = false;
unsigned long delay = 0;  // In seconds.
unsigned long num_threads = 8u;
std::string save_print_data_path = "";

// Command line parameters.
constexpr const char* kArgDelaySpecific = "--delay=";
constexpr const char* kArgPath = "--path=";
constexpr const char* kArgQueued = "--queued";
constexpr const char* kArgThreads = "--threads=";
constexpr const char* kArgUserSpecific = "--user";
constexpr const char* kArgHelp = "--help";
constexpr const char* kArgSavePrintRequestDataTo = "--save-print-request-data-to=";

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
      delay = std::stoul(arg.substr(strlen(kArgDelaySpecific)));
      if (delay > 30) {
          delay = 30;
      }
    } else if (arg.find(kArgPath) == 0) {
      path = arg.substr(strlen(kArgPath));
    } else if (arg.find(kArgQueued) == 0) {
      use_queue = true;
    } else if (arg.find(kArgThreads) == 0) {
      num_threads = std::stoul(arg.substr(strlen(kArgThreads)));
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
    << kArgDelaySpecific << "<delay> : Add a delay to request processing in seconds (max 30)." << std::endl
    << kArgPath << " <path> : Used the specified path instead of default. Must come after --user." << std::endl
    << kArgQueued << " : Queue requests for processing in a background thread" << std::endl
    << kArgThreads << " : When queued, number of threads in the request processing thread pool" << std::endl
    << kArgUserSpecific << " : Make agent OS user specific." << std::endl
    << kArgHelp << " : prints this help message" << std::endl
    << kArgSavePrintRequestDataTo << " : saves the PDF data to the given file path for print requests";
}

int main(int argc, char* argv[]) {
  if (!ParseCommandLine(argc, argv)) {
    PrintHelp();
    return 1;
  }

  auto handler = use_queue
      ? std::make_unique<QueuingHandler>(num_threads, delay, save_print_data_path)
      : std::make_unique<Handler>(delay, save_print_data_path);

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

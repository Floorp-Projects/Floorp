// Copyright 2022 The Chromium Authors.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_ANALYSIS_BROWSER_INCLUDE_CONTENT_ANALYSIS_SDK_ANALYSIS_CLIENT_H_
#define CONTENT_ANALYSIS_BROWSER_INCLUDE_CONTENT_ANALYSIS_SDK_ANALYSIS_CLIENT_H_

#include <memory>
#include <string>

#include "content_analysis/sdk/analysis.pb.h"

// This is the main include file for code using Content Analysis Connector
// Client SDK.  No other include is needed.
//
// A browser begins by creating an instance of Client using the factory
// function Client::Create().  This instance should live as long as the browser
// intends to send content analysis requests to the content analysis agent.

namespace content_analysis {
namespace sdk {

// Represents information about one instance of a content analysis agent
// process that is connected to the browser.
struct AgentInfo {
  unsigned long pid = 0;  // Process ID of content analysis agent process.
  std::string binary_path;  // The full path to the process's main binary.
};

// Represents a client that can request content analysis for locally running
// agent.  This class holds the client endpoint that the browser connects
// with when content analysis is required.
//
// See the demo directory for an example of how to use this class.
class Client {
 public:
   // Configuration options where creating an agent.  `name` is used to create
   // a channel between the agent and Google Chrome.
   struct Config {
     // Used to create a channel between the agent and Google Chrome.  Both must
     // use the same name to properly rendezvous with each other.  The channel
     // is platform specific.
     std::string name;

     // Set to true if there is a different agent instance per OS user.  Defaults
     // to false.
     bool user_specific = false;
   };

  // Returns a new client instance and calls Start().
  static std::unique_ptr<Client> Create(Config config);

  virtual ~Client() = default;

  // Returns the configuration parameters used to create the client.
  virtual const Config& GetConfig() const = 0;

  // Retrives information about the agent that is connected to the browser.
  virtual const AgentInfo& GetAgentInfo() const = 0;

  // Sends an analysis request to the agent and waits for a response.
  virtual int Send(ContentAnalysisRequest request,
                   ContentAnalysisResponse* response) = 0;

  // Sends an response acknowledgment back to the agent.
  virtual int Acknowledge(const ContentAnalysisAcknowledgement& ack) = 0;

  // Ask the agent to cancel all requests matching the criteria in `cancel`.
  // This is a best effort only, the agent may cancel some, all, or no requests
  // that match.
  virtual int CancelRequests(const ContentAnalysisCancelRequests& cancel) = 0;

 protected:
  Client() = default;
  Client(const Client& rhs) = delete;
  Client(Client&& rhs) = delete;
  Client& operator=(const Client& rhs) = delete;
  Client& operator=(Client&& rhs) = delete;
};

}  // namespace sdk
}  // namespace content_analysis

#endif  // CONTENT_ANALYSIS_BROWSER_INCLUDE_CONTENT_ANALYSIS_SDK_ANALYSIS_CLIENT_H_

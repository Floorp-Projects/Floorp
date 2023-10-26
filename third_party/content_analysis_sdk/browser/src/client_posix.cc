// Copyright 2022 The Chromium Authors.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "client_posix.h"

namespace content_analysis {
namespace sdk {

// static
std::unique_ptr<Client> Client::Create(Config config) {
  return std::make_unique<ClientPosix>(std::move(config));
}

ClientPosix::ClientPosix(Config config) : ClientBase(std::move(config)) {}

int ClientPosix::Send(ContentAnalysisRequest request,
                      ContentAnalysisResponse* response) {
  return -1;
}

int ClientPosix::Acknowledge(const ContentAnalysisAcknowledgement& ack) {
  return -1;
}

int ClientPosix::CancelRequests(const ContentAnalysisCancelRequests& cancel) {
  return -1;
}

}  // namespace sdk
}  // namespace content_analysis

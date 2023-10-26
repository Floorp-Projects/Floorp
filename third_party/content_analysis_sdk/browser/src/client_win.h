// Copyright 2022 The Chromium Authors.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_ANALYSIS_BROWSER_SRC_CLIENT_WIN_H_
#define CONTENT_ANALYSIS_BROWSER_SRC_CLIENT_WIN_H_

#include <string>

#include "client_base.h"

namespace content_analysis {
namespace sdk {

// Client implementaton for Windows.
class ClientWin : public ClientBase {
 public:
   ClientWin(Config config, int* rc);
   ~ClientWin() override;

  // Client:
  int Send(ContentAnalysisRequest request,
           ContentAnalysisResponse* response) override;
  int Acknowledge(const ContentAnalysisAcknowledgement& ack) override;
  int CancelRequests(const ContentAnalysisCancelRequests& cancel) override;

 private:
  static DWORD ConnectToPipe(const std::string& pipename, HANDLE* handle);

  // Performs a clean shutdown of the client.
  void Shutdown();

  HANDLE hPipe_ = INVALID_HANDLE_VALUE;
};

}  // namespace sdk
}  // namespace content_analysis

#endif  // CONTENT_ANALYSIS_BROWSER_SRC_CLIENT_WIN_H_

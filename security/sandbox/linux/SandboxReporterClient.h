/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_SandboxReporterClient_h
#define mozilla_SandboxReporterClient_h

#include "reporter/SandboxReporterCommon.h"

namespace mozilla {

// This class is instantiated in child processes in Sandbox.cpp to
// send reports from the SIGSYS handler to the SandboxReporter
// instance in the parent.
class SandboxReporterClient {
public:
  // Note: this does not take ownership of the file descriptor; if
  // it's not kSandboxReporterFileDesc (e.g., for unit testing), the
  // caller will need to close it to avoid leaks.
  SandboxReporterClient(SandboxReport::ProcType aProcType, int aFd);

  // This constructor uses the default fd (kSandboxReporterFileDesc)
  // for a sandboxed child process.
  explicit SandboxReporterClient(SandboxReport::ProcType aProcType);

  // Constructs a report from a signal context (the ucontext_t* passed
  // as void* to an sa_sigaction handler); uses the caller's pid and tid.
  SandboxReport MakeReport(const void* aContext);

  void SendReport(const SandboxReport& aReport);

  SandboxReport MakeReportAndSend(const void* aContext) {
    SandboxReport report = MakeReport(aContext);
    SendReport(report);
    return report;
  }
private:
  SandboxReport::ProcType mProcType;
  int mFd;
};

} // namespace mozilla

#endif // mozilla_SandboxReporterClient_h

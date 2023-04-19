#include "MozHelpers.h"

#include <iostream>

#include "gtest/gtest-spi.h"
#include "mozilla/Mutex.h"
#include "nsDebug.h"

namespace mozilla::gtest {

void DisableCrashReporter() {
  nsCOMPtr<nsICrashReporter> crashreporter =
      do_GetService("@mozilla.org/toolkit/crash-reporter;1");
  if (crashreporter) {
    crashreporter->SetEnabled(false);
  }
}

class ScopedTestResultReporterImpl
    : public ScopedTestResultReporter,
      public testing::ScopedFakeTestPartResultReporter {
 public:
  explicit ScopedTestResultReporterImpl(ExitMode aExitMode)
      : testing::ScopedFakeTestPartResultReporter(INTERCEPT_ALL_THREADS,
                                                  nullptr),
        mExitMode(aExitMode) {}

  ~ScopedTestResultReporterImpl() {
    switch (mExitMode) {
      case ExitMode::ExitOnDtor:
        exit(ExitCode(Status()));
      case ExitMode::NoExit:
        break;
    }
  }

  void ReportTestPartResult(const testing::TestPartResult& aResult) override {
    {
      MutexAutoLock lock(mMutex);
      if (aResult.nonfatally_failed() &&
          mStatus < TestResultStatus::NonFatalFailure) {
        mStatus = TestResultStatus::NonFatalFailure;
      }

      if (aResult.fatally_failed() &&
          mStatus < TestResultStatus::FatalFailure) {
        mStatus = TestResultStatus::FatalFailure;
      }
    }

    std::ostringstream stream;
    stream << aResult;
    printf_stderr("%s\n", stream.str().c_str());
  }

  TestResultStatus Status() const override {
    MutexAutoLock lock(mMutex);
    return mStatus;
  }

 private:
  const ExitMode mExitMode;

  mutable Mutex mMutex{"ScopedTestResultReporterImpl::mMutex"};
  TestResultStatus mStatus MOZ_GUARDED_BY(mMutex) = TestResultStatus::Pass;
};

UniquePtr<ScopedTestResultReporter> ScopedTestResultReporter::Create(
    ExitMode aExitMode) {
  return MakeUnique<ScopedTestResultReporterImpl>(aExitMode);
}

}  // namespace mozilla::gtest

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozISandboxReporter.h"
#include "SandboxReporter.h"

#include <time.h>

#include "mozilla/Assertions.h"
#include "mozilla/ModuleUtils.h"
#include "nsCOMPtr.h"
#include "nsPrintfCString.h"
#include "nsTArray.h"
#include "nsXULAppAPI.h"

namespace mozilla {

class SandboxReportWrapper final : public mozISandboxReport
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_MOZISANDBOXREPORT

  explicit SandboxReportWrapper(const SandboxReport& aReport)
  : mReport(aReport)
  { }

private:
  ~SandboxReportWrapper() = default;
  SandboxReport mReport;
};

NS_IMPL_ISUPPORTS(SandboxReportWrapper, mozISandboxReport)

/* readonly attribute uint64_t msecAgo; */
NS_IMETHODIMP SandboxReportWrapper::GetMsecAgo(uint64_t* aMsec)
{
  struct timespec then = mReport.mTime, now = { 0, 0 };
  clock_gettime(CLOCK_MONOTONIC_COARSE, &now);

  const uint64_t now_msec =
    uint64_t(now.tv_sec) * 1000 + now.tv_nsec / 1000000;
  const uint64_t then_msec =
    uint64_t(then.tv_sec) * 1000 + then.tv_nsec / 1000000;
  MOZ_DIAGNOSTIC_ASSERT(now_msec >= then_msec);
  if (now_msec >= then_msec) {
    *aMsec = now_msec - then_msec;
  } else {
    *aMsec = 0;
  }
  return NS_OK;
}

/* readonly attribute int32_t pid; */
NS_IMETHODIMP SandboxReportWrapper::GetPid(int32_t *aPid)
{
  *aPid = mReport.mPid;
  return NS_OK;
}

/* readonly attribute int32_t tid; */
NS_IMETHODIMP SandboxReportWrapper::GetTid(int32_t *aTid)
{
  *aTid = mReport.mTid;
  return NS_OK;
}

/* readonly attribute ACString procType; */
NS_IMETHODIMP SandboxReportWrapper::GetProcType(nsACString& aProcType)
{
  switch (mReport.mProcType) {
  case SandboxReport::ProcType::CONTENT:
    aProcType.AssignLiteral("content");
    return NS_OK;
  case SandboxReport::ProcType::FILE:
    aProcType.AssignLiteral("file");
    return NS_OK;
  case SandboxReport::ProcType::MEDIA_PLUGIN:
    aProcType.AssignLiteral("mediaPlugin");
    return NS_OK;
  default:
    MOZ_ASSERT(false);
    return NS_ERROR_UNEXPECTED;
  }
}

/* readonly attribute uint32_t syscall; */
NS_IMETHODIMP SandboxReportWrapper::GetSyscall(uint32_t *aSyscall)
{
  *aSyscall = static_cast<uint32_t>(mReport.mSyscall);
  MOZ_ASSERT(static_cast<SandboxReport::ULong>(*aSyscall) == mReport.mSyscall);
  return NS_OK;
}

/* readonly attribute uint32_t numArgs; */
NS_IMETHODIMP SandboxReportWrapper::GetNumArgs(uint32_t *aNumArgs)
{
  *aNumArgs = static_cast<uint32_t>(kSandboxSyscallArguments);
  return NS_OK;
}

/* ACString getArg (in uint32_t aIndex); */
NS_IMETHODIMP SandboxReportWrapper::GetArg(uint32_t aIndex,
					   nsACString& aRetval)
{
  if (aIndex >= kSandboxSyscallArguments) {
    return NS_ERROR_INVALID_ARG;
  }
  const auto arg = mReport.mArgs[aIndex];
  nsAutoCString str;
  // Use decimal for smaller numbers (more likely ints) and hex for
  // larger (more likely pointers).  This cutoff is arbitrary.
  if (arg >= 1000000) {
    str.AppendLiteral("0x");
    str.AppendInt(arg, 16);
  } else {
    str.AppendInt(arg, 10);
  }
  aRetval = str;
  return NS_OK;
}

class SandboxReportArray final : public mozISandboxReportArray
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_MOZISANDBOXREPORTARRAY

  explicit SandboxReportArray(SandboxReporter::Snapshot&& aSnap)
  : mOffset(aSnap.mOffset)
  , mArray(std::move(aSnap.mReports))
  { }

private:
  ~SandboxReportArray() = default;
  uint64_t mOffset;
  nsTArray<SandboxReport> mArray;
};

NS_IMPL_ISUPPORTS(SandboxReportArray, mozISandboxReportArray)

/* readonly attribute uint64_t begin; */
NS_IMETHODIMP SandboxReportArray::GetBegin(uint64_t *aBegin)
{
  *aBegin = mOffset;
  return NS_OK;
}

/* readonly attribute uint64_t end; */
NS_IMETHODIMP SandboxReportArray::GetEnd(uint64_t *aEnd)
{
  *aEnd = mOffset + mArray.Length();
  return NS_OK;
}

/* mozISandboxReport getElement (in uint64_t aIndex); */
NS_IMETHODIMP SandboxReportArray::GetElement(uint64_t aIndex, mozISandboxReport ** aRetval)
{
  uint64_t relIndex = aIndex - mOffset;
  if (relIndex >= mArray.Length()) {
    return NS_ERROR_INVALID_ARG;
  }

  nsCOMPtr<mozISandboxReport> wrapper =
    new SandboxReportWrapper(mArray[relIndex]);
  wrapper.forget(aRetval);
  return NS_OK;
}

class SandboxReporterWrapper final : public mozISandboxReporter
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_MOZISANDBOXREPORTER

  SandboxReporterWrapper() = default;

private:
  ~SandboxReporterWrapper() = default;
};

NS_IMPL_ISUPPORTS(SandboxReporterWrapper, mozISandboxReporter)

/* mozISandboxReportArray snapshot(); */
NS_IMETHODIMP SandboxReporterWrapper::Snapshot(mozISandboxReportArray** aRetval)
{
  if (!XRE_IsParentProcess()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  nsCOMPtr<mozISandboxReportArray> wrapper =
    new SandboxReportArray(SandboxReporter::Singleton()->GetSnapshot());
  wrapper.forget(aRetval);
  return NS_OK;
}

NS_GENERIC_FACTORY_CONSTRUCTOR(SandboxReporterWrapper)

NS_DEFINE_NAMED_CID(MOZ_SANDBOX_REPORTER_CID);

static const mozilla::Module::CIDEntry kSandboxReporterCIDs[] = {
  { &kMOZ_SANDBOX_REPORTER_CID, false, nullptr,
    SandboxReporterWrapperConstructor },
  { nullptr }
};

static const mozilla::Module::ContractIDEntry kSandboxReporterContracts[] = {
  { MOZ_SANDBOX_REPORTER_CONTRACTID, &kMOZ_SANDBOX_REPORTER_CID },
  { nullptr }
};

static const mozilla::Module kSandboxReporterModule = {
  mozilla::Module::kVersion,
  kSandboxReporterCIDs,
  kSandboxReporterContracts
};

NSMODULE_DEFN(SandboxReporterModule) = &kSandboxReporterModule;

} // namespace mozilla

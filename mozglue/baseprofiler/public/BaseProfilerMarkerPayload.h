/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef BaseProfilerMarkerPayload_h
#define BaseProfilerMarkerPayload_h

#include "BaseProfiler.h"

#ifndef MOZ_BASE_PROFILER
#  error Do not #include this header when MOZ_BASE_PROFILER is not #defined.
#endif

#include "mozilla/Attributes.h"
#include "mozilla/Maybe.h"
#include "mozilla/RefPtr.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/UniquePtrExtensions.h"

class SpliceableJSONWriter;
class UniqueStacks;

// This is an abstract class that can be implemented to supply data to be
// attached with a profiler marker.
//
// When subclassing this, note that the destructor can be called on any thread,
// i.e. not necessarily on the thread that created the object.
class ProfilerMarkerPayload {
 public:
  explicit ProfilerMarkerPayload(
      const mozilla::Maybe<std::string>& aDocShellId = mozilla::Nothing(),
      const mozilla::Maybe<uint32_t>& aDocShellHistoryId = mozilla::Nothing(),
      UniqueProfilerBacktrace aStack = nullptr)
      : mStack(std::move(aStack)),
        mDocShellId(aDocShellId),
        mDocShellHistoryId(aDocShellHistoryId) {}

  ProfilerMarkerPayload(
      const mozilla::TimeStamp& aStartTime, const mozilla::TimeStamp& aEndTime,
      const mozilla::Maybe<std::string>& aDocShellId = mozilla::Nothing(),
      const mozilla::Maybe<uint32_t>& aDocShellHistoryId = mozilla::Nothing(),
      UniqueProfilerBacktrace aStack = nullptr)
      : mStartTime(aStartTime),
        mEndTime(aEndTime),
        mStack(std::move(aStack)),
        mDocShellId(aDocShellId),
        mDocShellHistoryId(aDocShellHistoryId) {}

  virtual ~ProfilerMarkerPayload() {}

  virtual void StreamPayload(SpliceableJSONWriter& aWriter,
                             const mozilla::TimeStamp& aProcessStartTime,
                             UniqueStacks& aUniqueStacks) = 0;

  mozilla::TimeStamp GetStartTime() const { return mStartTime; }

 protected:
  MFBT_API void StreamType(const char* aMarkerType,
                           SpliceableJSONWriter& aWriter);
  MFBT_API void StreamCommonProps(const char* aMarkerType,
                                  SpliceableJSONWriter& aWriter,
                                  const mozilla::TimeStamp& aProcessStartTime,
                                  UniqueStacks& aUniqueStacks);

  void SetStack(UniqueProfilerBacktrace aStack) { mStack = std::move(aStack); }

  void SetDocShellHistoryId(
      const mozilla::Maybe<uint32_t>& aDocShellHistoryId) {
    mDocShellHistoryId = aDocShellHistoryId;
  }

  void SetDocShellId(const mozilla::Maybe<std::string>& aDocShellId) {
    mDocShellId = aDocShellId;
  }

 private:
  mozilla::TimeStamp mStartTime;
  mozilla::TimeStamp mEndTime;
  UniqueProfilerBacktrace mStack;
  mozilla::Maybe<std::string> mDocShellId;
  mozilla::Maybe<uint32_t> mDocShellHistoryId;
};

#define DECL_BASE_STREAM_PAYLOAD                                          \
  virtual void StreamPayload(SpliceableJSONWriter& aWriter,               \
                             const mozilla::TimeStamp& aProcessStartTime, \
                             UniqueStacks& aUniqueStacks) override;

// TODO: Increase the coverage of tracing markers that include DocShell
// information
class TracingMarkerPayload : public ProfilerMarkerPayload {
 public:
  TracingMarkerPayload(
      const char* aCategory, TracingKind aKind,
      const mozilla::Maybe<std::string>& aDocShellId = mozilla::Nothing(),
      const mozilla::Maybe<uint32_t>& aDocShellHistoryId = mozilla::Nothing(),
      UniqueProfilerBacktrace aCause = nullptr)
      : mCategory(aCategory), mKind(aKind) {
    if (aCause) {
      SetStack(std::move(aCause));
    }
    SetDocShellId(aDocShellId);
    SetDocShellHistoryId(aDocShellHistoryId);
  }

  DECL_BASE_STREAM_PAYLOAD

 private:
  const char* mCategory;
  TracingKind mKind;
};

class FileIOMarkerPayload : public ProfilerMarkerPayload {
 public:
  FileIOMarkerPayload(const char* aOperation, const char* aSource,
                      const char* aFilename,
                      const mozilla::TimeStamp& aStartTime,
                      const mozilla::TimeStamp& aEndTime,
                      UniqueProfilerBacktrace aStack)
      : ProfilerMarkerPayload(aStartTime, aEndTime, mozilla::Nothing(),
                              mozilla::Nothing(), std::move(aStack)),
        mSource(aSource),
        mOperation(aOperation ? strdup(aOperation) : nullptr),
        mFilename(aFilename ? strdup(aFilename) : nullptr) {
    MOZ_ASSERT(aSource);
  }

  DECL_BASE_STREAM_PAYLOAD

 private:
  const char* mSource;
  mozilla::UniqueFreePtr<char> mOperation;
  mozilla::UniqueFreePtr<char> mFilename;
};

class UserTimingMarkerPayload : public ProfilerMarkerPayload {
 public:
  UserTimingMarkerPayload(const std::string& aName,
                          const mozilla::TimeStamp& aStartTime,
                          const mozilla::Maybe<std::string>& aDocShellId,
                          const mozilla::Maybe<uint32_t>& aDocShellHistoryId)
      : ProfilerMarkerPayload(aStartTime, aStartTime, aDocShellId,
                              aDocShellHistoryId),
        mEntryType("mark"),
        mName(aName) {}

  UserTimingMarkerPayload(const std::string& aName,
                          const mozilla::Maybe<std::string>& aStartMark,
                          const mozilla::Maybe<std::string>& aEndMark,
                          const mozilla::TimeStamp& aStartTime,
                          const mozilla::TimeStamp& aEndTime,
                          const mozilla::Maybe<std::string>& aDocShellId,
                          const mozilla::Maybe<uint32_t>& aDocShellHistoryId)
      : ProfilerMarkerPayload(aStartTime, aEndTime, aDocShellId,
                              aDocShellHistoryId),
        mEntryType("measure"),
        mName(aName),
        mStartMark(aStartMark),
        mEndMark(aEndMark) {}

  DECL_BASE_STREAM_PAYLOAD

 private:
  // Either "mark" or "measure".
  const char* mEntryType;
  std::string mName;
  mozilla::Maybe<std::string> mStartMark;
  mozilla::Maybe<std::string> mEndMark;
};

class HangMarkerPayload : public ProfilerMarkerPayload {
 public:
  HangMarkerPayload(const mozilla::TimeStamp& aStartTime,
                    const mozilla::TimeStamp& aEndTime)
      : ProfilerMarkerPayload(aStartTime, aEndTime) {}

  DECL_BASE_STREAM_PAYLOAD
 private:
};

class LongTaskMarkerPayload : public ProfilerMarkerPayload {
 public:
  LongTaskMarkerPayload(const mozilla::TimeStamp& aStartTime,
                        const mozilla::TimeStamp& aEndTime)
      : ProfilerMarkerPayload(aStartTime, aEndTime) {}

  DECL_BASE_STREAM_PAYLOAD
};

class TextMarkerPayload : public ProfilerMarkerPayload {
 public:
  TextMarkerPayload(const std::string& aText,
                    const mozilla::TimeStamp& aStartTime)
      : ProfilerMarkerPayload(aStartTime, aStartTime), mText(aText) {}

  TextMarkerPayload(const std::string& aText,
                    const mozilla::TimeStamp& aStartTime,
                    const mozilla::TimeStamp& aEndTime)
      : ProfilerMarkerPayload(aStartTime, aEndTime), mText(aText) {}

  TextMarkerPayload(const std::string& aText,
                    const mozilla::TimeStamp& aStartTime,
                    const mozilla::Maybe<std::string>& aDocShellId,
                    const mozilla::Maybe<uint32_t>& aDocShellHistoryId)
      : ProfilerMarkerPayload(aStartTime, aStartTime, aDocShellId,
                              aDocShellHistoryId),
        mText(aText) {}

  TextMarkerPayload(const std::string& aText,
                    const mozilla::TimeStamp& aStartTime,
                    const mozilla::TimeStamp& aEndTime,
                    const mozilla::Maybe<std::string>& aDocShellId,
                    const mozilla::Maybe<uint32_t>& aDocShellHistoryId,
                    UniqueProfilerBacktrace aCause = nullptr)
      : ProfilerMarkerPayload(aStartTime, aEndTime, aDocShellId,
                              aDocShellHistoryId, std::move(aCause)),
        mText(aText) {}

  DECL_BASE_STREAM_PAYLOAD

 private:
  std::string mText;
};

class LogMarkerPayload : public ProfilerMarkerPayload {
 public:
  LogMarkerPayload(const char* aModule, const char* aText,
                   const mozilla::TimeStamp& aStartTime)
      : ProfilerMarkerPayload(aStartTime, aStartTime),
        mModule(aModule),
        mText(aText) {}

  DECL_BASE_STREAM_PAYLOAD

 private:
  std::string mModule;  // longest known LazyLogModule name is ~24
  std::string mText;
};

#endif  // BaseProfilerMarkerPayload_h

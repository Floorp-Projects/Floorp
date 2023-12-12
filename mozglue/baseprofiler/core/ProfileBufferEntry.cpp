/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ProfileBufferEntry.h"

#include <ostream>
#include <type_traits>

#include "mozilla/Logging.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/Sprintf.h"
#include "mozilla/StackWalk.h"

#include "BaseProfiler.h"
#include "mozilla/BaseProfilerMarkers.h"
#include "platform.h"
#include "ProfileBuffer.h"
#include "ProfilerBacktrace.h"

namespace mozilla {
namespace baseprofiler {

////////////////////////////////////////////////////////////////////////
// BEGIN ProfileBufferEntry

ProfileBufferEntry::ProfileBufferEntry()
    : mKind(Kind::INVALID), mStorage{0, 0, 0, 0, 0, 0, 0, 0} {}

// aString must be a static string.
ProfileBufferEntry::ProfileBufferEntry(Kind aKind, const char* aString)
    : mKind(aKind) {
  memcpy(mStorage, &aString, sizeof(aString));
}

ProfileBufferEntry::ProfileBufferEntry(Kind aKind, char aChars[kNumChars])
    : mKind(aKind) {
  memcpy(mStorage, aChars, kNumChars);
}

ProfileBufferEntry::ProfileBufferEntry(Kind aKind, void* aPtr) : mKind(aKind) {
  memcpy(mStorage, &aPtr, sizeof(aPtr));
}

ProfileBufferEntry::ProfileBufferEntry(Kind aKind, double aDouble)
    : mKind(aKind) {
  memcpy(mStorage, &aDouble, sizeof(aDouble));
}

ProfileBufferEntry::ProfileBufferEntry(Kind aKind, int aInt) : mKind(aKind) {
  memcpy(mStorage, &aInt, sizeof(aInt));
}

ProfileBufferEntry::ProfileBufferEntry(Kind aKind, int64_t aInt64)
    : mKind(aKind) {
  memcpy(mStorage, &aInt64, sizeof(aInt64));
}

ProfileBufferEntry::ProfileBufferEntry(Kind aKind, uint64_t aUint64)
    : mKind(aKind) {
  memcpy(mStorage, &aUint64, sizeof(aUint64));
}

ProfileBufferEntry::ProfileBufferEntry(Kind aKind,
                                       BaseProfilerThreadId aThreadId)
    : mKind(aKind) {
  static_assert(std::is_trivially_copyable_v<BaseProfilerThreadId>);
  static_assert(sizeof(aThreadId) <= sizeof(mStorage));
  memcpy(mStorage, &aThreadId, sizeof(aThreadId));
}

const char* ProfileBufferEntry::GetString() const {
  const char* result;
  memcpy(&result, mStorage, sizeof(result));
  return result;
}

void* ProfileBufferEntry::GetPtr() const {
  void* result;
  memcpy(&result, mStorage, sizeof(result));
  return result;
}

double ProfileBufferEntry::GetDouble() const {
  double result;
  memcpy(&result, mStorage, sizeof(result));
  return result;
}

int ProfileBufferEntry::GetInt() const {
  int result;
  memcpy(&result, mStorage, sizeof(result));
  return result;
}

int64_t ProfileBufferEntry::GetInt64() const {
  int64_t result;
  memcpy(&result, mStorage, sizeof(result));
  return result;
}

uint64_t ProfileBufferEntry::GetUint64() const {
  uint64_t result;
  memcpy(&result, mStorage, sizeof(result));
  return result;
}

BaseProfilerThreadId ProfileBufferEntry::GetThreadId() const {
  BaseProfilerThreadId result;
  static_assert(std::is_trivially_copyable_v<BaseProfilerThreadId>);
  memcpy(&result, mStorage, sizeof(result));
  return result;
}

void ProfileBufferEntry::CopyCharsInto(char (&aOutArray)[kNumChars]) const {
  memcpy(aOutArray, mStorage, kNumChars);
}

// END ProfileBufferEntry
////////////////////////////////////////////////////////////////////////

// As mentioned in ProfileBufferEntry.h, the JSON format contains many
// arrays whose elements are laid out according to various schemas to help
// de-duplication. This RAII class helps write these arrays by keeping track of
// the last non-null element written and adding the appropriate number of null
// elements when writing new non-null elements. It also automatically opens and
// closes an array element on the given JSON writer.
//
// You grant the AutoArraySchemaWriter exclusive access to the JSONWriter and
// the UniqueJSONStrings objects for the lifetime of AutoArraySchemaWriter. Do
// not access them independently while the AutoArraySchemaWriter is alive.
// If you need to add complex objects, call FreeFormElement(), which will give
// you temporary access to the writer.
//
// Example usage:
//
//     // Define the schema of elements in this type of array: [FOO, BAR, BAZ]
//     enum Schema : uint32_t {
//       FOO = 0,
//       BAR = 1,
//       BAZ = 2
//     };
//
//     AutoArraySchemaWriter writer(someJsonWriter, someUniqueStrings);
//     if (shouldWriteFoo) {
//       writer.IntElement(FOO, getFoo());
//     }
//     ... etc ...
//
//     The elements need to be added in-order.
class MOZ_RAII AutoArraySchemaWriter {
 public:
  explicit AutoArraySchemaWriter(SpliceableJSONWriter& aWriter)
      : mJSONWriter(aWriter), mNextFreeIndex(0) {
    mJSONWriter.StartArrayElement();
  }

  ~AutoArraySchemaWriter() { mJSONWriter.EndArray(); }

  template <typename T>
  void IntElement(uint32_t aIndex, T aValue) {
    static_assert(!std::is_same_v<T, uint64_t>,
                  "Narrowing uint64 -> int64 conversion not allowed");
    FillUpTo(aIndex);
    mJSONWriter.IntElement(static_cast<int64_t>(aValue));
  }

  void DoubleElement(uint32_t aIndex, double aValue) {
    FillUpTo(aIndex);
    mJSONWriter.DoubleElement(aValue);
  }

  void TimeMsElement(uint32_t aIndex, double aTime_ms) {
    FillUpTo(aIndex);
    mJSONWriter.TimeDoubleMsElement(aTime_ms);
  }

  void BoolElement(uint32_t aIndex, bool aValue) {
    FillUpTo(aIndex);
    mJSONWriter.BoolElement(aValue);
  }

 protected:
  SpliceableJSONWriter& Writer() { return mJSONWriter; }

  void FillUpTo(uint32_t aIndex) {
    MOZ_ASSERT(aIndex >= mNextFreeIndex);
    mJSONWriter.NullElements(aIndex - mNextFreeIndex);
    mNextFreeIndex = aIndex + 1;
  }

 private:
  SpliceableJSONWriter& mJSONWriter;
  uint32_t mNextFreeIndex;
};

// Same as AutoArraySchemaWriter, but this can also write strings (output as
// indexes into the table of unique strings).
class MOZ_RAII AutoArraySchemaWithStringsWriter : public AutoArraySchemaWriter {
 public:
  AutoArraySchemaWithStringsWriter(SpliceableJSONWriter& aWriter,
                                   UniqueJSONStrings& aStrings)
      : AutoArraySchemaWriter(aWriter), mStrings(aStrings) {}

  void StringElement(uint32_t aIndex, const Span<const char>& aValue) {
    FillUpTo(aIndex);
    mStrings.WriteElement(Writer(), aValue);
  }

 private:
  UniqueJSONStrings& mStrings;
};

UniqueStacks::StackKey UniqueStacks::BeginStack(const FrameKey& aFrame) {
  return StackKey(GetOrAddFrameIndex(aFrame));
}

UniqueStacks::StackKey UniqueStacks::AppendFrame(const StackKey& aStack,
                                                 const FrameKey& aFrame) {
  return StackKey(aStack, GetOrAddStackIndex(aStack),
                  GetOrAddFrameIndex(aFrame));
}

bool UniqueStacks::FrameKey::NormalFrameData::operator==(
    const NormalFrameData& aOther) const {
  return mLocation == aOther.mLocation &&
         mRelevantForJS == aOther.mRelevantForJS &&
         mInnerWindowID == aOther.mInnerWindowID && mLine == aOther.mLine &&
         mColumn == aOther.mColumn && mCategoryPair == aOther.mCategoryPair;
}

UniqueStacks::UniqueStacks()
    : mUniqueStrings(MakeUnique<UniqueJSONStrings>(
          FailureLatchInfallibleSource::Singleton())),
      mFrameTableWriter(FailureLatchInfallibleSource::Singleton()),
      mStackTableWriter(FailureLatchInfallibleSource::Singleton()) {
  mFrameTableWriter.StartBareList();
  mStackTableWriter.StartBareList();
}

uint32_t UniqueStacks::GetOrAddStackIndex(const StackKey& aStack) {
  uint32_t count = mStackToIndexMap.count();
  auto entry = mStackToIndexMap.lookupForAdd(aStack);
  if (entry) {
    MOZ_ASSERT(entry->value() < count);
    return entry->value();
  }

  MOZ_RELEASE_ASSERT(mStackToIndexMap.add(entry, aStack, count));
  StreamStack(aStack);
  return count;
}

uint32_t UniqueStacks::GetOrAddFrameIndex(const FrameKey& aFrame) {
  uint32_t count = mFrameToIndexMap.count();
  auto entry = mFrameToIndexMap.lookupForAdd(aFrame);
  if (entry) {
    MOZ_ASSERT(entry->value() < count);
    return entry->value();
  }

  MOZ_RELEASE_ASSERT(mFrameToIndexMap.add(entry, aFrame, count));
  StreamNonJITFrame(aFrame);
  return count;
}

void UniqueStacks::SpliceFrameTableElements(SpliceableJSONWriter& aWriter) {
  mFrameTableWriter.EndBareList();
  aWriter.TakeAndSplice(mFrameTableWriter.TakeChunkedWriteFunc());
}

void UniqueStacks::SpliceStackTableElements(SpliceableJSONWriter& aWriter) {
  mStackTableWriter.EndBareList();
  aWriter.TakeAndSplice(mStackTableWriter.TakeChunkedWriteFunc());
}

void UniqueStacks::StreamStack(const StackKey& aStack) {
  enum Schema : uint32_t { PREFIX = 0, FRAME = 1 };

  AutoArraySchemaWriter writer(mStackTableWriter);
  if (aStack.mPrefixStackIndex.isSome()) {
    writer.IntElement(PREFIX, *aStack.mPrefixStackIndex);
  }
  writer.IntElement(FRAME, aStack.mFrameIndex);
}

void UniqueStacks::StreamNonJITFrame(const FrameKey& aFrame) {
  using NormalFrameData = FrameKey::NormalFrameData;

  enum Schema : uint32_t {
    LOCATION = 0,
    RELEVANT_FOR_JS = 1,
    INNER_WINDOW_ID = 2,
    IMPLEMENTATION = 3,
    LINE = 4,
    COLUMN = 5,
    CATEGORY = 6,
    SUBCATEGORY = 7
  };

  AutoArraySchemaWithStringsWriter writer(mFrameTableWriter, *mUniqueStrings);

  const NormalFrameData& data = aFrame.mData.as<NormalFrameData>();
  writer.StringElement(LOCATION, data.mLocation);
  writer.BoolElement(RELEVANT_FOR_JS, data.mRelevantForJS);

  // It's okay to convert uint64_t to double here because DOM always creates IDs
  // that are convertible to double.
  writer.DoubleElement(INNER_WINDOW_ID, data.mInnerWindowID);

  if (data.mLine.isSome()) {
    writer.IntElement(LINE, *data.mLine);
  }
  if (data.mColumn.isSome()) {
    writer.IntElement(COLUMN, *data.mColumn);
  }
  if (data.mCategoryPair.isSome()) {
    const ProfilingCategoryPairInfo& info =
        GetProfilingCategoryPairInfo(*data.mCategoryPair);
    writer.IntElement(CATEGORY, uint32_t(info.mCategory));
    writer.IntElement(SUBCATEGORY, info.mSubcategoryIndex);
  }
}

struct ProfileSample {
  uint32_t mStack;
  double mTime;
  Maybe<double> mResponsiveness;
};

static void WriteSample(SpliceableJSONWriter& aWriter,
                        const ProfileSample& aSample) {
  enum Schema : uint32_t {
    STACK = 0,
    TIME = 1,
    EVENT_DELAY = 2,
  };

  AutoArraySchemaWriter writer(aWriter);

  writer.IntElement(STACK, aSample.mStack);

  writer.TimeMsElement(TIME, aSample.mTime);

  if (aSample.mResponsiveness.isSome()) {
    writer.DoubleElement(EVENT_DELAY, *aSample.mResponsiveness);
  }
}

class EntryGetter {
 public:
  explicit EntryGetter(ProfileChunkedBuffer::Reader& aReader,
                       uint64_t aInitialReadPos = 0)
      : mBlockIt(
            aReader.At(ProfileBufferBlockIndex::CreateFromProfileBufferIndex(
                aInitialReadPos))),
        mBlockItEnd(aReader.end()) {
    if (!ReadLegacyOrEnd()) {
      // Find and read the next non-legacy entry.
      Next();
    }
  }

  bool Has() const { return mBlockIt != mBlockItEnd; }

  const ProfileBufferEntry& Get() const {
    MOZ_ASSERT(Has(), "Caller should have checked `Has()` before `Get()`");
    return mEntry;
  }

  void Next() {
    MOZ_ASSERT(Has(), "Caller should have checked `Has()` before `Next()`");
    for (;;) {
      ++mBlockIt;
      if (ReadLegacyOrEnd()) {
        // Either we're at the end, or we could read a legacy entry -> Done.
        break;
      }
      // Otherwise loop around until we hit the end or a legacy entry.
    }
  }

  ProfileBufferBlockIndex CurBlockIndex() const {
    return mBlockIt.CurrentBlockIndex();
  }

  uint64_t CurPos() const {
    return CurBlockIndex().ConvertToProfileBufferIndex();
  }

 private:
  // Try to read the entry at the current `mBlockIt` position.
  // * If we're at the end of the buffer, just return `true`.
  // * If there is a "legacy" entry (containing a real `ProfileBufferEntry`),
  //   read it into `mEntry`, and return `true` as well.
  // * Otherwise the entry contains a "modern" type that cannot be read into
  // `mEntry`, return `false` (so `EntryGetter` can skip to another entry).
  bool ReadLegacyOrEnd() {
    if (!Has()) {
      return true;
    }
    // Read the entry "kind", which is always at the start of all entries.
    ProfileBufferEntryReader aER = *mBlockIt;
    auto type = static_cast<ProfileBufferEntry::Kind>(
        aER.ReadObject<ProfileBufferEntry::KindUnderlyingType>());
    MOZ_ASSERT(static_cast<ProfileBufferEntry::KindUnderlyingType>(type) <
               static_cast<ProfileBufferEntry::KindUnderlyingType>(
                   ProfileBufferEntry::Kind::MODERN_LIMIT));
    if (type >= ProfileBufferEntry::Kind::LEGACY_LIMIT) {
      aER.SetRemainingBytes(0);
      return false;
    }
    // Here, we have a legacy item, we need to read it from the start.
    // Because the above `ReadObject` moved the reader, we ned to reset it to
    // the start of the entry before reading the whole entry.
    aER = *mBlockIt;
    aER.ReadBytes(&mEntry, aER.RemainingBytes());
    return true;
  }

  ProfileBufferEntry mEntry;
  ProfileChunkedBuffer::BlockIterator mBlockIt;
  const ProfileChunkedBuffer::BlockIterator mBlockItEnd;
};

// The following grammar shows legal sequences of profile buffer entries.
// The sequences beginning with a ThreadId entry are known as "samples".
//
// (
//   ( /* Samples */
//     ThreadId
//     Time
//     ( NativeLeafAddr
//     | Label FrameFlags? DynamicStringFragment* LineNumber? CategoryPair?
//     | JitReturnAddr
//     )+
//     Responsiveness?
//   )
//   | MarkerData
//   | ( /* Counters */
//       CounterId
//       Time
//       (
//         CounterKey
//         Count
//         Number?
//       )*
//     )
//   | CollectionStart
//   | CollectionEnd
//   | Pause
//   | Resume
//   | ( ProfilerOverheadTime /* Sampling start timestamp */
//       ProfilerOverheadDuration /* Lock acquisition */
//       ProfilerOverheadDuration /* Expired data cleaning */
//       ProfilerOverheadDuration /* Counters */
//       ProfilerOverheadDuration /* Threads */
//     )
// )*
//
// The most complicated part is the stack entry sequence that begins with
// Label. Here are some examples.
//
// - ProfilingStack frames without a dynamic string:
//
//     Label("js::RunScript")
//     CategoryPair(ProfilingCategoryPair::JS)
//
//     Label("XREMain::XRE_main")
//     LineNumber(4660)
//     CategoryPair(ProfilingCategoryPair::OTHER)
//
//     Label("ElementRestyler::ComputeStyleChangeFor")
//     LineNumber(3003)
//     CategoryPair(ProfilingCategoryPair::CSS)
//
// - ProfilingStack frames with a dynamic string:
//
//     Label("nsObserverService::NotifyObservers")
//     FrameFlags(uint64_t(ProfilingStackFrame::Flags::IS_LABEL_FRAME))
//     DynamicStringFragment("domwindo")
//     DynamicStringFragment("wopened")
//     LineNumber(291)
//     CategoryPair(ProfilingCategoryPair::OTHER)
//
//     Label("")
//     FrameFlags(uint64_t(ProfilingStackFrame::Flags::IS_JS_FRAME))
//     DynamicStringFragment("closeWin")
//     DynamicStringFragment("dow (chr")
//     DynamicStringFragment("ome://gl")
//     DynamicStringFragment("obal/con")
//     DynamicStringFragment("tent/glo")
//     DynamicStringFragment("balOverl")
//     DynamicStringFragment("ay.js:5)")
//     DynamicStringFragment("")          # this string holds the closing '\0'
//     LineNumber(25)
//     CategoryPair(ProfilingCategoryPair::JS)
//
//     Label("")
//     FrameFlags(uint64_t(ProfilingStackFrame::Flags::IS_JS_FRAME))
//     DynamicStringFragment("bound (s")
//     DynamicStringFragment("elf-host")
//     DynamicStringFragment("ed:914)")
//     LineNumber(945)
//     CategoryPair(ProfilingCategoryPair::JS)
//
// - A profiling stack frame with an overly long dynamic string:
//
//     Label("")
//     FrameFlags(uint64_t(ProfilingStackFrame::Flags::IS_LABEL_FRAME))
//     DynamicStringFragment("(too lon")
//     DynamicStringFragment("g)")
//     LineNumber(100)
//     CategoryPair(ProfilingCategoryPair::NETWORK)
//
// - A wasm JIT frame:
//
//     Label("")
//     FrameFlags(uint64_t(0))
//     DynamicStringFragment("wasm-fun")
//     DynamicStringFragment("ction[87")
//     DynamicStringFragment("36] (blo")
//     DynamicStringFragment("b:http:/")
//     DynamicStringFragment("/webasse")
//     DynamicStringFragment("mbly.org")
//     DynamicStringFragment("/3dc5759")
//     DynamicStringFragment("4-ce58-4")
//     DynamicStringFragment("626-975b")
//     DynamicStringFragment("-08ad116")
//     DynamicStringFragment("30bc1:38")
//     DynamicStringFragment("29856)")
//
// - A JS frame in a synchronous sample:
//
//     Label("")
//     FrameFlags(uint64_t(ProfilingStackFrame::Flags::IS_LABEL_FRAME))
//     DynamicStringFragment("u (https")
//     DynamicStringFragment("://perf-")
//     DynamicStringFragment("html.io/")
//     DynamicStringFragment("ac0da204")
//     DynamicStringFragment("aaa44d75")
//     DynamicStringFragment("a800.bun")
//     DynamicStringFragment("dle.js:2")
//     DynamicStringFragment("5)")

// Because this is a format entirely internal to the Profiler, any parsing
// error indicates a bug in the ProfileBuffer writing or the parser itself,
// or possibly flaky hardware.
#define ERROR_AND_CONTINUE(msg)                            \
  {                                                        \
    fprintf(stderr, "ProfileBuffer parse error: %s", msg); \
    MOZ_ASSERT(false, msg);                                \
    continue;                                              \
  }

BaseProfilerThreadId ProfileBuffer::StreamSamplesToJSON(
    SpliceableJSONWriter& aWriter, BaseProfilerThreadId aThreadId,
    double aSinceTime, UniqueStacks& aUniqueStacks) const {
  UniquePtr<char[]> dynStrBuf = MakeUnique<char[]>(kMaxFrameKeyLength);

  return mEntries.Read([&](ProfileChunkedBuffer::Reader* aReader) {
    MOZ_ASSERT(aReader,
               "ProfileChunkedBuffer cannot be out-of-session when sampler is "
               "running");

    BaseProfilerThreadId processedThreadId;

    EntryGetter e(*aReader);

    for (;;) {
      // This block skips entries until we find the start of the next sample.
      // This is useful in three situations.
      //
      // - The circular buffer overwrites old entries, so when we start parsing
      //   we might be in the middle of a sample, and we must skip forward to
      //   the start of the next sample.
      //
      // - We skip samples that don't have an appropriate ThreadId or Time.
      //
      // - We skip range Pause, Resume, CollectionStart, Counter and
      //   CollectionEnd entries between samples.
      while (e.Has()) {
        if (e.Get().IsThreadId()) {
          break;
        }
        e.Next();
      }

      if (!e.Has()) {
        break;
      }

      // Due to the skip_to_next_sample block above, if we have an entry here it
      // must be a ThreadId entry.
      MOZ_ASSERT(e.Get().IsThreadId());

      BaseProfilerThreadId threadId = e.Get().GetThreadId();
      e.Next();

      // Ignore samples that are for the wrong thread.
      if (threadId != aThreadId && aThreadId.IsSpecified()) {
        continue;
      }

      MOZ_ASSERT(
          aThreadId.IsSpecified() || !processedThreadId.IsSpecified(),
          "Unspecified aThreadId should only be used with 1-sample buffer");

      ProfileSample sample;

      if (e.Has() && e.Get().IsTime()) {
        sample.mTime = e.Get().GetDouble();
        e.Next();

        // Ignore samples that are too old.
        if (sample.mTime < aSinceTime) {
          continue;
        }
      } else {
        ERROR_AND_CONTINUE("expected a Time entry");
      }

      UniqueStacks::StackKey stack =
          aUniqueStacks.BeginStack(UniqueStacks::FrameKey("(root)"));

      int numFrames = 0;
      while (e.Has()) {
        if (e.Get().IsNativeLeafAddr()) {
          numFrames++;

          void* pc = e.Get().GetPtr();
          e.Next();

          static const uint32_t BUF_SIZE = 256;
          char buf[BUF_SIZE];

          // Bug 753041: We need a double cast here to tell GCC that we don't
          // want to sign extend 32-bit addresses starting with 0xFXXXXXX.
          unsigned long long pcULL = (unsigned long long)(uintptr_t)pc;
          SprintfLiteral(buf, "0x%llx", pcULL);

          // If the "MOZ_PROFILER_SYMBOLICATE" env-var is set, we add a local
          // symbolication description to the PC address. This is off by
          // default, and mainly intended for local development.
          static const bool preSymbolicate = []() {
            const char* symbolicate = getenv("MOZ_PROFILER_SYMBOLICATE");
            return symbolicate && symbolicate[0] != '\0';
          }();
          if (preSymbolicate) {
            MozCodeAddressDetails details;
            if (MozDescribeCodeAddress(pc, &details)) {
              // Replace \0 terminator with space.
              const uint32_t pcLen = strlen(buf);
              buf[pcLen] = ' ';
              // Add description after space. Note: Using a frame number of 0,
              // as using `numFrames` wouldn't help here, and would prevent
              // combining same function calls that happen at different depths.
              // TODO: Remove unsightly "#00: " if too annoying. :-)
              MozFormatCodeAddressDetails(
                  buf + pcLen + 1, BUF_SIZE - (pcLen + 1), 0, pc, &details);
            }
          }

          stack = aUniqueStacks.AppendFrame(stack, UniqueStacks::FrameKey(buf));

        } else if (e.Get().IsLabel()) {
          numFrames++;

          const char* label = e.Get().GetString();
          e.Next();

          using FrameFlags = ProfilingStackFrame::Flags;
          uint32_t frameFlags = 0;
          if (e.Has() && e.Get().IsFrameFlags()) {
            frameFlags = uint32_t(e.Get().GetUint64());
            e.Next();
          }

          bool relevantForJS =
              frameFlags & uint32_t(FrameFlags::RELEVANT_FOR_JS);

          // Copy potential dynamic string fragments into dynStrBuf, so that
          // dynStrBuf will then contain the entire dynamic string.
          size_t i = 0;
          dynStrBuf[0] = '\0';
          while (e.Has()) {
            if (e.Get().IsDynamicStringFragment()) {
              char chars[ProfileBufferEntry::kNumChars];
              e.Get().CopyCharsInto(chars);
              for (char c : chars) {
                if (i < kMaxFrameKeyLength) {
                  dynStrBuf[i] = c;
                  i++;
                }
              }
              e.Next();
            } else {
              break;
            }
          }
          dynStrBuf[kMaxFrameKeyLength - 1] = '\0';
          bool hasDynamicString = (i != 0);

          std::string frameLabel;
          if (label[0] != '\0' && hasDynamicString) {
            if (frameFlags & uint32_t(FrameFlags::STRING_TEMPLATE_METHOD)) {
              frameLabel += label;
              frameLabel += '.';
              frameLabel += dynStrBuf.get();
            } else if (frameFlags &
                       uint32_t(FrameFlags::STRING_TEMPLATE_GETTER)) {
              frameLabel += "get ";
              frameLabel += label;
              frameLabel += '.';
              frameLabel += dynStrBuf.get();
            } else if (frameFlags &
                       uint32_t(FrameFlags::STRING_TEMPLATE_SETTER)) {
              frameLabel += "set ";
              frameLabel += label;
              frameLabel += '.';
              frameLabel += dynStrBuf.get();
            } else {
              frameLabel += label;
              frameLabel += ' ';
              frameLabel += dynStrBuf.get();
            }
          } else if (hasDynamicString) {
            frameLabel += dynStrBuf.get();
          } else {
            frameLabel += label;
          }

          uint64_t innerWindowID = 0;
          if (e.Has() && e.Get().IsInnerWindowID()) {
            innerWindowID = uint64_t(e.Get().GetUint64());
            e.Next();
          }

          Maybe<unsigned> line;
          if (e.Has() && e.Get().IsLineNumber()) {
            line = Some(unsigned(e.Get().GetInt()));
            e.Next();
          }

          Maybe<unsigned> column;
          if (e.Has() && e.Get().IsColumnNumber()) {
            column = Some(unsigned(e.Get().GetInt()));
            e.Next();
          }

          Maybe<ProfilingCategoryPair> categoryPair;
          if (e.Has() && e.Get().IsCategoryPair()) {
            categoryPair =
                Some(ProfilingCategoryPair(uint32_t(e.Get().GetInt())));
            e.Next();
          }

          stack = aUniqueStacks.AppendFrame(
              stack, UniqueStacks::FrameKey(std::move(frameLabel),
                                            relevantForJS, innerWindowID, line,
                                            column, categoryPair));

        } else {
          break;
        }
      }

      if (numFrames == 0) {
        // It is possible to have empty stacks if native stackwalking is
        // disabled. Skip samples with empty stacks. (See Bug 1497985).
        // Thus, don't use ERROR_AND_CONTINUE, but just continue.
        continue;
      }

      sample.mStack = aUniqueStacks.GetOrAddStackIndex(stack);

      if (e.Has() && e.Get().IsResponsiveness()) {
        sample.mResponsiveness = Some(e.Get().GetDouble());
        e.Next();
      }

      WriteSample(aWriter, sample);

      processedThreadId = threadId;
    }

    return processedThreadId;
  });
}

void ProfileBuffer::StreamMarkersToJSON(SpliceableJSONWriter& aWriter,
                                        BaseProfilerThreadId aThreadId,
                                        const TimeStamp& aProcessStartTime,
                                        double aSinceTime,
                                        UniqueStacks& aUniqueStacks) const {
  mEntries.ReadEach([&](ProfileBufferEntryReader& aER) {
    auto type = static_cast<ProfileBufferEntry::Kind>(
        aER.ReadObject<ProfileBufferEntry::KindUnderlyingType>());
    MOZ_ASSERT(static_cast<ProfileBufferEntry::KindUnderlyingType>(type) <
               static_cast<ProfileBufferEntry::KindUnderlyingType>(
                   ProfileBufferEntry::Kind::MODERN_LIMIT));
    if (type == ProfileBufferEntry::Kind::Marker) {
      ::mozilla::base_profiler_markers_detail::DeserializeAfterKindAndStream(
          aER,
          [&](const BaseProfilerThreadId& aMarkerThreadId) {
            return (aMarkerThreadId == aThreadId) ? &aWriter : nullptr;
          },
          [&](ProfileChunkedBuffer& aChunkedBuffer) {
            ProfilerBacktrace backtrace("", &aChunkedBuffer);
            backtrace.StreamJSON(aWriter, TimeStamp::ProcessCreation(),
                                 aUniqueStacks);
          },
          // We don't have Rust markers in the mozglue.
          [&](mozilla::base_profiler_markers_detail::Streaming::
                  DeserializerTag) {
            MOZ_ASSERT_UNREACHABLE("No Rust markers in mozglue.");
          });
    } else {
      // The entry was not a marker, we need to skip to the end.
      aER.SetRemainingBytes(0);
    }
  });
}

void ProfileBuffer::StreamProfilerOverheadToJSON(
    SpliceableJSONWriter& aWriter, const TimeStamp& aProcessStartTime,
    double aSinceTime) const {
  mEntries.Read([&](ProfileChunkedBuffer::Reader* aReader) {
    MOZ_ASSERT(aReader,
               "ProfileChunkedBuffer cannot be out-of-session when sampler is "
               "running");

    EntryGetter e(*aReader);

    enum Schema : uint32_t {
      TIME = 0,
      LOCKING = 1,
      MARKER_CLEANING = 2,
      COUNTERS = 3,
      THREADS = 4
    };

    aWriter.StartObjectProperty("profilerOverhead");
    aWriter.StartObjectProperty("samples");
    // Stream all sampling overhead data. We skip other entries, because we
    // process them in StreamSamplesToJSON()/etc.
    {
      JSONSchemaWriter schema(aWriter);
      schema.WriteField("time");
      schema.WriteField("locking");
      schema.WriteField("expiredMarkerCleaning");
      schema.WriteField("counters");
      schema.WriteField("threads");
    }

    aWriter.StartArrayProperty("data");
    double firstTime = 0.0;
    double lastTime = 0.0;
    ProfilerStats intervals, overheads, lockings, cleanings, counters, threads;
    while (e.Has()) {
      // valid sequence: ProfilerOverheadTime, ProfilerOverheadDuration * 4
      if (e.Get().IsProfilerOverheadTime()) {
        double time = e.Get().GetDouble();
        if (time >= aSinceTime) {
          e.Next();
          if (!e.Has() || !e.Get().IsProfilerOverheadDuration()) {
            ERROR_AND_CONTINUE(
                "expected a ProfilerOverheadDuration entry after "
                "ProfilerOverheadTime");
          }
          double locking = e.Get().GetDouble();
          e.Next();
          if (!e.Has() || !e.Get().IsProfilerOverheadDuration()) {
            ERROR_AND_CONTINUE(
                "expected a ProfilerOverheadDuration entry after "
                "ProfilerOverheadTime,ProfilerOverheadDuration");
          }
          double cleaning = e.Get().GetDouble();
          e.Next();
          if (!e.Has() || !e.Get().IsProfilerOverheadDuration()) {
            ERROR_AND_CONTINUE(
                "expected a ProfilerOverheadDuration entry after "
                "ProfilerOverheadTime,ProfilerOverheadDuration*2");
          }
          double counter = e.Get().GetDouble();
          e.Next();
          if (!e.Has() || !e.Get().IsProfilerOverheadDuration()) {
            ERROR_AND_CONTINUE(
                "expected a ProfilerOverheadDuration entry after "
                "ProfilerOverheadTime,ProfilerOverheadDuration*3");
          }
          double thread = e.Get().GetDouble();

          if (firstTime == 0.0) {
            firstTime = time;
          } else {
            // Note that we'll have 1 fewer interval than other numbers (because
            // we need both ends of an interval to know its duration). The final
            // difference should be insignificant over the expected many
            // thousands of iterations.
            intervals.Count(time - lastTime);
          }
          lastTime = time;
          overheads.Count(locking + cleaning + counter + thread);
          lockings.Count(locking);
          cleanings.Count(cleaning);
          counters.Count(counter);
          threads.Count(thread);

          AutoArraySchemaWriter writer(aWriter);
          writer.TimeMsElement(TIME, time);
          writer.DoubleElement(LOCKING, locking);
          writer.DoubleElement(MARKER_CLEANING, cleaning);
          writer.DoubleElement(COUNTERS, counter);
          writer.DoubleElement(THREADS, thread);
        }
      }
      e.Next();
    }
    aWriter.EndArray();   // data
    aWriter.EndObject();  // samples

    // Only output statistics if there is at least one full interval (and
    // therefore at least two samplings.)
    if (intervals.n > 0) {
      aWriter.StartObjectProperty("statistics");
      aWriter.DoubleProperty("profiledDuration", lastTime - firstTime);
      aWriter.IntProperty("samplingCount", overheads.n);
      aWriter.DoubleProperty("overheadDurations", overheads.sum);
      aWriter.DoubleProperty("overheadPercentage",
                             overheads.sum / (lastTime - firstTime));
#define PROFILER_STATS(name, var)                           \
  aWriter.DoubleProperty("mean" name, (var).sum / (var).n); \
  aWriter.DoubleProperty("min" name, (var).min);            \
  aWriter.DoubleProperty("max" name, (var).max);
      PROFILER_STATS("Interval", intervals);
      PROFILER_STATS("Overhead", overheads);
      PROFILER_STATS("Lockings", lockings);
      PROFILER_STATS("Cleaning", cleanings);
      PROFILER_STATS("Counter", counters);
      PROFILER_STATS("Thread", threads);
#undef PROFILER_STATS
      aWriter.EndObject();  // statistics
    }
    aWriter.EndObject();  // profilerOverhead
  });
}

struct CounterSample {
  double mTime;
  uint64_t mNumber;
  int64_t mCount;
};

using CounterSamples = Vector<CounterSample>;

// HashMap lookup, if not found, a default value is inserted.
// Returns reference to (existing or new) value inside the HashMap.
template <typename HashM, typename Key>
static auto& LookupOrAdd(HashM& aMap, Key&& aKey) {
  auto addPtr = aMap.lookupForAdd(aKey);
  if (!addPtr) {
    MOZ_RELEASE_ASSERT(aMap.add(addPtr, std::forward<Key>(aKey),
                                typename HashM::Entry::ValueType{}));
    MOZ_ASSERT(!!addPtr);
  }
  return addPtr->value();
}

void ProfileBuffer::StreamCountersToJSON(SpliceableJSONWriter& aWriter,
                                         const TimeStamp& aProcessStartTime,
                                         double aSinceTime) const {
  // Because this is a format entirely internal to the Profiler, any parsing
  // error indicates a bug in the ProfileBuffer writing or the parser itself,
  // or possibly flaky hardware.

  mEntries.Read([&](ProfileChunkedBuffer::Reader* aReader) {
    MOZ_ASSERT(aReader,
               "ProfileChunkedBuffer cannot be out-of-session when sampler is "
               "running");

    EntryGetter e(*aReader);

    enum Schema : uint32_t { TIME = 0, COUNT = 1, NUMBER = 2 };

    // Stream all counters. We skip other entries, because we process them in
    // StreamSamplesToJSON()/etc.
    //
    // Valid sequence in the buffer:
    // CounterID
    // Time
    // ( Count Number? )*
    //
    // And the JSON (example):
    // "counters": {
    //  "name": "malloc",
    //  "category": "Memory",
    //  "description": "Amount of allocated memory",
    //  "samples": {
    //   "schema": {"time": 0, "count": 1, "number": 2},
    //   "data": [
    //    [
    //     16117.033968000002,
    //     2446216,
    //     6801320
    //    ],
    //    [
    //     16118.037638,
    //     2446216,
    //     6801320
    //    ],
    //   ],
    //  },
    // }

    // Build the map of counters and populate it
    HashMap<void*, CounterSamples> counters;

    while (e.Has()) {
      // skip all non-Counters, including if we start in the middle of a counter
      if (e.Get().IsCounterId()) {
        void* id = e.Get().GetPtr();
        CounterSamples& data = LookupOrAdd(counters, id);
        e.Next();
        if (!e.Has() || !e.Get().IsTime()) {
          ERROR_AND_CONTINUE("expected a Time entry");
        }
        double time = e.Get().GetDouble();
        e.Next();
        if (time >= aSinceTime) {
          if (!e.Has() || !e.Get().IsCount()) {
            ERROR_AND_CONTINUE("expected a Count entry");
          }
          int64_t count = e.Get().GetUint64();
          e.Next();
          uint64_t number;
          if (!e.Has() || !e.Get().IsNumber()) {
            number = 0;
          } else {
            number = e.Get().GetInt64();
            e.Next();
          }
          CounterSample sample = {time, number, count};
          MOZ_RELEASE_ASSERT(data.append(sample));
        } else {
          // skip counter sample - only need to skip the initial counter
          // id, then let the loop at the top skip the rest
        }
      } else {
        e.Next();
      }
    }
    // we have a map of counter entries; dump them to JSON
    if (counters.count() == 0) {
      return;
    }

    aWriter.StartArrayProperty("counters");
    for (auto iter = counters.iter(); !iter.done(); iter.next()) {
      CounterSamples& samples = iter.get().value();
      size_t size = samples.length();
      if (size == 0) {
        continue;
      }
      const BaseProfilerCount* base_counter =
          static_cast<const BaseProfilerCount*>(iter.get().key());

      aWriter.Start();
      aWriter.StringProperty("name", MakeStringSpan(base_counter->mLabel));
      aWriter.StringProperty("category",
                             MakeStringSpan(base_counter->mCategory));
      aWriter.StringProperty("description",
                             MakeStringSpan(base_counter->mDescription));

      bool hasNumber = false;
      for (size_t i = 0; i < size; i++) {
        if (samples[i].mNumber != 0) {
          hasNumber = true;
          break;
        }
      }

      aWriter.StartObjectProperty("samples");
      {
        JSONSchemaWriter schema(aWriter);
        schema.WriteField("time");
        schema.WriteField("count");
        if (hasNumber) {
          schema.WriteField("number");
        }
      }

      aWriter.StartArrayProperty("data");
      uint64_t previousNumber = 0;
      int64_t previousCount = 0;
      for (size_t i = 0; i < size; i++) {
        // Encode as deltas, and only encode if different than the last
        // sample
        if (i == 0 || samples[i].mNumber != previousNumber ||
            samples[i].mCount != previousCount) {
          MOZ_ASSERT(i == 0 || samples[i].mTime >= samples[i - 1].mTime);
          MOZ_ASSERT(samples[i].mNumber >= previousNumber);
          MOZ_ASSERT(samples[i].mNumber - previousNumber <=
                     uint64_t(std::numeric_limits<int64_t>::max()));

          AutoArraySchemaWriter writer(aWriter);
          writer.TimeMsElement(TIME, samples[i].mTime);
          writer.IntElement(COUNT, samples[i].mCount - previousCount);
          if (hasNumber) {
            writer.IntElement(NUMBER, static_cast<int64_t>(samples[i].mNumber -
                                                           previousNumber));
          }
          previousNumber = samples[i].mNumber;
          previousCount = samples[i].mCount;
        }
      }
      aWriter.EndArray();   // data
      aWriter.EndObject();  // samples
      aWriter.End();        // for each counter
    }
    aWriter.EndArray();  // counters
  });
}

#undef ERROR_AND_CONTINUE

static void AddPausedRange(SpliceableJSONWriter& aWriter, const char* aReason,
                           const Maybe<double>& aStartTime,
                           const Maybe<double>& aEndTime) {
  aWriter.Start();
  if (aStartTime) {
    aWriter.TimeDoubleMsProperty("startTime", *aStartTime);
  } else {
    aWriter.NullProperty("startTime");
  }
  if (aEndTime) {
    aWriter.TimeDoubleMsProperty("endTime", *aEndTime);
  } else {
    aWriter.NullProperty("endTime");
  }
  aWriter.StringProperty("reason", MakeStringSpan(aReason));
  aWriter.End();
}

void ProfileBuffer::StreamPausedRangesToJSON(SpliceableJSONWriter& aWriter,
                                             double aSinceTime) const {
  mEntries.Read([&](ProfileChunkedBuffer::Reader* aReader) {
    MOZ_ASSERT(aReader,
               "ProfileChunkedBuffer cannot be out-of-session when sampler is "
               "running");

    EntryGetter e(*aReader);

    Maybe<double> currentPauseStartTime;
    Maybe<double> currentCollectionStartTime;

    while (e.Has()) {
      if (e.Get().IsPause()) {
        currentPauseStartTime = Some(e.Get().GetDouble());
      } else if (e.Get().IsResume()) {
        AddPausedRange(aWriter, "profiler-paused", currentPauseStartTime,
                       Some(e.Get().GetDouble()));
        currentPauseStartTime = Nothing();
      } else if (e.Get().IsCollectionStart()) {
        currentCollectionStartTime = Some(e.Get().GetDouble());
      } else if (e.Get().IsCollectionEnd()) {
        AddPausedRange(aWriter, "collecting", currentCollectionStartTime,
                       Some(e.Get().GetDouble()));
        currentCollectionStartTime = Nothing();
      }
      e.Next();
    }

    if (currentPauseStartTime) {
      AddPausedRange(aWriter, "profiler-paused", currentPauseStartTime,
                     Nothing());
    }
    if (currentCollectionStartTime) {
      AddPausedRange(aWriter, "collecting", currentCollectionStartTime,
                     Nothing());
    }
  });
}

bool ProfileBuffer::DuplicateLastSample(BaseProfilerThreadId aThreadId,
                                        const TimeStamp& aProcessStartTime,
                                        Maybe<uint64_t>& aLastSample) {
  if (!aLastSample) {
    return false;
  }

  ProfileChunkedBuffer tempBuffer(
      ProfileChunkedBuffer::ThreadSafety::WithoutMutex, WorkerChunkManager());

  auto retrieveWorkerChunk = MakeScopeExit(
      [&]() { WorkerChunkManager().Reset(tempBuffer.GetAllChunks()); });

  const bool ok = mEntries.Read([&](ProfileChunkedBuffer::Reader* aReader) {
    MOZ_ASSERT(aReader,
               "ProfileChunkedBuffer cannot be out-of-session when sampler is "
               "running");

    EntryGetter e(*aReader, *aLastSample);

    if (e.CurPos() != *aLastSample) {
      // The last sample is no longer within the buffer range, so we cannot
      // use it. Reset the stored buffer position to Nothing().
      aLastSample.reset();
      return false;
    }

    MOZ_RELEASE_ASSERT(e.Has() && e.Get().IsThreadId() &&
                       e.Get().GetThreadId() == aThreadId);

    e.Next();

    // Go through the whole entry and duplicate it, until we find the next
    // one.
    while (e.Has()) {
      switch (e.Get().GetKind()) {
        case ProfileBufferEntry::Kind::Pause:
        case ProfileBufferEntry::Kind::Resume:
        case ProfileBufferEntry::Kind::PauseSampling:
        case ProfileBufferEntry::Kind::ResumeSampling:
        case ProfileBufferEntry::Kind::CollectionStart:
        case ProfileBufferEntry::Kind::CollectionEnd:
        case ProfileBufferEntry::Kind::ThreadId:
          // We're done.
          return true;
        case ProfileBufferEntry::Kind::Time:
          // Copy with new time
          AddEntry(
              tempBuffer,
              ProfileBufferEntry::Time(
                  (TimeStamp::Now() - aProcessStartTime).ToMilliseconds()));
          break;
        case ProfileBufferEntry::Kind::Number:
        case ProfileBufferEntry::Kind::Count:
        case ProfileBufferEntry::Kind::Responsiveness:
          // Don't copy anything not part of a thread's stack sample
          break;
        case ProfileBufferEntry::Kind::CounterId:
          // CounterId is normally followed by Time - if so, we'd like
          // to skip it.  If we duplicate Time, it won't hurt anything, just
          // waste buffer space (and this can happen if the CounterId has
          // fallen off the end of the buffer, but Time (and Number/Count)
          // are still in the buffer).
          e.Next();
          if (e.Has() && e.Get().GetKind() != ProfileBufferEntry::Kind::Time) {
            // this would only happen if there was an invalid sequence
            // in the buffer.  Don't skip it.
            continue;
          }
          // we've skipped Time
          break;
        case ProfileBufferEntry::Kind::ProfilerOverheadTime:
          // ProfilerOverheadTime is normally followed by
          // ProfilerOverheadDuration*4 - if so, we'd like to skip it. Don't
          // duplicate, as we are in the middle of a sampling and will soon
          // capture its own overhead.
          e.Next();
          // A missing Time would only happen if there was an invalid
          // sequence in the buffer. Don't skip unexpected entry.
          if (e.Has() &&
              e.Get().GetKind() !=
                  ProfileBufferEntry::Kind::ProfilerOverheadDuration) {
            continue;
          }
          e.Next();
          if (e.Has() &&
              e.Get().GetKind() !=
                  ProfileBufferEntry::Kind::ProfilerOverheadDuration) {
            continue;
          }
          e.Next();
          if (e.Has() &&
              e.Get().GetKind() !=
                  ProfileBufferEntry::Kind::ProfilerOverheadDuration) {
            continue;
          }
          e.Next();
          if (e.Has() &&
              e.Get().GetKind() !=
                  ProfileBufferEntry::Kind::ProfilerOverheadDuration) {
            continue;
          }
          // we've skipped ProfilerOverheadTime and
          // ProfilerOverheadDuration*4.
          break;
        default: {
          // Copy anything else we don't know about.
          AddEntry(tempBuffer, e.Get());
          break;
        }
      }
      e.Next();
    }
    return true;
  });

  if (!ok) {
    return false;
  }

  // If the buffer was big enough, there won't be any cleared blocks.
  if (tempBuffer.GetState().mClearedBlockCount != 0) {
    // No need to try to read stack again as it won't fit. Reset the stored
    // buffer position to Nothing().
    aLastSample.reset();
    return false;
  }

  aLastSample = Some(AddThreadIdEntry(aThreadId));

  tempBuffer.Read([&](ProfileChunkedBuffer::Reader* aReader) {
    MOZ_ASSERT(aReader, "tempBuffer cannot be out-of-session");

    EntryGetter e(*aReader);

    while (e.Has()) {
      AddEntry(e.Get());
      e.Next();
    }
  });

  return true;
}

void ProfileBuffer::DiscardSamplesBeforeTime(double aTime) {
  // This function does nothing!
  // The duration limit will be removed from Firefox, see bug 1632365.
  Unused << aTime;
}

// END ProfileBuffer
////////////////////////////////////////////////////////////////////////

}  // namespace baseprofiler
}  // namespace mozilla

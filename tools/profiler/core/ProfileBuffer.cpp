/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ProfileBuffer.h"

#include "mozilla/MathAlgorithms.h"

#include "ProfilerMarker.h"
#include "jsfriendapi.h"
#include "nsScriptSecurityManager.h"
#include "nsJSPrincipals.h"

using namespace mozilla;

ProfileBuffer::ProfileBuffer(uint32_t aEntrySize)
  : mEntryIndexMask(0)
  , mRangeStart(0)
  , mRangeEnd(0)
  , mEntrySize(0)
{
  // Round aEntrySize up to the nearest power of two, so that we can index
  // mEntries with a simple mask and don't need to do a slow modulo operation.
  const uint32_t UINT32_MAX_POWER_OF_TWO = 1 << 31;
  MOZ_RELEASE_ASSERT(aEntrySize <= UINT32_MAX_POWER_OF_TWO,
                     "aEntrySize is larger than what we support");
  mEntrySize = RoundUpPow2(aEntrySize);
  mEntryIndexMask = mEntrySize - 1;
  mEntries = MakeUnique<ProfileBufferEntry[]>(mEntrySize);
}

ProfileBuffer::~ProfileBuffer()
{
  while (mStoredMarkers.peek()) {
    delete mStoredMarkers.popHead();
  }
}

// Called from signal, call only reentrant functions
void
ProfileBuffer::AddEntry(const ProfileBufferEntry& aEntry)
{
  GetEntry(mRangeEnd++) = aEntry;

  // The distance between mRangeStart and mRangeEnd must never exceed
  // mEntrySize, so advance mRangeStart if necessary.
  if (mRangeEnd - mRangeStart > mEntrySize) {
    mRangeStart++;
  }
}

uint64_t
ProfileBuffer::AddThreadIdEntry(int aThreadId)
{
  uint64_t pos = mRangeEnd;
  AddEntry(ProfileBufferEntry::ThreadId(aThreadId));
  return pos;
}

void
ProfileBuffer::AddStoredMarker(ProfilerMarker *aStoredMarker)
{
  aStoredMarker->SetPositionInBuffer(mRangeEnd);
  mStoredMarkers.insert(aStoredMarker);
}

void
ProfileBuffer::CollectCodeLocation(
  const char* aLabel, const char* aStr, int aLineNumber,
  const Maybe<js::ProfilingStackFrame::Category>& aCategory)
{
  AddEntry(ProfileBufferEntry::Label(aLabel));

  if (aStr) {
    // Store the string using one or more DynamicStringFragment entries.
    size_t strLen = strlen(aStr) + 1;   // +1 for the null terminator
    for (size_t j = 0; j < strLen; ) {
      // Store up to kNumChars characters in the entry.
      char chars[ProfileBufferEntry::kNumChars];
      size_t len = ProfileBufferEntry::kNumChars;
      if (j + len >= strLen) {
        len = strLen - j;
      }
      memcpy(chars, &aStr[j], len);
      j += ProfileBufferEntry::kNumChars;

      AddEntry(ProfileBufferEntry::DynamicStringFragment(chars));
    }
  }

  if (aLineNumber != -1) {
    AddEntry(ProfileBufferEntry::LineNumber(aLineNumber));
  }

  if (aCategory.isSome()) {
    AddEntry(ProfileBufferEntry::Category(int(*aCategory)));
  }
}

void
ProfileBuffer::DeleteExpiredStoredMarkers()
{
  // Delete markers of samples that have been overwritten due to circular
  // buffer wraparound.
  while (mStoredMarkers.peek() &&
         mStoredMarkers.peek()->HasExpired(mRangeStart)) {
    delete mStoredMarkers.popHead();
  }
}

size_t
ProfileBuffer::SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const
{
  size_t n = aMallocSizeOf(this);
  n += aMallocSizeOf(mEntries.get());

  // Measurement of the following members may be added later if DMD finds it
  // is worthwhile:
  // - memory pointed to by the elements within mEntries
  // - mStoredMarkers

  return n;
}

/* ProfileBufferCollector */

static bool
IsChromeJSScript(JSScript* aScript)
{
  // WARNING: this function runs within the profiler's "critical section".
  auto compartment = js::GetScriptCompartment(aScript);
  return js::IsSystemCompartment(compartment);
}

void
ProfileBufferCollector::CollectNativeLeafAddr(void* aAddr)
{
  mBuf.AddEntry(ProfileBufferEntry::NativeLeafAddr(aAddr));
}

void
ProfileBufferCollector::CollectJitReturnAddr(void* aAddr)
{
  mBuf.AddEntry(ProfileBufferEntry::JitReturnAddr(aAddr));
}

void
ProfileBufferCollector::CollectWasmFrame(const char* aLabel)
{
  mBuf.CollectCodeLocation("", aLabel, -1, Nothing());
}

void
ProfileBufferCollector::CollectProfilingStackFrame(const js::ProfilingStackFrame& aFrame)
{
  // WARNING: this function runs within the profiler's "critical section".

  MOZ_ASSERT(aFrame.kind() == js::ProfilingStackFrame::Kind::LABEL ||
             aFrame.kind() == js::ProfilingStackFrame::Kind::JS_NORMAL);

  const char* label = aFrame.label();
  const char* dynamicString = aFrame.dynamicString();
  bool isChromeJSEntry = false;
  int lineno = -1;

  if (aFrame.isJsFrame()) {
    // There are two kinds of JS frames that get pushed onto the ProfilingStack.
    //
    // - label = "", dynamic string = <something>
    // - label = "js::RunScript", dynamic string = nullptr
    //
    // The line number is only interesting in the first case.

    if (label[0] == '\0') {
      MOZ_ASSERT(dynamicString);

      // We call aFrame.script() repeatedly -- rather than storing the result in
      // a local variable in order -- to avoid rooting hazards.
      if (aFrame.script()) {
        isChromeJSEntry = IsChromeJSScript(aFrame.script());
        if (aFrame.pc()) {
          lineno = JS_PCToLineNumber(aFrame.script(), aFrame.pc());
        }
      }

    } else {
      MOZ_ASSERT(strcmp(label, "js::RunScript") == 0 && !dynamicString);
    }
  } else {
    MOZ_ASSERT(aFrame.isLabelFrame());
    lineno = aFrame.line();
  }

  if (dynamicString) {
    // Adjust the dynamic string as necessary.
    if (ProfilerFeature::HasPrivacy(mFeatures) && !isChromeJSEntry) {
      dynamicString = "(private)";
    } else if (strlen(dynamicString) >= ProfileBuffer::kMaxFrameKeyLength) {
      dynamicString = "(too long)";
    }
  }

  mBuf.CollectCodeLocation(label, dynamicString, lineno,
                           Some(aFrame.category()));
}

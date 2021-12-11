/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TelemetryIOInterposeObserver.h"
#include "core/TelemetryCommon.h"
#include "js/Array.h"               // JS::NewArrayObject
#include "js/PropertyAndElement.h"  // JS_DefineUCProperty
#include "js/ValueArray.h"
#include "nsIFile.h"

namespace mozilla::Telemetry {

TelemetryIOInterposeObserver::TelemetryIOInterposeObserver(nsIFile* aXreDir)
    : mCurStage(STAGE_STARTUP) {
  nsAutoString xreDirPath;
  nsresult rv = aXreDir->GetPath(xreDirPath);
  if (NS_SUCCEEDED(rv)) {
    AddPath(xreDirPath, u"{xre}"_ns);
  }
}

void TelemetryIOInterposeObserver::AddPath(const nsAString& aPath,
                                           const nsAString& aSubstName) {
  mSafeDirs.AppendElement(SafeDir(aPath, aSubstName));
}

// Threshold for reporting slow main-thread I/O (50 milliseconds).
const TimeDuration kTelemetryReportThreshold =
    TimeDuration::FromMilliseconds(50);

void TelemetryIOInterposeObserver::Observe(Observation& aOb) {
  // We only report main-thread I/O
  if (!IsMainThread()) {
    return;
  }

  if (aOb.ObservedOperation() == OpNextStage) {
    mCurStage = NextStage(mCurStage);
    MOZ_ASSERT(mCurStage < NUM_STAGES);
    return;
  }

  if (aOb.Duration() < kTelemetryReportThreshold) {
    return;
  }

  // Get the filename
  nsAutoString filename;
  aOb.Filename(filename);

  // Discard observations without filename
  if (filename.IsEmpty()) {
    return;
  }

#if defined(XP_WIN)
  auto comparator = nsCaseInsensitiveStringComparator;
#else
  auto comparator = nsTDefaultStringComparator<char16_t>;
#endif
  nsAutoString processedName;
  uint32_t safeDirsLen = mSafeDirs.Length();
  for (uint32_t i = 0; i < safeDirsLen; ++i) {
    if (StringBeginsWith(filename, mSafeDirs[i].mPath, comparator)) {
      processedName = mSafeDirs[i].mSubstName;
      processedName += Substring(filename, mSafeDirs[i].mPath.Length());
      break;
    }
  }

  if (processedName.IsEmpty()) {
    return;
  }

  // Create a new entry or retrieve the existing one
  FileIOEntryType* entry = mFileStats.PutEntry(processedName);
  if (entry) {
    FileStats& stats = entry->GetModifiableData()->mStats[mCurStage];
    // Update the statistics
    stats.totalTime += (double)aOb.Duration().ToMilliseconds();
    switch (aOb.ObservedOperation()) {
      case OpCreateOrOpen:
        stats.creates++;
        break;
      case OpRead:
        stats.reads++;
        break;
      case OpWrite:
        stats.writes++;
        break;
      case OpFSync:
        stats.fsyncs++;
        break;
      case OpStat:
        stats.stats++;
        break;
      default:
        break;
    }
  }
}

bool TelemetryIOInterposeObserver::ReflectFileStats(FileIOEntryType* entry,
                                                    JSContext* cx,
                                                    JS::Handle<JSObject*> obj) {
  JS::RootedValueArray<NUM_STAGES> stages(cx);

  FileStatsByStage& statsByStage = *entry->GetModifiableData();
  for (int s = STAGE_STARTUP; s < NUM_STAGES; ++s) {
    FileStats& fileStats = statsByStage.mStats[s];

    if (fileStats.totalTime == 0 && fileStats.creates == 0 &&
        fileStats.reads == 0 && fileStats.writes == 0 &&
        fileStats.fsyncs == 0 && fileStats.stats == 0) {
      // Don't add an array that contains no information
      stages[s].setNull();
      continue;
    }

    // Array we want to report
    JS::RootedValueArray<6> stats(cx);
    stats[0].setNumber(fileStats.totalTime);
    stats[1].setNumber(fileStats.creates);
    stats[2].setNumber(fileStats.reads);
    stats[3].setNumber(fileStats.writes);
    stats[4].setNumber(fileStats.fsyncs);
    stats[5].setNumber(fileStats.stats);

    // Create jsStats as array of elements above
    JS::RootedObject jsStats(cx, JS::NewArrayObject(cx, stats));
    if (!jsStats) {
      continue;
    }

    stages[s].setObject(*jsStats);
  }

  JS::Rooted<JSObject*> jsEntry(cx, JS::NewArrayObject(cx, stages));
  if (!jsEntry) {
    return false;
  }

  // Add jsEntry to top-level dictionary
  const nsAString& key = entry->GetKey();
  return JS_DefineUCProperty(cx, obj, key.Data(), key.Length(), jsEntry,
                             JSPROP_ENUMERATE | JSPROP_READONLY);
}

bool TelemetryIOInterposeObserver::ReflectIntoJS(
    JSContext* cx, JS::Handle<JSObject*> rootObj) {
  return mFileStats.ReflectIntoJS(ReflectFileStats, cx, rootObj);
}

/**
 * Get size of hash table with file stats
 */

size_t TelemetryIOInterposeObserver::SizeOfIncludingThis(
    mozilla::MallocSizeOf aMallocSizeOf) const {
  return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
}

size_t TelemetryIOInterposeObserver::SizeOfExcludingThis(
    mozilla::MallocSizeOf aMallocSizeOf) const {
  size_t size = 0;
  size += mFileStats.ShallowSizeOfExcludingThis(aMallocSizeOf);
  for (auto iter = mFileStats.ConstIter(); !iter.Done(); iter.Next()) {
    size += iter.Get()->GetKey().SizeOfExcludingThisIfUnshared(aMallocSizeOf);
  }
  size += mSafeDirs.ShallowSizeOfExcludingThis(aMallocSizeOf);
  uint32_t safeDirsLen = mSafeDirs.Length();
  for (uint32_t i = 0; i < safeDirsLen; ++i) {
    size += mSafeDirs[i].SizeOfExcludingThis(aMallocSizeOf);
  }
  return size;
}

}  // namespace mozilla::Telemetry

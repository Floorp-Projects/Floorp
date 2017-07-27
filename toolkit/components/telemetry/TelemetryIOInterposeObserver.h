/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
 * IOInterposeObserver recording statistics of main-thread I/O during execution,
 * aimed at consumption by TelemetryImpl
 */

#ifndef TelemetryIOInterposeObserver_h__
#define TelemetryIOInterposeObserver_h__

#include "jsapi.h"
#include "nsTArray.h"
#include "nsTHashtable.h"
#include "nsHashKeys.h"
#include "nsBaseHashtable.h"
#include "nsClassHashtable.h"

#include "TelemetryCommon.h"
#include "mozilla/IOInterposer.h"

namespace mozilla {
namespace Telemetry {

class TelemetryIOInterposeObserver : public IOInterposeObserver
{
  /** File-level statistics structure */
  struct FileStats {
    FileStats()
      : creates(0)
      , reads(0)
      , writes(0)
      , fsyncs(0)
      , stats(0)
      , totalTime(0)
    {}
    uint32_t  creates;      /** Number of create/open operations */
    uint32_t  reads;        /** Number of read operations */
    uint32_t  writes;       /** Number of write operations */
    uint32_t  fsyncs;       /** Number of fsync operations */
    uint32_t  stats;        /** Number of stat operations */
    double    totalTime;    /** Accumulated duration of all operations */
  };

  struct SafeDir {
    SafeDir(const nsAString& aPath, const nsAString& aSubstName)
      : mPath(aPath)
      , mSubstName(aSubstName)
    {}
    size_t SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const {
      return mPath.SizeOfExcludingThisIfUnshared(aMallocSizeOf) +
             mSubstName.SizeOfExcludingThisIfUnshared(aMallocSizeOf);
    }
    nsString  mPath;        /** Path to the directory */
    nsString  mSubstName;   /** Name to substitute with */
  };

public:
  explicit TelemetryIOInterposeObserver(nsIFile* aXreDir);

  /**
   * An implementation of Observe that records statistics of all
   * file IO operations.
   */
  void Observe(Observation& aOb) override;

  /**
   * Reflect recorded file IO statistics into Javascript
   */
  bool ReflectIntoJS(JSContext *cx, JS::Handle<JSObject*> rootObj);

  /**
   * Adds a path for inclusion in main thread I/O report.
   * @param aPath Directory path
   * @param aSubstName Name to substitute for aPath for privacy reasons
   */
  void AddPath(const nsAString& aPath, const nsAString& aSubstName);

  /**
   * Get size of hash table with file stats
   */
  size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;

  size_t SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;

private:
  enum Stage
  {
    STAGE_STARTUP = 0,
    STAGE_NORMAL,
    STAGE_SHUTDOWN,
    NUM_STAGES
  };
  static inline Stage NextStage(Stage aStage)
  {
    switch (aStage) {
      case STAGE_STARTUP:
        return STAGE_NORMAL;
      case STAGE_NORMAL:
        return STAGE_SHUTDOWN;
      case STAGE_SHUTDOWN:
        return STAGE_SHUTDOWN;
      default:
        return NUM_STAGES;
    }
  }

  struct FileStatsByStage
  {
    FileStats mStats[NUM_STAGES];
  };
  typedef nsBaseHashtableET<nsStringHashKey, FileStatsByStage> FileIOEntryType;

  // Statistics for each filename
  Common::AutoHashtable<FileIOEntryType> mFileStats;
  // Container for whitelisted directories
  nsTArray<SafeDir> mSafeDirs;
  Stage             mCurStage;

  /**
   * Reflect a FileIOEntryType object to a Javascript property on obj with
   * filename as key containing array:
   * [totalTime, creates, reads, writes, fsyncs, stats]
   */
  static bool ReflectFileStats(FileIOEntryType* entry, JSContext *cx,
                               JS::Handle<JSObject*> obj);
};

} // namespace Telemetry
} // namespace mozilla

#endif // TelemetryIOInterposeObserver_h__

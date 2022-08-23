/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ProfiledThreadData.h"

#include "BaseProfiler.h"
#include "ProfileBuffer.h"

#include "mozilla/BaseProfileJSONWriter.h"

#if defined(GP_OS_darwin)
#  include <pthread.h>
#endif

namespace mozilla {
namespace baseprofiler {

ProfiledThreadData::ProfiledThreadData(ThreadInfo* aThreadInfo)
    : mThreadInfo(aThreadInfo) {}

ProfiledThreadData::~ProfiledThreadData() {}

void ProfiledThreadData::StreamJSON(const ProfileBuffer& aBuffer,
                                    SpliceableJSONWriter& aWriter,
                                    const std::string& aProcessName,
                                    const std::string& aETLDplus1,
                                    const TimeStamp& aProcessStartTime,
                                    double aSinceTime) {
  UniqueStacks uniqueStacks;

  aWriter.SetUniqueStrings(uniqueStacks.UniqueStrings());

  aWriter.Start();
  {
    StreamSamplesAndMarkers(mThreadInfo->Name(), mThreadInfo->ThreadId(),
                            aBuffer, aWriter, aProcessName, aETLDplus1,
                            aProcessStartTime, mThreadInfo->RegisterTime(),
                            mUnregisterTime, aSinceTime, uniqueStacks);

    aWriter.StartObjectProperty("stackTable");
    {
      {
        JSONSchemaWriter schema(aWriter);
        schema.WriteField("prefix");
        schema.WriteField("frame");
      }

      aWriter.StartArrayProperty("data");
      { uniqueStacks.SpliceStackTableElements(aWriter); }
      aWriter.EndArray();
    }
    aWriter.EndObject();

    aWriter.StartObjectProperty("frameTable");
    {
      {
        JSONSchemaWriter schema(aWriter);
        schema.WriteField("location");
        schema.WriteField("relevantForJS");
        schema.WriteField("innerWindowID");
        schema.WriteField("implementation");
        schema.WriteField("optimizations");
        schema.WriteField("line");
        schema.WriteField("column");
        schema.WriteField("category");
        schema.WriteField("subcategory");
      }

      aWriter.StartArrayProperty("data");
      { uniqueStacks.SpliceFrameTableElements(aWriter); }
      aWriter.EndArray();
    }
    aWriter.EndObject();

    aWriter.StartArrayProperty("stringTable");
    {
      std::move(uniqueStacks.UniqueStrings())
          .SpliceStringTableElements(aWriter);
    }
    aWriter.EndArray();
  }
  aWriter.End();

  aWriter.ResetUniqueStrings();
}

BaseProfilerThreadId StreamSamplesAndMarkers(
    const char* aName, BaseProfilerThreadId aThreadId,
    const ProfileBuffer& aBuffer, SpliceableJSONWriter& aWriter,
    const std::string& aProcessName, const std::string& aETLDplus1,
    const TimeStamp& aProcessStartTime, const TimeStamp& aRegisterTime,
    const TimeStamp& aUnregisterTime, double aSinceTime,
    UniqueStacks& aUniqueStacks) {
  BaseProfilerThreadId processedThreadId;

  aWriter.StringProperty(
      "processType",
      "(unknown)" /* XRE_GeckoProcessTypeToString(XRE_GetProcessType()) */);

  {
    std::string name = aName;
    // We currently need to distinguish threads output by Base Profiler from
    // those in Gecko Profiler, as the frontend could get confused and lose
    // tracks with the same name.
    // TODO: As part of the profilers de-duplication, thread data from both
    // profilers should end up in the same track, at which point this won't be
    // necessary anymore. See meta bug 1557566.
    name += " (pre-xul)";
    aWriter.StringProperty("name", name);
  }

  // Use given process name (if any).
  if (!aProcessName.empty()) {
    aWriter.StringProperty("processName", aProcessName);
  }
  if (!aETLDplus1.empty()) {
    aWriter.StringProperty("eTLD+1", aETLDplus1);
  }

  if (aRegisterTime) {
    aWriter.DoubleProperty(
        "registerTime", (aRegisterTime - aProcessStartTime).ToMilliseconds());
  } else {
    aWriter.NullProperty("registerTime");
  }

  if (aUnregisterTime) {
    aWriter.DoubleProperty(
        "unregisterTime",
        (aUnregisterTime - aProcessStartTime).ToMilliseconds());
  } else {
    aWriter.NullProperty("unregisterTime");
  }

  aWriter.StartObjectProperty("samples");
  {
    {
      JSONSchemaWriter schema(aWriter);
      schema.WriteField("stack");
      schema.WriteField("time");
      schema.WriteField("eventDelay");
    }

    aWriter.StartArrayProperty("data");
    {
      processedThreadId = aBuffer.StreamSamplesToJSON(
          aWriter, aThreadId, aSinceTime, aUniqueStacks);
    }
    aWriter.EndArray();
  }
  aWriter.EndObject();

  aWriter.StartObjectProperty("markers");
  {
    {
      JSONSchemaWriter schema(aWriter);
      schema.WriteField("name");
      schema.WriteField("startTime");
      schema.WriteField("endTime");
      schema.WriteField("phase");
      schema.WriteField("category");
      schema.WriteField("data");
    }

    aWriter.StartArrayProperty("data");
    {
      aBuffer.StreamMarkersToJSON(aWriter, aThreadId, aProcessStartTime,
                                  aSinceTime, aUniqueStacks);
    }
    aWriter.EndArray();
  }
  aWriter.EndObject();

  // Tech note: If `ToNumber()` returns a uint64_t, the conversion to int64_t is
  // "implementation-defined" before C++20. This is acceptable here, because
  // this is a one-way conversion to a unique identifier that's used to visually
  // separate data by thread on the front-end.
  aWriter.IntProperty(
      "pid", static_cast<int64_t>(profiler_current_process_id().ToNumber()));
  aWriter.IntProperty("tid",
                      static_cast<int64_t>(aThreadId.IsSpecified()
                                               ? aThreadId.ToNumber()
                                               : processedThreadId.ToNumber()));

  return processedThreadId;
}

}  // namespace baseprofiler
}  // namespace mozilla

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TelemetryGeckoViewPersistence.h"

#include "jsapi.h"
#include "mozilla/JSONWriter.h"
#include "mozilla/Path.h"
#include "mozilla/Preferences.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/SystemGroup.h"
#include "mozilla/dom/ScriptSettings.h" // for AutoJSAPI
#include "mozilla/dom/SimpleGlobalObject.h"
#include "nsDirectoryServiceDefs.h"
#include "nsIFile.h"
#include "nsIInputStream.h"
#include "nsIObserverService.h"
#include "nsIOutputStream.h"
#include "nsISafeOutputStream.h"
#include "nsITimer.h"
#include "nsLocalFile.h"
#include "nsNetUtil.h"
#include "nsXULAppAPI.h"
#include "prenv.h"
#include "prio.h"
#include "TelemetryScalar.h"
#include "xpcpublic.h"

using mozilla::MakeScopeExit;
using mozilla::Preferences;
using mozilla::StaticRefPtr;
using mozilla::SystemGroup;
using mozilla::TaskCategory;
using mozilla::dom::AutoJSAPI;
using mozilla::dom::SimpleGlobalObject;

using PathChar = mozilla::filesystem::Path::value_type;
using PathCharPtr = const PathChar*;

// Enable logging by default on Debug builds.
#ifdef DEBUG
// If we're building for Android, use the provided logging facility.
#ifdef MOZ_WIDGET_ANDROID
#include <android/log.h>
#define ANDROID_LOG(fmt, ...) \
  __android_log_print(ANDROID_LOG_DEBUG, "Telemetry", fmt, ##__VA_ARGS__)
#else
// If we're building for other platforms (e.g. for running test coverage), try
// to print something anyway.
#define ANDROID_LOG(...) printf_stderr("\n**** TELEMETRY: " __VA_ARGS__)
#endif // MOZ_WIDGET_ANDROID
#else
// No-op on Release builds.
#define ANDROID_LOG(...)
#endif // DEBUG

// The Gecko runtime can be killed at anytime. Moreover, we can
// have very short lived sessions. The persistence timeout governs
// how frequently measurements are saved to disk.
const uint32_t kDefaultPersistenceTimeoutMs = 60 * 1000; // 60s

// The name of the persistence file used for saving the
// measurements.
const char16_t kPersistenceFileName[] = u"gv_measurements.json";

// This topic is notified and propagated up to the application to
// make sure it knows that data loading has complete and that snapshotting
// can now be performed.
const char kLoadCompleteTopic[] = "internal-telemetry-geckoview-load-complete";

// The timer used for persisting measurements data.
nsITimer* gPersistenceTimer;
// The worker thread to perform persistence.
StaticRefPtr<nsIThread> gPersistenceThread;

namespace {

void PersistenceThreadPersist();

/**
+ * The helper class used by mozilla::JSONWriter to
+ * serialize the JSON structure to a file.
+ */
class StreamingJSONWriter : public mozilla::JSONWriteFunc
{
public:
  nsresult Open(nsCOMPtr<nsIFile> aOutFile)
  {
    MOZ_ASSERT(!mStream, "Open must not be called twice");
    nsresult rv = NS_NewSafeLocalFileOutputStream(getter_AddRefs(mStream), aOutFile);
    NS_ENSURE_SUCCESS(rv, rv);
    return NS_OK;
  }

  nsresult Close()
  {
    MOZ_ASSERT(mStream, "Close must be called on an already opened stream");
    // We don't need to care too much about checking if count matches
    // the length of aData: Finish() will do that for us and fail if
    // Write did not persist all the data or mStream->Close() failed.
    // Note that |nsISafeOutputStream| will write to a temp file and only
    // overwrite the destination if no error was reported.
    nsCOMPtr<nsISafeOutputStream> safeStream = do_QueryInterface(mStream);
    MOZ_ASSERT(safeStream);
    return safeStream->Finish();
  }

  void Write(const char* aStr) override
  {
    uint32_t count;
    mozilla::Unused << mStream->Write(aStr, strlen(aStr), &count);
  }

private:
  nsCOMPtr<nsIOutputStream> mStream;
};

/**
 * Get the path to the Android Data dir.
 *
 * @param {nsTString<PathChar>} aOutDir - the variable holding the path.
 * @return {nsresult} NS_OK if the data dir path was found, a failure value otherwise.
 */
nsresult
GetAndroidDataDir(nsTString<PathChar>& aOutDir)
{
  // This relies on the Java environment to set the location of the
  // cache directory. If that happens, the following variable is set.
  // This should always be the case.
  const char *dataDir = PR_GetEnv("MOZ_ANDROID_DATA_DIR");
  if (!dataDir || !*dataDir) {
    ANDROID_LOG("GetAndroidDataDir - Cannot find the data directory in the environment.");
    return NS_ERROR_FAILURE;
  }

  aOutDir.AssignASCII(dataDir);
  return NS_OK;
}

/**
 * Get the path to the persistence file in the Android Data dir.
 *
 * @param {nsCOMPtr<nsIFile>} aOutFile - the nsIFile pointer holding the file info.
 * @return {nsresult} NS_OK if the persistence file was found, a failure value otherwise.
 */
nsresult
GetPersistenceFile(nsCOMPtr<nsIFile>& aOutFile)
{
  nsTString<PathChar> dataDir;
  nsresult rv = GetAndroidDataDir(dataDir);
  NS_ENSURE_SUCCESS(rv, rv);

  // Append the extension to the filename.
  nsAutoString fileName;
  fileName.Assign(kPersistenceFileName);

  aOutFile = new nsLocalFile(dataDir);
  aOutFile->Append(fileName);
  ANDROID_LOG("GetPersistenceFile -  %s", aOutFile->HumanReadablePath().get());
  return NS_OK;
}

/**
 * Read and parses JSON content from a file.
 *
 * @param {nsCOMPtr<nsIFile>} aFile - the nsIFile handle to the file.
 * @param {nsACString} fileContent - the content of the file.
 * @return {nsresult} NS_OK if the file was correctly read, an error code otherwise.
 */
nsresult
ReadFromFile(const nsCOMPtr<nsIFile>& aFile, nsACString& fileContent)
{
  int64_t fileSize = 0;
  nsresult rv = aFile->GetFileSize(&fileSize);
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsCOMPtr<nsIInputStream> inStream;
  rv = NS_NewLocalFileInputStream(getter_AddRefs(inStream),
                                  aFile,
                                  PR_RDONLY);
  NS_ENSURE_SUCCESS(rv, rv);

  // Make sure to close the stream.
  auto scopedStreamClose = MakeScopeExit([inStream] { inStream->Close(); });

  rv = NS_ReadInputStreamToString(inStream, fileContent, fileSize);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

/**
 * Arms the persistence timer and instructs to run the persistence
 * task off the main thread.
 */
void
MainThreadArmPersistenceTimer()
{
  MOZ_ASSERT(NS_IsMainThread());
  ANDROID_LOG("MainThreadArmPersistenceTimer");

  // We won't have a persistence timer the first time this runs, so take
  // care of that.
  if (!gPersistenceTimer) {
    gPersistenceTimer =
      NS_NewTimer(SystemGroup::EventTargetFor(TaskCategory::Other)).take();
    if (!gPersistenceTimer) {
      ANDROID_LOG("MainThreadArmPersistenceTimer - Timer creation failed.");
      return;
    }
  }

  // Define the callback for the persistence timer: it will dispatch the persistence
  // task off the main thread. Once finished, it will trigger the timer again.
  nsTimerCallbackFunc timerCallback = [](nsITimer* aTimer, void* aClosure) {
    gPersistenceThread->Dispatch(NS_NewRunnableFunction("PersistenceThreadPersist",
      []() -> void { ::PersistenceThreadPersist(); }));
  };

  uint32_t timeout = Preferences::GetUint("toolkit.telemetry.geckoPersistenceTimeout",
                                          kDefaultPersistenceTimeoutMs);

  // Schedule the timer to automatically run and reschedule
  // every |kPersistenceTimeoutMs|.
  gPersistenceTimer->InitWithNamedFuncCallback(timerCallback,
                                               nullptr,
                                               timeout,
                                               nsITimer::TYPE_ONE_SHOT_LOW_PRIORITY,
                                               "TelemetryGeckoViewPersistence::Persist");
}

/**
 * Parse the string data into a JSON structure, using
 * the native JS JSON parser.
 */
void
MainThreadParsePersistedProbes(const nsACString& aProbeData)
{
  // We're required to run on the main thread since we're using JS.
  MOZ_ASSERT(NS_IsMainThread());
  ANDROID_LOG("MainThreadParsePersistedProbes");

  // We need a JS context to run the parsing stuff in.
  JSObject* cleanGlobal =
    SimpleGlobalObject::Create(SimpleGlobalObject::GlobalType::BindingDetail);
  if (!cleanGlobal) {
    ANDROID_LOG("MainThreadParsePersistedProbes - Failed to create a JS global object");
    return;
  }

  AutoJSAPI jsapi;
  if (NS_WARN_IF(!jsapi.Init(cleanGlobal))) {
    ANDROID_LOG("MainThreadParsePersistedProbes - Failed to get JS API");
    return;
  }

  // Parse the JSON using the JS API.
  JS::RootedValue data(jsapi.cx());
  NS_ConvertUTF8toUTF16 utf16Content(aProbeData);
  if (!JS_ParseJSON(jsapi.cx(), utf16Content.BeginReading(), utf16Content.Length(), &data)) {
    ANDROID_LOG("MainThreadParsePersistedProbes - Failed to parse the persisted JSON");
    return;
  }

  // Get the data for the scalars.
  JS::RootedObject dataObj(jsapi.cx(), &data.toObject());
  JS::RootedValue scalarData(jsapi.cx());
  if (JS_GetProperty(jsapi.cx(), dataObj, "scalars", &scalarData)) {
    // If the data is an object, try to parse its properties. If not,
    // silently skip and try to load the other sections.
    if (!scalarData.isObject()
        || NS_FAILED(TelemetryScalar::DeserializePersistedScalars(jsapi.cx(), scalarData))) {
      ANDROID_LOG("MainThreadParsePersistedProbes - Failed to parse 'scalars'.");
      MOZ_ASSERT(!JS_IsExceptionPending(jsapi.cx()), "Parsers must suppress exceptions themselves");
    }
  } else {
    // Getting the "scalars" property failed, suppress the exception
    // and continue.
    JS_ClearPendingException(jsapi.cx());
  }

  JS::RootedValue keyedScalarData(jsapi.cx());
  if (JS_GetProperty(jsapi.cx(), dataObj, "keyedScalars", &keyedScalarData)) {
    // If the data is an object, try to parse its properties. If not,
    // silently skip and try to load the other sections.
    if (!keyedScalarData.isObject()
        || NS_FAILED(TelemetryScalar::DeserializePersistedKeyedScalars(jsapi.cx(), keyedScalarData))) {
      ANDROID_LOG("MainThreadParsePersistedProbes - Failed to parse 'keyedScalars'.");
      MOZ_ASSERT(!JS_IsExceptionPending(jsapi.cx()), "Parsers must suppress exceptions themselves");
    }
  } else {
    // Getting the "keyedScalars" property failed, suppress the exception
    // and continue.
    JS_ClearPendingException(jsapi.cx());
  }
}

/**
 * The persistence worker function, meant to be run off the main thread.
 */
void
PersistenceThreadPersist()
{
  MOZ_ASSERT(XRE_IsParentProcess(), "We must only persist from the parent process.");
  MOZ_ASSERT(!NS_IsMainThread(), "This function must be called off the main thread.");
  ANDROID_LOG("PersistenceThreadPersist");

  // If the function completes or fails, make sure to spin up the persistence timer again.
  auto scopedArmTimer = MakeScopeExit([&] {
    NS_DispatchToMainThread(
      NS_NewRunnableFunction("MainThreadArmPersistenceTimer", []() -> void {
        MainThreadArmPersistenceTimer();
      }));
  });

  nsCOMPtr<nsIFile> persistenceFile;
  if (NS_FAILED(GetPersistenceFile(persistenceFile))) {
    ANDROID_LOG("PersistenceThreadPersist - Failed to get the persistence file.");
    return;
  }

  // Open the persistence file.
  mozilla::UniquePtr<StreamingJSONWriter> jsonWriter =
    mozilla::MakeUnique<StreamingJSONWriter>();

  if (!jsonWriter || NS_FAILED(jsonWriter->Open(persistenceFile))) {
    ANDROID_LOG("PersistenceThreadPersist - There was an error opening the persistence file.");
    return;
  }

  // Build the JSON structure: give up the ownership of jsonWriter.
  mozilla::JSONWriter w(mozilla::Move(jsonWriter));
  w.Start();

  w.StartObjectProperty("scalars");
  if (NS_FAILED(TelemetryScalar::SerializeScalars(w))) {
    ANDROID_LOG("Persist - Failed to persist scalars.");
  }
  w.EndObject();

  w.StartObjectProperty("keyedScalars");
  if (NS_FAILED(TelemetryScalar::SerializeKeyedScalars(w))) {
    ANDROID_LOG("Persist - Failed to persist keyed scalars.");
  }
  w.EndObject();

  // End the building process.
  w.End();

  // Android can kill us while we are writing to disk and, if that happens,
  // we end up with a corrupted json overwriting the old session data.
  // Luckily, |StreamingJSONWriter::Close| is smart enough to write to a
  // temporary file and only overwrite the original file if nothing bad happened.
  nsresult rv = static_cast<StreamingJSONWriter*>(w.WriteFunc())->Close();
  if (NS_FAILED(rv)) {
    ANDROID_LOG("PersistenceThreadPersist - There was an error writing to the persistence file.");
    return;
  }
}

/**
 * This function loads the persisted metrics from a JSON file
 * and adds them to the related storage. After it completes,
 * it spins up the persistence timer.
 *
 * Please note that this function is meant to be run off the
 * main-thread.
 */
void
PersistenceThreadLoadData()
{
  MOZ_ASSERT(XRE_IsParentProcess(), "We must only persist from the parent process.");
  MOZ_ASSERT(!NS_IsMainThread(), "We must perform I/O off the main thread.");
  ANDROID_LOG("PersistenceThreadLoadData");

  // If the function completes or fails, make sure to spin up the persistence timer.
  nsAutoCString fileContent;
  auto scopedArmTimer = MakeScopeExit([&] {
    NS_DispatchToMainThread(
      NS_NewRunnableFunction("MainThreadArmPersistenceTimer", [fileContent]() -> void {
        // Try to parse the probes if the file was not empty.
        if (!fileContent.IsEmpty()) {
          MainThreadParsePersistedProbes(fileContent);
        }
        // Arm the timer.
        MainThreadArmPersistenceTimer();
        // Notify that we're good to take snapshots!
        nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
        if (os) {
          os->NotifyObservers(nullptr, kLoadCompleteTopic, nullptr);
        }
      }));
  });

  // Attempt to load the persistence file. This could fail if we're not able
  // to allocate enough memory for the content. See bug 1460911.
  nsCOMPtr<nsIFile> persistenceFile;
  if (NS_FAILED(GetPersistenceFile(persistenceFile))
      || NS_FAILED(ReadFromFile(persistenceFile, fileContent))) {
    ANDROID_LOG("PersistenceThreadLoadData - Failed to load cache file at %s",
                persistenceFile->HumanReadablePath().get());
    return;
  }
}

} // anonymous namespace

// This namespace exposes testing only helpers to simplify writing
// gtest cases.
namespace TelemetryGeckoViewTesting {

void
TestDispatchPersist()
{
  gPersistenceThread->Dispatch(NS_NewRunnableFunction("Persist",
    []() -> void { ::PersistenceThreadPersist(); }));
}

} // GeckoViewTesting

void
TelemetryGeckoViewPersistence::InitPersistence()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (gPersistenceThread) {
    ANDROID_LOG("Init must only be called once.");
    return;
  }

  // Only register the persistence timer in the parent process in
  // order to persist data for all the processes.
  if (!XRE_IsParentProcess()) {
    ANDROID_LOG("InitPersistence - Bailing out on child process.");
    return;
  }

  ANDROID_LOG("InitPersistence");

  // Spawn a new thread for handling GeckoView Telemetry persistence I/O.
  // We just spawn it once and re-use it later.
  nsCOMPtr<nsIThread> thread;
  nsresult rv =
    NS_NewNamedThread("TelemetryGVIO", getter_AddRefs(thread));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    ANDROID_LOG("InitPersistence -  Failed to instantiate the worker thread.");
    return;
  }

  gPersistenceThread = thread.forget();

  // Trigger the loading of the persistence data. After the function
  // completes it will automatically arm the persistence timer.
  gPersistenceThread->Dispatch(
    NS_NewRunnableFunction("PersistenceThreadLoadData", &PersistenceThreadLoadData));
}

void
TelemetryGeckoViewPersistence::DeInitPersistence()
{
  MOZ_ASSERT(NS_IsMainThread());

  // Bail out if this is not the parent process.
  if (!XRE_IsParentProcess()) {
    ANDROID_LOG("DeInitPersistence - Bailing out.");
    return;
  }

  // Even though we need to implement this function, it might end up
  // not being called: Android might kill us without notice to reclaim
  // our memory in case some other foreground application needs it.
  ANDROID_LOG("DeInitPersistence");

  if (gPersistenceThread) {
    gPersistenceThread->Shutdown();
    gPersistenceThread = nullptr;
  }

  if (gPersistenceTimer) {
    // Always make sure the timer is canceled.
    MOZ_ALWAYS_SUCCEEDS(gPersistenceTimer->Cancel());
    NS_RELEASE(gPersistenceTimer);
  }
}

void
TelemetryGeckoViewPersistence::ClearPersistenceData()
{
  // This can be run on any thread, as we just dispatch the persistence
  // task to the persistence thread.
  MOZ_ASSERT(gPersistenceThread);

  ANDROID_LOG("ClearPersistenceData");

  // Trigger clearing the persisted measurements off the main thread.
  gPersistenceThread->Dispatch(NS_NewRunnableFunction("ClearPersistedData",
    []() -> void {
      nsCOMPtr<nsIFile> persistenceFile;
      if (NS_FAILED(GetPersistenceFile(persistenceFile)) ||
          NS_FAILED(persistenceFile->Remove(false))) {
        ANDROID_LOG("ClearPersistenceData - Failed to remove the persistence file.");
        return;
      }
    }));
}

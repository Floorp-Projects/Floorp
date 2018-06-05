/* vim:set ts=2 sw=2 sts=2 et: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

#include "gtest/gtest.h"
#include "mozilla/JSONWriter.h"
#include "nsDirectoryServiceDefs.h"
#include "nsIObserver.h"
#include "nsIObserverService.h"
#include "nsIOutputStream.h"
#include "nsITelemetry.h"
#include "nsJSUtils.h"
#include "nsNetUtil.h"
#include "nsThreadUtils.h"
#include "nsPrintfCString.h"
#include "prenv.h"
#include "Telemetry.h"
#include "TelemetryFixture.h"
#include "TelemetryGeckoViewPersistence.h"
#include "TelemetryScalar.h"
#include "TelemetryTestHelpers.h"

using namespace mozilla;
using namespace TelemetryTestHelpers;

const char kSampleData[] = R"({
  "scalars": {
    "content": {
      "telemetry.test.all_processes_uint": 37
    }
  },
  "keyedScalars": {
    "parent": {
      "telemetry.test.keyed_unsigned_int": {
        "testKey": 73
      }
    }
  }
})";

const char16_t kPersistedFilename[] = u"gv_measurements.json";
const char kDataLoadedTopic[] = "internal-telemetry-geckoview-load-complete";

namespace {

/**
 * Using gtest assertion macros requires the containing function to return
 * a void type. For this reason, all the functions below are using that return
 * type.
 */
void
GetMockedDataDir(nsAString& aMockedDir)
{
  // Get the OS temporary directory.
  nsCOMPtr<nsIFile> tmpDir;
  nsresult rv = NS_GetSpecialDirectory(NS_OS_TEMP_DIR,
                                       getter_AddRefs(tmpDir));
  ASSERT_EQ(NS_SUCCEEDED(rv), true);
  // Return the mocked dir.
  rv = tmpDir->GetPath(aMockedDir);
  ASSERT_EQ(NS_SUCCEEDED(rv), true);
}

void
MockAndroidDataDir()
{
  // Get the OS temporary directory.
  nsAutoString mockedPath;
  GetMockedDataDir(mockedPath);

  // Set the environment variable to mock.
  // Note: we intentionally leak it with |ToNewCString| as PR_SetEnv forces
  // us to!
  nsAutoCString mockedEnv(
    nsPrintfCString("MOZ_ANDROID_DATA_DIR=%s", NS_ConvertUTF16toUTF8(mockedPath).get()));
  ASSERT_EQ(PR_SetEnv(ToNewCString(mockedEnv)), PR_SUCCESS);
}

void
WritePersistenceFile(const nsACString& aData)
{
  // Write the file to the temporary directory.
  nsCOMPtr<nsIFile> file;
  nsresult rv = NS_GetSpecialDirectory(NS_OS_TEMP_DIR,
                                       getter_AddRefs(file));
  ASSERT_EQ(NS_SUCCEEDED(rv), true);

  // Append the filename and the extension.
  nsAutoString fileName;
  fileName.Append(kPersistedFilename);
  file->Append(fileName);

  nsCOMPtr<nsIOutputStream> stream;
  rv = NS_NewLocalFileOutputStream(getter_AddRefs(stream), file);
  ASSERT_EQ(NS_SUCCEEDED(rv), true);

  uint32_t count;
  rv = stream->Write(aData.Data(), aData.Length(), &count);
  // Make sure we wrote correctly.
  ASSERT_EQ(NS_SUCCEEDED(rv), true);
  ASSERT_EQ(count, aData.Length());

  stream->Close();
}

void
RemovePersistenceFile()
{
  nsCOMPtr<nsIFile> file;
  nsresult rv = NS_GetSpecialDirectory(NS_OS_TEMP_DIR,
                                       getter_AddRefs(file));
  ASSERT_EQ(NS_SUCCEEDED(rv), true);

  // Append the filename and the extension.
  nsAutoString fileName;
  fileName.Append(kPersistedFilename);
  file->Append(fileName);

  bool exists = true;
  rv = file->Exists(&exists);
  ASSERT_EQ(NS_OK, rv) << "nsIFile::Exists cannot fail";

  if (exists) {
    rv = file->Remove(false);
    ASSERT_EQ(NS_OK, rv) << "nsIFile::Remove cannot delete the requested file";
  }
}

void
CheckPersistenceFileExists(bool& aFileExists)
{
  nsCOMPtr<nsIFile> file;
  nsresult rv = NS_GetSpecialDirectory(NS_OS_TEMP_DIR,
                                       getter_AddRefs(file));
  ASSERT_EQ(NS_OK, rv) << "NS_GetSpecialDirectory must return a valid directory";

  // Append the filename and the extension.
  nsAutoString fileName;
  fileName.Append(kPersistedFilename);
  file->Append(fileName);

  rv = file->Exists(&aFileExists);
  ASSERT_EQ(NS_OK, rv) << "nsIFile::Exists must not fail";
}

/**
 * A helper class to wait for the internal "data loaded"
 * topic.
 */
class DataLoadedObserver final : public nsIObserver
{
  ~DataLoadedObserver() = default;

public:
  NS_DECL_ISUPPORTS

  explicit DataLoadedObserver() :
    mDataLoaded(false)
  {
    // The following line can fail to fetch the observer service. However,
    // since we're test code, we're fine with crashing due to that.
    nsCOMPtr<nsIObserverService> observerService =
      mozilla::services::GetObserverService();
    observerService->AddObserver(this, kDataLoadedTopic, false);
  }

  void WaitForNotification()
  {
    mozilla::SpinEventLoopUntil([&]() { return mDataLoaded; });
  }

  NS_IMETHOD Observe(nsISupports* aSubject,
                     const char* aTopic,
                     const char16_t* aData) override
  {
    if (!strcmp(aTopic, kDataLoadedTopic)) {
      nsCOMPtr<nsIObserverService> observerService =
        mozilla::services::GetObserverService();
      observerService->RemoveObserver(this, kDataLoadedTopic);
      mDataLoaded = true;
    }

    return NS_OK;
  }

private:
  bool mDataLoaded;
};

NS_IMPL_ISUPPORTS(
  DataLoadedObserver,
  nsIObserver
)

} // Anonymous

/**
 * A GeckoView specific test fixture. Please note that this
 * can't live in the above anonymous namespace.
 */
class TelemetryGeckoViewFixture : public TelemetryTestFixture {
protected:
  virtual void SetUp() {
    TelemetryTestFixture::SetUp();
    MockAndroidDataDir();
  }
};

namespace TelemetryGeckoViewTesting {

void TestDispatchPersist();

} // TelemetryGeckoViewTesting

/**
 * Test that corrupted JSON files don't crash the Telemetry core.
 */
TEST_F(TelemetryGeckoViewFixture, CorruptedPersistenceFiles) {
  AutoJSContextWithGlobal cx(mCleanGlobal);

  // Try to load a corrupted file.
  WritePersistenceFile(NS_LITERAL_CSTRING("{"));
  TelemetryGeckoViewPersistence::InitPersistence();
  TelemetryGeckoViewPersistence::DeInitPersistence();

  // Cleanup/remove the files.
  RemovePersistenceFile();
}

/**
 * Test that valid and empty JSON files don't crash the Telemetry core.
 */
TEST_F(TelemetryGeckoViewFixture, EmptyPersistenceFiles) {
  AutoJSContextWithGlobal cx(mCleanGlobal);

  // Try to load an empty file/corrupted file.
  WritePersistenceFile(EmptyCString());
  TelemetryGeckoViewPersistence::InitPersistence();
  TelemetryGeckoViewPersistence::DeInitPersistence();

  // Cleanup/remove the files.
  RemovePersistenceFile();
}

/**
 * Test that we're able to clear the persistence storage.
 */
TEST_F(TelemetryGeckoViewFixture, ClearPersistenceFiles) {
  AutoJSContextWithGlobal cx(mCleanGlobal);

  bool fileExists = false;
  CheckPersistenceFileExists(fileExists);
  ASSERT_FALSE(fileExists) << "No persisted measurements must exist on the disk";

  WritePersistenceFile(nsDependentCString(kSampleData));
  CheckPersistenceFileExists(fileExists);
  ASSERT_TRUE(fileExists) << "We should have written the test persistence file to disk";

  // Init the persistence: this will trigger the measurements to be written
  // to disk off-the-main thread.
  TelemetryGeckoViewPersistence::InitPersistence();
  TelemetryGeckoViewPersistence::ClearPersistenceData();
  TelemetryGeckoViewPersistence::DeInitPersistence();

  CheckPersistenceFileExists(fileExists);
  ASSERT_FALSE(fileExists) << "ClearPersistenceData must remove the persistence file";
}

/**
 * Test that the data loaded topic gets notified correctly.
 */
TEST_F(TelemetryGeckoViewFixture, CheckDataLoadedTopic) {
  AutoJSContextWithGlobal cx(mCleanGlobal);

  bool fileExists = false;
  CheckPersistenceFileExists(fileExists);
  ASSERT_FALSE(fileExists) << "No persisted measurements must exist on the disk";

  // Check that the data loaded topic is notified after attempting the load
  // if no measurement file exists.
  RefPtr<DataLoadedObserver> loadingFinished = new DataLoadedObserver();
  TelemetryGeckoViewPersistence::InitPersistence();
  loadingFinished->WaitForNotification();
  TelemetryGeckoViewPersistence::DeInitPersistence();

  // Check that the topic is triggered when the measuements file exists.
  WritePersistenceFile(nsDependentCString(kSampleData));
  CheckPersistenceFileExists(fileExists);
  ASSERT_TRUE(fileExists) << "The persisted measurements must exist on the disk";

  // Check that the data loaded topic is triggered when the measurement file exists.
  loadingFinished = new DataLoadedObserver();
  TelemetryGeckoViewPersistence::InitPersistence();
  loadingFinished->WaitForNotification();
  TelemetryGeckoViewPersistence::DeInitPersistence();

  // Cleanup/remove the files.
  RemovePersistenceFile();
}

/**
 * Test that we can correctly persist the scalar data.
 */
TEST_F(TelemetryGeckoViewFixture, PersistScalars) {
  AutoJSContextWithGlobal cx(mCleanGlobal);

  Unused << mTelemetry->ClearScalars();

  bool fileExists = false;
  CheckPersistenceFileExists(fileExists);
  ASSERT_FALSE(fileExists) << "No persisted measurements must exist on the disk";

  RefPtr<DataLoadedObserver> loadingFinished = new DataLoadedObserver();

  // Init the persistence: this will trigger the measurements to be written
  // to disk off-the-main thread.
  TelemetryGeckoViewPersistence::InitPersistence();
  loadingFinished->WaitForNotification();

  // Set some scalars: we can only test the parent process as we don't support other
  // processes in gtests.
  const uint32_t kExpectedUintValue = 37;
  const uint32_t kExpectedKeyedUintValue = 73;
  Telemetry::ScalarSet(Telemetry::ScalarID::TELEMETRY_TEST_ALL_PROCESSES_UINT, kExpectedUintValue);
  Telemetry::ScalarSet(Telemetry::ScalarID::TELEMETRY_TEST_KEYED_UNSIGNED_INT,
                       NS_LITERAL_STRING("gv_key"), kExpectedKeyedUintValue);

  // Dispatch the persisting task: we don't wait for the timer to expire
  // as we need a reliable and reproducible way to kick this off. We ensure
  // that the task runs by shutting down the persistence: this shuts down the
  // thread which executes the task as the last action.
  TelemetryGeckoViewTesting::TestDispatchPersist();
  TelemetryGeckoViewPersistence::DeInitPersistence();

  CheckPersistenceFileExists(fileExists);
  ASSERT_TRUE(fileExists) << "The persisted measurements must exist on the disk";

  // Clear the in-memory scalars again. They will be restored from the disk.
  Unused << mTelemetry->ClearScalars();

  // Load the persisted file again.
  TelemetryGeckoViewPersistence::InitPersistence();
  TelemetryGeckoViewPersistence::DeInitPersistence();

  // Get a snapshot of the keyed and plain scalars.
  JS::RootedValue scalarsSnapshot(cx.GetJSContext());
  JS::RootedValue keyedScalarsSnapshot(cx.GetJSContext());
  GetScalarsSnapshot(false, cx.GetJSContext(), &scalarsSnapshot);
  GetScalarsSnapshot(true, cx.GetJSContext(), &keyedScalarsSnapshot);

  // Verify that the scalars were correctly persisted and restored.
  CheckUintScalar("telemetry.test.all_processes_uint", cx.GetJSContext(),
                  scalarsSnapshot, kExpectedUintValue);
  CheckKeyedUintScalar("telemetry.test.keyed_unsigned_int", "gv_key", cx.GetJSContext(),
                       keyedScalarsSnapshot, kExpectedKeyedUintValue);

  // Cleanup/remove the files.
  RemovePersistenceFile();
}

/**
 * Test that we can correctly persist the histogram data.
 */
TEST_F(TelemetryGeckoViewFixture, PersistHistograms) {
  AutoJSContextWithGlobal cx(mCleanGlobal);

  // Clear the histogram data.
  GetAndClearHistogram(cx.GetJSContext(), mTelemetry,
                       NS_LITERAL_CSTRING("TELEMETRY_TEST_MULTIPRODUCT"), false /* is_keyed */);
  GetAndClearHistogram(cx.GetJSContext(), mTelemetry,
                       NS_LITERAL_CSTRING("TELEMETRY_TEST_KEYED_COUNT"), true /* is_keyed */);

  bool fileExists = false;
  CheckPersistenceFileExists(fileExists);
  ASSERT_FALSE(fileExists) << "No persisted measurements must exist on the disk";

  RefPtr<DataLoadedObserver> loadingFinished = new DataLoadedObserver();

  // Init the persistence: this will trigger the measurements to be written
  // to disk off-the-main thread.
  TelemetryGeckoViewPersistence::InitPersistence();
  loadingFinished->WaitForNotification();

  // Set some histograms: we can only test the parent process as we don't support other
  // processes in gtests.
  const uint32_t kExpectedUintValue = 37;
  const nsTArray<uint32_t> keyedSamples({5, 10, 15});
  const uint32_t kExpectedKeyedSum = 5 + 10 + 15;
  Telemetry::Accumulate(Telemetry::TELEMETRY_TEST_MULTIPRODUCT, kExpectedUintValue);
  Telemetry::Accumulate(Telemetry::TELEMETRY_TEST_KEYED_COUNT, NS_LITERAL_CSTRING("gv_key"),
                        keyedSamples);

  // Dispatch the persisting task: we don't wait for the timer to expire
  // as we need a reliable and reproducible way to kick off this. We ensure
  // that the task runs by shutting down the persistence: this shuts down the
  // thread which executes the task as the last action.
  TelemetryGeckoViewTesting::TestDispatchPersist();
  TelemetryGeckoViewPersistence::DeInitPersistence();

  CheckPersistenceFileExists(fileExists);
  ASSERT_TRUE(fileExists) << "The persisted measurements must exist on the disk";

  // Clear the in-memory histograms again. They will be restored from the disk.
  GetAndClearHistogram(cx.GetJSContext(), mTelemetry,
                       NS_LITERAL_CSTRING("TELEMETRY_TEST_MULTIPRODUCT"), false /* is_keyed */);
  GetAndClearHistogram(cx.GetJSContext(), mTelemetry,
                       NS_LITERAL_CSTRING("TELEMETRY_TEST_KEYED_COUNT"), true /* is_keyed */);


  // Load the persisted file again.
  TelemetryGeckoViewPersistence::InitPersistence();
  TelemetryGeckoViewPersistence::DeInitPersistence();

  // Get a snapshot of the keyed and plain histograms.
  JS::RootedValue snapshot(cx.GetJSContext());
  JS::RootedValue keyedSnapshot(cx.GetJSContext());
  GetSnapshots(cx.GetJSContext(), mTelemetry, "TELEMETRY_TEST_MULTIPRODUCT",
               &snapshot, false /* is_keyed */);
  GetSnapshots(cx.GetJSContext(), mTelemetry, "TELEMETRY_TEST_KEYED_COUNT",
               &keyedSnapshot, true /* is_keyed */);

  // Validate the loaded histogram data.
  JS::RootedValue histogram(cx.GetJSContext());
  GetProperty(cx.GetJSContext(), "TELEMETRY_TEST_MULTIPRODUCT", snapshot, &histogram);

  // Get "sum" property from histogram
  JS::RootedValue sum(cx.GetJSContext());
  GetProperty(cx.GetJSContext(), "sum", histogram,  &sum);

  // Check that the "sum" stored in the histogram matches with |kExpectedValue|
  uint32_t uSum = 0;
  JS::ToUint32(cx.GetJSContext(), sum, &uSum);
  ASSERT_EQ(uSum, kExpectedUintValue) << "The histogram is not returning the expected value";

  // Validate the keyed histogram data.
  GetProperty(cx.GetJSContext(), "TELEMETRY_TEST_KEYED_COUNT", keyedSnapshot, &histogram);

  // Get "testkey" property from histogram and check that it stores the correct
  // data.
  JS::RootedValue expectedKeyData(cx.GetJSContext());
  GetProperty(cx.GetJSContext(), "gv_key", histogram,  &expectedKeyData);
  ASSERT_FALSE(expectedKeyData.isUndefined())
    << "Cannot find the expected key in the keyed histogram data";
  GetProperty(cx.GetJSContext(), "sum", expectedKeyData,  &sum);
  JS::ToUint32(cx.GetJSContext(), sum, &uSum);
  ASSERT_EQ(uSum, kExpectedKeyedSum)
    << "The histogram is not returning the expected sum for 'gv_key'";

  // Cleanup/remove the files.
  RemovePersistenceFile();
}

/**
 * Test GeckoView timer telemetry is correctly recorded.
 */
TEST_F(TelemetryGeckoViewFixture, TimerHitCountProbe) {
  AutoJSContextWithGlobal cx(mCleanGlobal);

  Unused << mTelemetry->ClearScalars();

  // Init the persistence and wait for loading to complete.
  RefPtr<DataLoadedObserver> loadingFinished = new DataLoadedObserver();
  TelemetryGeckoViewPersistence::InitPersistence();
  loadingFinished->WaitForNotification();
  // Simulate hitting the timer twice.
  TelemetryGeckoViewTesting::TestDispatchPersist();
  TelemetryGeckoViewTesting::TestDispatchPersist();
  TelemetryGeckoViewPersistence::DeInitPersistence();

  // Get a snapshot of the keyed and plain scalars.
  JS::RootedValue scalarsSnapshot(cx.GetJSContext());
  GetScalarsSnapshot(false, cx.GetJSContext(), &scalarsSnapshot);

  // Verify that the scalars were correctly persisted and restored.
  CheckUintScalar("telemetry.persistence_timer_hit_count", cx.GetJSContext(),
                  scalarsSnapshot, 2);

  // Cleanup/remove the files.
  RemovePersistenceFile();
}

TEST_F(TelemetryGeckoViewFixture, EmptyPendingOperations) {
  AutoJSContextWithGlobal cx(mCleanGlobal);

  Unused << mTelemetry->ClearScalars();

  // Force loading mode
  TelemetryScalar::DeserializationStarted();

  // Do nothing explicitely

  // Force pending operations to be applied and end load mode.
  // It should not crash and don't change any scalars.
  TelemetryScalar::ApplyPendingOperations();

  // Check that the snapshot is empty
  JS::RootedValue scalarsSnapshot(cx.GetJSContext());
  GetScalarsSnapshot(false, cx.GetJSContext(), &scalarsSnapshot);

  ASSERT_TRUE(scalarsSnapshot.isUndefined()) << "Scalars snapshot should not contain any data.";
}

TEST_F(TelemetryGeckoViewFixture, SimpleAppendOperation) {
  AutoJSContextWithGlobal cx(mCleanGlobal);

  Unused << mTelemetry->ClearScalars();

  // Set an initial value, so we can test that it is not overwritten.
  uint32_t initialValue = 1;
  Telemetry::ScalarSet(Telemetry::ScalarID::TELEMETRY_TEST_UNSIGNED_INT_KIND, initialValue);

  // Force loading mode
  TelemetryScalar::DeserializationStarted();

  // Add to a scalar
  uint32_t value = 37;
  Telemetry::ScalarAdd(Telemetry::ScalarID::TELEMETRY_TEST_UNSIGNED_INT_KIND, value);

  // Verify that this was not yet applied.
  JS::RootedValue scalarsSnapshot(cx.GetJSContext());
  GetScalarsSnapshot(false, cx.GetJSContext(), &scalarsSnapshot);

  CheckUintScalar("telemetry.test.unsigned_int_kind", cx.GetJSContext(), scalarsSnapshot, initialValue);

  // Force pending operations to be applied and end load mode
  TelemetryScalar::ApplyPendingOperations();

  // Verify recorded operations are applied
  GetScalarsSnapshot(false, cx.GetJSContext(), &scalarsSnapshot);
  CheckUintScalar("telemetry.test.unsigned_int_kind", cx.GetJSContext(), scalarsSnapshot, initialValue+value);
}

TEST_F(TelemetryGeckoViewFixture, ApplyPendingOperationsAfterLoad) {
  AutoJSContextWithGlobal cx(mCleanGlobal);

  Unused << mTelemetry->ClearScalars();

  const char persistedData[] = R"({
 "scalars": {
  "parent": {
   "telemetry.test.unsigned_int_kind": 14
  }
 }
})";

  // Force loading mode
  TelemetryScalar::DeserializationStarted();

  // Add to a scalar, this should be recorded
  uint32_t addValue = 10;
  Telemetry::ScalarAdd(Telemetry::ScalarID::TELEMETRY_TEST_UNSIGNED_INT_KIND, addValue);

  // Load persistence file
  RefPtr<DataLoadedObserver> loadingFinished = new DataLoadedObserver();
  WritePersistenceFile(nsDependentCString(persistedData));
  TelemetryGeckoViewPersistence::InitPersistence();
  loadingFinished->WaitForNotification();

  // At this point all pending operations should have been applied.

  // Increment again, now directly applied
  uint32_t val = 1;
  Telemetry::ScalarAdd(Telemetry::ScalarID::TELEMETRY_TEST_UNSIGNED_INT_KIND, val);


  JS::RootedValue scalarsSnapshot(cx.GetJSContext());
  GetScalarsSnapshot(false, cx.GetJSContext(), &scalarsSnapshot);

  uint32_t expectedValue = 25;
  CheckUintScalar("telemetry.test.unsigned_int_kind", cx.GetJSContext(), scalarsSnapshot, expectedValue);
}

TEST_F(TelemetryGeckoViewFixture, MultipleAppendOperations) {
  AutoJSContextWithGlobal cx(mCleanGlobal);

  Unused << mTelemetry->ClearScalars();

  // Force loading mode
  TelemetryScalar::DeserializationStarted();

  // Modify all kinds of scalars
  uint32_t startValue = 35;
  uint32_t expectedValue = 40;
  Telemetry::ScalarSet(Telemetry::ScalarID::TELEMETRY_TEST_UNSIGNED_INT_KIND, startValue);
  Telemetry::ScalarSetMaximum(Telemetry::ScalarID::TELEMETRY_TEST_UNSIGNED_INT_KIND, startValue + 2);
  Telemetry::ScalarAdd(Telemetry::ScalarID::TELEMETRY_TEST_UNSIGNED_INT_KIND, 3);

  Telemetry::ScalarSet(Telemetry::ScalarID::TELEMETRY_TEST_BOOLEAN_KIND, true);
  Telemetry::ScalarSet(Telemetry::ScalarID::TELEMETRY_TEST_STRING_KIND, NS_LITERAL_STRING("Star Wars VI"));

  // Modify all kinds of keyed scalars
  Telemetry::ScalarSet(Telemetry::ScalarID::TELEMETRY_TEST_KEYED_UNSIGNED_INT,
      NS_LITERAL_STRING("chewbacca"), startValue);
  Telemetry::ScalarSetMaximum(Telemetry::ScalarID::TELEMETRY_TEST_KEYED_UNSIGNED_INT,
      NS_LITERAL_STRING("chewbacca"), startValue + 2);
  Telemetry::ScalarAdd(Telemetry::ScalarID::TELEMETRY_TEST_KEYED_UNSIGNED_INT,
      NS_LITERAL_STRING("chewbacca"), 3);

  Telemetry::ScalarSet(Telemetry::ScalarID::TELEMETRY_TEST_KEYED_BOOLEAN_KIND,
      NS_LITERAL_STRING("chewbacca"),
      true);

  // Force pending operations to be applied and end load mode
  TelemetryScalar::ApplyPendingOperations();

  JS::RootedValue scalarsSnapshot(cx.GetJSContext());
  JS::RootedValue keyedScalarsSnapshot(cx.GetJSContext());
  GetScalarsSnapshot(false, cx.GetJSContext(), &scalarsSnapshot);
  GetScalarsSnapshot(true, cx.GetJSContext(), &keyedScalarsSnapshot);

  CheckUintScalar("telemetry.test.unsigned_int_kind", cx.GetJSContext(), scalarsSnapshot, expectedValue);
  CheckBoolScalar("telemetry.test.boolean_kind", cx.GetJSContext(), scalarsSnapshot, true);
  CheckStringScalar("telemetry.test.string_kind", cx.GetJSContext(), scalarsSnapshot, "Star Wars VI");

  CheckKeyedUintScalar("telemetry.test.keyed_unsigned_int", "chewbacca",
      cx.GetJSContext(), keyedScalarsSnapshot, expectedValue);
  CheckKeyedBoolScalar("telemetry.test.keyed_boolean_kind", "chewbacca",
      cx.GetJSContext(), keyedScalarsSnapshot, true);
}

TEST_F(TelemetryGeckoViewFixture, PendingOperationsHighWater) {
  AutoJSContextWithGlobal cx(mCleanGlobal);

  const char* testProbeName = "telemetry.test.unsigned_int_kind";
  const char* reachedName = "telemetry.pending_operations_highwatermark_reached";

  Unused << mTelemetry->ClearScalars();

  // Setting initial values so we can test them easily
  Telemetry::ScalarSet(Telemetry::ScalarID::TELEMETRY_TEST_UNSIGNED_INT_KIND, 0u);
  Telemetry::ScalarSet(Telemetry::ScalarID::TELEMETRY_PENDING_OPERATIONS_HIGHWATERMARK_REACHED, 0u);

  // Force loading mode
  TelemetryScalar::DeserializationStarted();

  // Fill up the pending operations list
  uint32_t expectedValue = 10000;
  for (uint32_t i=0; i < expectedValue; i++) {
    Telemetry::ScalarAdd(Telemetry::ScalarID::TELEMETRY_TEST_UNSIGNED_INT_KIND, 1);
  }

  // Nothing should be applied yet
  JS::RootedValue scalarsSnapshot(cx.GetJSContext());
  GetScalarsSnapshot(false, cx.GetJSContext(), &scalarsSnapshot);
  CheckUintScalar(testProbeName, cx.GetJSContext(), scalarsSnapshot, 0);
  CheckUintScalar(reachedName, cx.GetJSContext(), scalarsSnapshot, 0);

  // Spill over the buffer to immediately apply all operations
  Telemetry::ScalarAdd(Telemetry::ScalarID::TELEMETRY_TEST_UNSIGNED_INT_KIND, 1);

  // Now we should see all values
  GetScalarsSnapshot(false, cx.GetJSContext(), &scalarsSnapshot);
  CheckUintScalar(testProbeName, cx.GetJSContext(), scalarsSnapshot, expectedValue+1);
  CheckUintScalar(reachedName, cx.GetJSContext(), scalarsSnapshot, 1);
}

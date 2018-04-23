/* vim:set ts=2 sw=2 sts=2 et: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

#include "gtest/gtest.h"
#include "mozilla/JSONWriter.h"
#include "nsDirectoryServiceDefs.h"
#include "nsIOutputStream.h"
#include "nsITelemetry.h"
#include "nsJSUtils.h"
#include "nsNetUtil.h"
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

void
CheckJSONEqual(JSContext* aCx, JS::HandleValue aData, JS::HandleValue aDataOther)
{
  auto JSONCreator = [](const char16_t* aBuf, uint32_t aLen, void* aData) -> bool
  {
    nsAString* result = static_cast<nsAString*>(aData);
    result->Append(static_cast<const char16_t*>(aBuf),
                   static_cast<uint32_t>(aLen));
    return true;
  };

  // Unfortunately, we dont
  nsAutoString dataAsString;
  JS::RootedObject dataObj(aCx, &aData.toObject());
  ASSERT_TRUE(JS::ToJSONMaybeSafely(aCx, dataObj, JSONCreator, &dataAsString))
    << "The JS object must be correctly converted to a JSON string";

  nsAutoString otherAsString;
  JS::RootedObject otherObj(aCx, &aDataOther.toObject());
  ASSERT_TRUE(JS::ToJSONMaybeSafely(aCx, otherObj, JSONCreator, &otherAsString))
    << "The JS object must be correctly converted to a JSON string";

  ASSERT_TRUE(dataAsString.Equals(otherAsString))
    << "The JSON strings must match";
}

void
TestSerializeScalars(JSONWriter& aWriter)
{
  // Report the same data that's in kSampleData for scalars.
  // We only want to make sure that I/O and parsing works, as telemetry
  // measurement updates is taken care of by xpcshell tests.
  aWriter.StartObjectProperty("content");
  aWriter.IntProperty("telemetry.test.all_processes_uint", 37);
  aWriter.EndObject();
}

void
TestSerializeKeyedScalars(JSONWriter& aWriter)
{
  // Report the same data that's in kSampleData for keyed scalars.
  // We only want to make sure that I/O and parsing works, as telemetry
  // measurement updates is taken care of by xpcshell tests.
  aWriter.StartObjectProperty("parent");
  aWriter.StartObjectProperty("telemetry.test.keyed_unsigned_int");
  aWriter.IntProperty("testKey", 73);
  aWriter.EndObject();
  aWriter.EndObject();
}

void
TestDeserializePersistedScalars(JSContext* aCx, JS::HandleValue aData)
{
  // Get a JS object out of the JSON sample.
  JS::RootedValue sampleData(aCx);
  NS_ConvertUTF8toUTF16 utf16Content(kSampleData);
  ASSERT_TRUE(JS_ParseJSON(aCx, utf16Content.BeginReading(), utf16Content.Length(), &sampleData))
    << "Failed to create a JS object from the JSON sample";

  // Get sampleData["scalars"].
  JS::RootedObject sampleObj(aCx, &sampleData.toObject());
  JS::RootedValue scalarData(aCx);
  ASSERT_TRUE(JS_GetProperty(aCx, sampleObj, "scalars", &scalarData) && scalarData.isObject())
    << "Failed to get sampleData['scalars']";

  CheckJSONEqual(aCx, aData, scalarData);
}

void
TestDeserializePersistedKeyedScalars(JSContext* aCx, JS::HandleValue aData)
{
  // Get a JS object out of the JSON sample.
  JS::RootedValue sampleData(aCx);
  NS_ConvertUTF8toUTF16 utf16Content(kSampleData);
  ASSERT_TRUE(JS_ParseJSON(aCx, utf16Content.BeginReading(), utf16Content.Length(), &sampleData))
    << "Failed to create a JS object from the JSON sample";

  // Get sampleData["keyedScalars"].
  JS::RootedObject sampleObj(aCx, &sampleData.toObject());
  JS::RootedValue keyedScalarData(aCx);
  ASSERT_TRUE(JS_GetProperty(aCx, sampleObj, "keyedScalars", &keyedScalarData)
              && keyedScalarData.isObject()) << "Failed to get sampleData['keyedScalars']";

  CheckJSONEqual(aCx, aData, keyedScalarData);
}

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

/**
 * We can't link TelemetryScalar.cpp to these test files, so mock up
 * the required functions to make the linker not complain.
 */
namespace TelemetryScalar {

nsresult SerializeScalars(JSONWriter& aWriter) { TestSerializeScalars(aWriter); return NS_OK; }
nsresult SerializeKeyedScalars(JSONWriter& aWriter) { TestSerializeKeyedScalars(aWriter); return NS_OK; }
nsresult DeserializePersistedScalars(JSContext* aCx, JS::HandleValue aData) { TestDeserializePersistedScalars(aCx, aData); return NS_OK; }
nsresult DeserializePersistedKeyedScalars(JSContext* aCx, JS::HandleValue aData) { TestDeserializePersistedKeyedScalars(aCx, aData); return NS_OK; }

} // TelemetryScalar

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
 * Test that we can correctly persist the data.
 */
TEST_F(TelemetryGeckoViewFixture, PersistData) {
  AutoJSContextWithGlobal cx(mCleanGlobal);

  bool fileExists = false;
  CheckPersistenceFileExists(fileExists);
  ASSERT_FALSE(fileExists) << "No persisted measurements must exist on the disk";

  // Init the persistence: this will trigger the measurements to be written
  // to disk off-the-main thread.
  TelemetryGeckoViewPersistence::InitPersistence();

  // Dispatch the persisting task: we don't wait for the timer to expire
  // as we need a reliable and reproducible way to kick off this. We ensure
  // that the task runs by shutting down the persistence: this shuts down the
  // thread which executes the task as the last action.
  TelemetryGeckoViewTesting::TestDispatchPersist();
  TelemetryGeckoViewPersistence::DeInitPersistence();

  CheckPersistenceFileExists(fileExists);
  ASSERT_TRUE(fileExists) << "The persisted measurements must exist on the disk";

  // Load the persisted file again: this will trigger the TestLoad* functions
  // that will validate the data.
  TelemetryGeckoViewPersistence::InitPersistence();
  TelemetryGeckoViewPersistence::DeInitPersistence();

  // Cleanup/remove the files.
  RemovePersistenceFile();
}

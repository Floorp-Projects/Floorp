/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

#ifndef TelemetryFixture_h_
#define TelemetryFixture_h_

#include "mozilla/CycleCollectedJSContext.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/dom/SimpleGlobalObject.h"

using namespace mozilla;

class TelemetryTestFixture: public ::testing::Test {
protected:
  TelemetryTestFixture() : mCleanGlobal(nullptr) {}
  virtual void SetUp();

  JSObject* mCleanGlobal;

  nsCOMPtr<nsITelemetry> mTelemetry;
};

void
TelemetryTestFixture::SetUp()
{
  mTelemetry = do_GetService("@mozilla.org/base/telemetry;1");

  mCleanGlobal =
    dom::SimpleGlobalObject::Create(dom::SimpleGlobalObject::GlobalType::BindingDetail);

  // The test must fail if we failed getting the global.
  ASSERT_NE(mCleanGlobal, nullptr) << "SimpleGlobalObject must return a valid global object.";
}


// AutoJSAPI is annotated with MOZ_STACK_CLASS and thus cannot be
// used as a member of TelemetryTestFixture, since gtest instantiates
// that on the heap. To work around the problem, use the following class
// at the beginning of each Telemetry test.
// Note: this is very similar to AutoJSContext, but it allows to pass a
// global JS object in.
class MOZ_RAII AutoJSContextWithGlobal {
public:
  explicit AutoJSContextWithGlobal(JSObject* aGlobalObject);
  JSContext* GetJSContext() const;

protected:
  dom::AutoJSAPI mJsAPI;
  JSContext* mCx;
};

AutoJSContextWithGlobal::AutoJSContextWithGlobal(JSObject* aGlobalObject)
  : mCx(nullptr)
{
  // The JS API must initialize correctly.
  MOZ_ALWAYS_TRUE(mJsAPI.Init(aGlobalObject));
}

JSContext* AutoJSContextWithGlobal::GetJSContext() const
{
  return mJsAPI.cx();
}

#endif //TelemetryFixture_h_

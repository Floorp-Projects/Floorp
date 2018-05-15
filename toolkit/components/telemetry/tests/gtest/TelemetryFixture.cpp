/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

#include "TelemetryFixture.h"
#include "mozilla/dom/SimpleGlobalObject.h"

using namespace mozilla;

void
TelemetryTestFixture::SetUp()
{
  mTelemetry = do_GetService("@mozilla.org/base/telemetry;1");

  mCleanGlobal =
    dom::SimpleGlobalObject::Create(dom::SimpleGlobalObject::GlobalType::BindingDetail);

  // The test must fail if we failed getting the global.
  ASSERT_NE(mCleanGlobal, nullptr) << "SimpleGlobalObject must return a valid global object.";
}

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

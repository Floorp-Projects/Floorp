/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ExtensionTest.h"
#include "ExtensionEventManager.h"

#include "js/Equality.h"  // JS::StrictlyEqual
#include "mozilla/dom/ExtensionTestBinding.h"
#include "nsIGlobalObject.h"

namespace mozilla {
namespace extensions {

bool IsInAutomation(JSContext* aCx, JSObject* aGlobal) {
  return NS_IsMainThread()
             ? xpc::IsInAutomation()
             : dom::WorkerGlobalScope::IsInAutomation(aCx, aGlobal);
}

NS_IMPL_CYCLE_COLLECTING_ADDREF(ExtensionTest);
NS_IMPL_CYCLE_COLLECTING_RELEASE(ExtensionTest)
NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(ExtensionTest, mGlobal, mExtensionBrowser,
                                      mOnMessageEventMgr);

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(ExtensionTest)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

ExtensionTest::ExtensionTest(nsIGlobalObject* aGlobal,
                             ExtensionBrowser* aExtensionBrowser)
    : mGlobal(aGlobal), mExtensionBrowser(aExtensionBrowser) {
  MOZ_DIAGNOSTIC_ASSERT(mGlobal);
  MOZ_DIAGNOSTIC_ASSERT(mExtensionBrowser);
}

/* static */
bool ExtensionTest::IsAllowed(JSContext* aCx, JSObject* aGlobal) {
  // Allow browser.test API namespace while running in xpcshell tests.
  if (PR_GetEnv("XPCSHELL_TEST_PROFILE_DIR")) {
    return true;
  }

  return IsInAutomation(aCx, aGlobal);
}

JSObject* ExtensionTest::WrapObject(JSContext* aCx,
                                    JS::Handle<JSObject*> aGivenProto) {
  return dom::ExtensionTest_Binding::Wrap(aCx, this, aGivenProto);
}

nsIGlobalObject* ExtensionTest::GetParentObject() const { return mGlobal; }

void ExtensionTest::CallWebExtMethodAssertEq(
    JSContext* aCx, const nsAString& aApiMethod,
    const dom::Sequence<JS::Value>& aArgs, ErrorResult& aRv) {
  uint32_t argsCount = aArgs.Length();

  JS::Rooted<JS::Value> expectedVal(
      aCx, argsCount > 0 ? aArgs[0] : JS::UndefinedValue());
  JS::Rooted<JS::Value> actualVal(
      aCx, argsCount > 1 ? aArgs[1] : JS::UndefinedValue());
  JS::Rooted<JS::Value> messageVal(
      aCx, argsCount > 2 ? aArgs[2] : JS::UndefinedValue());

  bool isEqual;
  if (NS_WARN_IF(!JS::StrictlyEqual(aCx, actualVal, expectedVal, &isEqual))) {
    ThrowUnexpectedError(aCx, aRv);
    return;
  }

  JS::RootedString expectedJSString(aCx, JS::ToString(aCx, expectedVal));
  JS::RootedString actualJSString(aCx, JS::ToString(aCx, actualVal));
  JS::RootedString messageJSString(aCx, JS::ToString(aCx, messageVal));

  nsString expected;
  nsString actual;
  nsString message;

  if (NS_WARN_IF(!AssignJSString(aCx, expected, expectedJSString) ||
                 !AssignJSString(aCx, actual, actualJSString) ||
                 !AssignJSString(aCx, message, messageJSString))) {
    ThrowUnexpectedError(aCx, aRv);
    return;
  }

  if (!isEqual && actual.Equals(expected)) {
    actual.AppendLiteral(" (different)");
  }

  if (NS_WARN_IF(!dom::ToJSValue(aCx, expected, &expectedVal) ||
                 !dom::ToJSValue(aCx, actual, &actualVal) ||
                 !dom::ToJSValue(aCx, message, &messageVal))) {
    ThrowUnexpectedError(aCx, aRv);
    return;
  }

  dom::Sequence<JS::Value> args;
  if (NS_WARN_IF(!args.AppendElement(expectedVal, fallible) ||
                 !args.AppendElement(actualVal, fallible) ||
                 !args.AppendElement(messageVal, fallible))) {
    ThrowUnexpectedError(aCx, aRv);
    return;
  }

  CallWebExtMethodNoReturn(aCx, aApiMethod, args, aRv);
}

ExtensionEventManager* ExtensionTest::OnMessage() {
  if (!mOnMessageEventMgr) {
    mOnMessageEventMgr = CreateEventManager(u"onMessage"_ns);
  }

  return mOnMessageEventMgr;
}

}  // namespace extensions
}  // namespace mozilla

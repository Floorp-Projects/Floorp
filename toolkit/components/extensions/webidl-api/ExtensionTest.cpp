/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ExtensionTest.h"
#include "ExtensionEventManager.h"

#include "js/Equality.h"            // JS::StrictlyEqual
#include "js/PropertyAndElement.h"  // JS_GetProperty
#include "mozilla/dom/ExtensionTestBinding.h"
#include "nsIGlobalObject.h"
#include "js/RegExp.h"

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

MOZ_CAN_RUN_SCRIPT bool ExtensionTest::AssertMatchInternal(
    JSContext* aCx, const JS::HandleValue aActualValue,
    const JS::HandleValue aExpectedMatchValue, const nsAString& aMessagePre,
    const dom::Optional<nsAString>& aMessage,
    UniquePtr<dom::SerializedStackHolder> aSerializedCallerStack,
    ErrorResult& aRv) {
  // Stringify the actual value, if the expected value is a regexp or a string
  // then it will be used as part of the matching assertion, otherwise it is
  // still interpolated in the assertion message.
  JS::Rooted<JSString*> actualToString(aCx, JS::ToString(aCx, aActualValue));
  NS_ENSURE_TRUE(actualToString, false);
  nsAutoJSString actualString;
  NS_ENSURE_TRUE(actualString.init(aCx, actualToString), false);

  bool matched = false;

  if (aExpectedMatchValue.isObject()) {
    JS::RootedObject expectedMatchObj(aCx, &aExpectedMatchValue.toObject());

    bool isRegexp;
    NS_ENSURE_TRUE(JS::ObjectIsRegExp(aCx, expectedMatchObj, &isRegexp), false);

    if (isRegexp) {
      // Expected value is a regexp, test if the stringified actual value does
      // match.
      nsString input(actualString);
      size_t index = 0;
      JS::RootedValue rxResult(aCx);
      NS_ENSURE_TRUE(JS::ExecuteRegExpNoStatics(
                         aCx, expectedMatchObj, input.BeginWriting(),
                         actualString.Length(), &index, true, &rxResult),
                     false);
      matched = !rxResult.isNull();
    } else if (JS::IsCallable(expectedMatchObj) &&
               !JS::IsConstructor(expectedMatchObj)) {
      // Expected value is a matcher function, execute it with the value as a
      // parameter:
      //
      // - if the matcher function throws, steal the exception to re-raise it
      //   to the extension code that called the assertion method, but
      //   continue to still report the assertion as failed to the WebExtensions
      //   internals.
      //
      // - if the function return a falsey value, the assertion should fail and
      //   no exception is raised to the extension code that called the
      //   assertion
      JS::Rooted<JS::Value> retval(aCx);
      aRv.MightThrowJSException();
      if (!JS::Call(aCx, JS::UndefinedHandleValue, expectedMatchObj,
                    JS::HandleValueArray(aActualValue), &retval)) {
        aRv.StealExceptionFromJSContext(aCx);
        matched = false;
      } else {
        matched = JS::ToBoolean(retval);
      }
    } else if (JS::IsConstructor(expectedMatchObj)) {
      // Expected value is a constructor, test if the actual value is an
      // instanceof the expected constructor.
      NS_ENSURE_TRUE(
          JS::InstanceofOperator(aCx, expectedMatchObj, aActualValue, &matched),
          false);
    } else {
      // Fallback to strict equal for any other js object type we don't expect.
      NS_ENSURE_TRUE(
          JS::StrictlyEqual(aCx, aActualValue, aExpectedMatchValue, &matched),
          false);
    }
  } else if (aExpectedMatchValue.isString()) {
    // Expected value is a string, assertion should fail if the expected string
    // isn't equal to the stringified actual value.
    JS::Rooted<JSString*> expectedToString(
        aCx, JS::ToString(aCx, aExpectedMatchValue));
    NS_ENSURE_TRUE(expectedToString, false);

    nsAutoJSString expectedString;
    NS_ENSURE_TRUE(expectedString.init(aCx, expectedToString), false);

    // If actual is an object and it has a message property that is a string,
    // then we want to use that message string as the string to compare the
    // expected one with.
    //
    // This is needed mainly to match the current JS implementation.
    //
    // TODO(Bug 1731094): as a low priority follow up, we may want to reconsider
    // and compare the entire stringified error (which is also often a common
    // behavior in many third party JS test frameworks).
    JS::RootedValue messageVal(aCx);
    if (aActualValue.isObject()) {
      JS::Rooted<JSObject*> actualValueObj(aCx, &aActualValue.toObject());

      if (!JS_GetProperty(aCx, actualValueObj, "message", &messageVal)) {
        // GetProperty may raise an exception, in that case we steal the
        // exception to re-raise it to the caller, but continue to still report
        // the assertion as failed to the WebExtensions internals.
        aRv.StealExceptionFromJSContext(aCx);
        matched = false;
      }

      if (messageVal.isString()) {
        actualToString.set(messageVal.toString());
        NS_ENSURE_TRUE(actualString.init(aCx, actualToString), false);
      }
    }
    matched = expectedString.Equals(actualString);
  } else {
    // Fallback to strict equal for any other js value type we don't expect.
    NS_ENSURE_TRUE(
        JS::StrictlyEqual(aCx, aActualValue, aExpectedMatchValue, &matched),
        false);
  }

  // Convert the expected value to a source string, to be interpolated
  // in the assertion message.
  JS::Rooted<JSString*> expectedToSource(
      aCx, JS_ValueToSource(aCx, aExpectedMatchValue));
  NS_ENSURE_TRUE(expectedToSource, false);
  nsAutoJSString expectedSource;
  NS_ENSURE_TRUE(expectedSource.init(aCx, expectedToSource), false);

  nsString message;
  message.AppendPrintf("%s to match '%s', got '%s'",
                       NS_ConvertUTF16toUTF8(aMessagePre).get(),
                       NS_ConvertUTF16toUTF8(expectedSource).get(),
                       NS_ConvertUTF16toUTF8(actualString).get());
  if (aMessage.WasPassed()) {
    message.AppendPrintf(": %s", NS_ConvertUTF16toUTF8(aMessage.Value()).get());
  }

  // Complete the assertion by forwarding the boolean result and the
  // interpolated assertion message to the test.assertTrue API method on the
  // main thread.
  dom::Sequence<JS::Value> assertTrueArgs;
  JS::Rooted<JS::Value> arg0(aCx);
  JS::Rooted<JS::Value> arg1(aCx);
  NS_ENSURE_FALSE(!dom::ToJSValue(aCx, matched, &arg0) ||
                      !dom::ToJSValue(aCx, message, &arg1) ||
                      !assertTrueArgs.AppendElement(arg0, fallible) ||
                      !assertTrueArgs.AppendElement(arg1, fallible),
                  false);

  auto request = CallFunctionNoReturn(u"assertTrue"_ns);
  IgnoredErrorResult erv;
  if (aSerializedCallerStack) {
    request->SetSerializedCallerStack(std::move(aSerializedCallerStack));
  }
  request->Run(GetGlobalObject(), aCx, assertTrueArgs, erv);
  NS_ENSURE_FALSE(erv.Failed(), false);
  return true;
}

ExtensionEventManager* ExtensionTest::OnMessage() {
  if (!mOnMessageEventMgr) {
    mOnMessageEventMgr = CreateEventManager(u"onMessage"_ns);
  }

  return mOnMessageEventMgr;
}

}  // namespace extensions
}  // namespace mozilla

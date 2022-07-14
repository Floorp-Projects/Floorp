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

  JS::Rooted<JSString*> expectedJSString(aCx, JS::ToString(aCx, expectedVal));
  JS::Rooted<JSString*> actualJSString(aCx, JS::ToString(aCx, actualVal));
  JS::Rooted<JSString*> messageJSString(aCx, JS::ToString(aCx, messageVal));

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
    const nsAString& aMessage,
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
    JS::Rooted<JSObject*> expectedMatchObj(aCx,
                                           &aExpectedMatchValue.toObject());

    bool isRegexp;
    NS_ENSURE_TRUE(JS::ObjectIsRegExp(aCx, expectedMatchObj, &isRegexp), false);

    if (isRegexp) {
      // Expected value is a regexp, test if the stringified actual value does
      // match.
      nsString input(actualString);
      size_t index = 0;
      JS::Rooted<JS::Value> rxResult(aCx);
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
          JS_HasInstance(aCx, expectedMatchObj, aActualValue, &matched), false);
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
    JS::Rooted<JS::Value> messageVal(aCx);
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
  if (!aMessage.IsEmpty()) {
    message.AppendPrintf(": %s", NS_ConvertUTF16toUTF8(aMessage).get());
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

MOZ_CAN_RUN_SCRIPT void ExtensionTest::AssertThrows(
    JSContext* aCx, dom::Function& aFunction,
    const JS::HandleValue aExpectedError, const nsAString& aMessage,
    ErrorResult& aRv) {
  // Call the function that is expected to throw, then get the pending exception
  // to pass it to the AssertMatchInternal.
  ErrorResult erv;
  erv.MightThrowJSException();
  JS::Rooted<JS::Value> ignoredRetval(aCx);
  aFunction.Call({}, &ignoredRetval, erv, "ExtensionTest::AssertThrows",
                 dom::Function::eRethrowExceptions);

  bool didThrow = false;
  JS::Rooted<JS::Value> exn(aCx);

  if (erv.MaybeSetPendingException(aCx) && JS_GetPendingException(aCx, &exn)) {
    JS_ClearPendingException(aCx);
    didThrow = true;
  }

  // If the function did not throw, then the assertion is failed
  // and the result should be forwarded to assertTrue on the main thread.
  if (!didThrow) {
    JS::Rooted<JSString*> expectedErrorToSource(
        aCx, JS_ValueToSource(aCx, aExpectedError));
    if (NS_WARN_IF(!expectedErrorToSource)) {
      ThrowUnexpectedError(aCx, aRv);
      return;
    }
    nsAutoJSString expectedErrorSource;
    if (NS_WARN_IF(!expectedErrorSource.init(aCx, expectedErrorToSource))) {
      ThrowUnexpectedError(aCx, aRv);
      return;
    }

    nsString message;
    message.AppendPrintf("Function did not throw, expected error '%s'",
                         NS_ConvertUTF16toUTF8(expectedErrorSource).get());
    if (!aMessage.IsEmpty()) {
      message.AppendPrintf(": %s", NS_ConvertUTF16toUTF8(aMessage).get());
    }

    dom::Sequence<JS::Value> assertTrueArgs;
    JS::Rooted<JS::Value> arg0(aCx);
    JS::Rooted<JS::Value> arg1(aCx);
    if (NS_WARN_IF(!dom::ToJSValue(aCx, false, &arg0) ||
                   !dom::ToJSValue(aCx, message, &arg1) ||
                   !assertTrueArgs.AppendElement(arg0, fallible) ||
                   !assertTrueArgs.AppendElement(arg1, fallible))) {
      ThrowUnexpectedError(aCx, aRv);
      return;
    }

    CallWebExtMethodNoReturn(aCx, u"assertTrue"_ns, assertTrueArgs, aRv);
    if (NS_WARN_IF(aRv.Failed())) {
      ThrowUnexpectedError(aCx, aRv);
    }
    return;
  }

  if (NS_WARN_IF(!AssertMatchInternal(aCx, exn, aExpectedError,
                                      u"Function threw, expecting error"_ns,
                                      aMessage, nullptr, aRv))) {
    ThrowUnexpectedError(aCx, aRv);
  }
}

MOZ_CAN_RUN_SCRIPT void ExtensionTest::AssertThrows(
    JSContext* aCx, dom::Function& aFunction,
    const JS::HandleValue aExpectedError, ErrorResult& aRv) {
  AssertThrows(aCx, aFunction, aExpectedError, EmptyString(), aRv);
}

ExtensionEventManager* ExtensionTest::OnMessage() {
  if (!mOnMessageEventMgr) {
    mOnMessageEventMgr = CreateEventManager(u"onMessage"_ns);
  }

  return mOnMessageEventMgr;
}

#define ASSERT_REJECT_UNKNOWN_FAIL_STR "Failed to complete assertRejects call"

class AssertRejectsHandler final : public dom::PromiseNativeHandler {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(AssertRejectsHandler)

  static void Create(ExtensionTest* aExtensionTest, dom::Promise* aPromise,
                     dom::Promise* outPromise,
                     JS::Handle<JS::Value> aExpectedMatchValue,
                     const nsAString& aMessage,
                     UniquePtr<dom::SerializedStackHolder>&& aCallerStack) {
    MOZ_ASSERT(aPromise);
    MOZ_ASSERT(outPromise);
    MOZ_ASSERT(aExtensionTest);

    RefPtr<AssertRejectsHandler> handler = new AssertRejectsHandler(
        aExtensionTest, outPromise, aExpectedMatchValue, aMessage,
        std::move(aCallerStack));

    aPromise->AppendNativeHandler(handler);
  }

  MOZ_CAN_RUN_SCRIPT void ResolvedCallback(JSContext* aCx,
                                           JS::Handle<JS::Value> aValue,
                                           ErrorResult& aRv) override {
    nsAutoJSString expectedErrorSource;
    JS::Rooted<JS::Value> rootedExpectedMatchValue(aCx, mExpectedMatchValue);
    JS::Rooted<JSString*> expectedErrorToSource(
        aCx, JS_ValueToSource(aCx, rootedExpectedMatchValue));
    if (NS_WARN_IF(!expectedErrorToSource ||
                   !expectedErrorSource.init(aCx, expectedErrorToSource))) {
      mOutPromise->MaybeRejectWithUnknownError(ASSERT_REJECT_UNKNOWN_FAIL_STR);
      return;
    }

    nsString message;
    message.AppendPrintf("Promise resolved, expect rejection '%s'",
                         NS_ConvertUTF16toUTF8(expectedErrorSource).get());

    if (!mMessageStr.IsEmpty()) {
      message.AppendPrintf(": %s", NS_ConvertUTF16toUTF8(mMessageStr).get());
    }

    dom::Sequence<JS::Value> assertTrueArgs;
    JS::Rooted<JS::Value> arg0(aCx);
    JS::Rooted<JS::Value> arg1(aCx);
    if (NS_WARN_IF(!dom::ToJSValue(aCx, false, &arg0) ||
                   !dom::ToJSValue(aCx, message, &arg1) ||
                   !assertTrueArgs.AppendElement(arg0, fallible) ||
                   !assertTrueArgs.AppendElement(arg1, fallible))) {
      mOutPromise->MaybeRejectWithUnknownError(ASSERT_REJECT_UNKNOWN_FAIL_STR);
      return;
    }

    IgnoredErrorResult erv;
    auto request = mExtensionTest->CallFunctionNoReturn(u"assertTrue"_ns);
    request->SetSerializedCallerStack(std::move(mCallerStack));
    request->Run(mExtensionTest->GetGlobalObject(), aCx, assertTrueArgs, erv);
    if (NS_WARN_IF(erv.Failed())) {
      mOutPromise->MaybeRejectWithUnknownError(ASSERT_REJECT_UNKNOWN_FAIL_STR);
      return;
    }
    mOutPromise->MaybeResolve(JS::UndefinedValue());
  }

  MOZ_CAN_RUN_SCRIPT void RejectedCallback(JSContext* aCx,
                                           JS::Handle<JS::Value> aValue,
                                           ErrorResult& aRv) override {
    JS::Rooted<JS::Value> expectedMatchRooted(aCx, mExpectedMatchValue);
    ErrorResult erv;

    if (NS_WARN_IF(!MOZ_KnownLive(mExtensionTest)
                        ->AssertMatchInternal(
                            aCx, aValue, expectedMatchRooted,
                            u"Promise rejected, expected rejection"_ns,
                            mMessageStr, std::move(mCallerStack), erv))) {
      // Reject for other unknown errors.
      mOutPromise->MaybeRejectWithUnknownError(ASSERT_REJECT_UNKNOWN_FAIL_STR);
      return;
    }

    // Reject with the matcher function exception.
    erv.WouldReportJSException();
    if (erv.Failed()) {
      mOutPromise->MaybeReject(std::move(erv));
      return;
    }
    mExpectedMatchValue.setUndefined();
    mOutPromise->MaybeResolveWithUndefined();
  }

 private:
  AssertRejectsHandler(ExtensionTest* aExtensionTest, dom::Promise* mOutPromise,
                       JS::Handle<JS::Value> aExpectedMatchValue,
                       const nsAString& aMessage,
                       UniquePtr<dom::SerializedStackHolder>&& aCallerStack)
      : mOutPromise(mOutPromise), mExtensionTest(aExtensionTest) {
    MOZ_ASSERT(mOutPromise);
    MOZ_ASSERT(mExtensionTest);
    mozilla::HoldJSObjects(this);
    mExpectedMatchValue.set(aExpectedMatchValue);
    mCallerStack = std::move(aCallerStack);
    mMessageStr = aMessage;
  }

  ~AssertRejectsHandler() {
    mOutPromise = nullptr;
    mExtensionTest = nullptr;
    mExpectedMatchValue.setUndefined();
    mozilla::DropJSObjects(this);
  };

  RefPtr<dom::Promise> mOutPromise;
  RefPtr<ExtensionTest> mExtensionTest;
  JS::Heap<JS::Value> mExpectedMatchValue;
  UniquePtr<dom::SerializedStackHolder> mCallerStack;
  nsString mMessageStr;
};

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(AssertRejectsHandler)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION_CLASS(AssertRejectsHandler)

NS_IMPL_CYCLE_COLLECTING_ADDREF(AssertRejectsHandler)
NS_IMPL_CYCLE_COLLECTING_RELEASE(AssertRejectsHandler)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(AssertRejectsHandler)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mExtensionTest)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mOutPromise)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(AssertRejectsHandler)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mExpectedMatchValue)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(AssertRejectsHandler)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mExtensionTest)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mOutPromise)
  tmp->mExpectedMatchValue.setUndefined();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

void ExtensionTest::AssertRejects(
    JSContext* aCx, dom::Promise& aPromise,
    const JS::HandleValue aExpectedError, const nsAString& aMessage,
    const dom::Optional<OwningNonNull<dom::Function>>& aCallback,
    JS::MutableHandle<JS::Value> aRetval, ErrorResult& aRv) {
  auto* global = GetGlobalObject();

  IgnoredErrorResult erv;
  RefPtr<dom::Promise> outPromise = dom::Promise::Create(global, erv);
  if (NS_WARN_IF(erv.Failed())) {
    ThrowUnexpectedError(aCx, aRv);
    return;
  }
  MOZ_ASSERT(outPromise);

  AssertRejectsHandler::Create(this, &aPromise, outPromise, aExpectedError,
                               aMessage, dom::GetCurrentStack(aCx));

  if (aCallback.WasPassed()) {
    // In theory we could also support the callback-based behavior, but we
    // only use this in tests and so we don't really need to support it
    // for Chrome-compatibility reasons.
    aRv.ThrowNotSupportedError("assertRejects does not support a callback");
    return;
  }

  if (NS_WARN_IF(!ToJSValue(aCx, outPromise, aRetval))) {
    ThrowUnexpectedError(aCx, aRv);
    return;
  }
}

void ExtensionTest::AssertRejects(
    JSContext* aCx, dom::Promise& aPromise,
    const JS::HandleValue aExpectedError,
    const dom::Optional<OwningNonNull<dom::Function>>& aCallback,
    JS::MutableHandle<JS::Value> aRetval, ErrorResult& aRv) {
  AssertRejects(aCx, aPromise, aExpectedError, EmptyString(), aCallback,
                aRetval, aRv);
}

}  // namespace extensions
}  // namespace mozilla

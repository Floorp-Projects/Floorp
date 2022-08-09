/* -*- Mode: c++; c-basic-offset: 2; tab-width: 20; indent-tabs-mode: nil; -*-
 * vim: set sw=2 ts=4 expandtab:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EventDispatcher.h"

#include "JavaBuiltins.h"
#include "nsAppShell.h"
#include "nsJSUtils.h"
#include "js/Array.h"  // JS::GetArrayLength, JS::IsArrayObject, JS::NewArrayObject
#include "js/PropertyAndElement.h"  // JS_Enumerate, JS_GetElement, JS_GetProperty, JS_GetPropertyById, JS_SetElement, JS_SetUCProperty
#include "js/String.h"              // JS::StringHasLatin1Chars
#include "js/Warnings.h"            // JS::WarnUTF8
#include "xpcpublic.h"

#include "mozilla/fallible.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/java/EventCallbackWrappers.h"
#include "mozilla/jni/GeckoBundleUtils.h"

// Disable the C++ 2a warning. See bug #1509926
#if defined(__clang__)
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wc++2a-compat"
#endif

namespace mozilla {
namespace widget {

namespace detail {

bool CheckJS(JSContext* aCx, bool aResult) {
  if (!aResult) {
    JS_ClearPendingException(aCx);
  }
  return aResult;
}

nsresult BoxData(const nsAString& aEvent, JSContext* aCx,
                 JS::Handle<JS::Value> aData, jni::Object::LocalRef& aOut,
                 bool aObjectOnly) {
  nsresult rv = jni::BoxData(aCx, aData, aOut, aObjectOnly);
  if (rv != NS_ERROR_INVALID_ARG) {
    return rv;
  }

  NS_ConvertUTF16toUTF8 event(aEvent);
  if (JS_IsExceptionPending(aCx)) {
    JS::WarnUTF8(aCx, "Error dispatching %s", event.get());
  } else {
    JS_ReportErrorUTF8(aCx, "Invalid event data for %s", event.get());
  }
  return NS_ERROR_INVALID_ARG;
}

nsresult UnboxString(JSContext* aCx, const jni::Object::LocalRef& aData,
                     JS::MutableHandle<JS::Value> aOut) {
  if (!aData) {
    aOut.setNull();
    return NS_OK;
  }

  MOZ_ASSERT(aData.IsInstanceOf<jni::String>());

  JNIEnv* const env = aData.Env();
  const jstring jstr = jstring(aData.Get());
  const size_t len = env->GetStringLength(jstr);
  const jchar* const jchars = env->GetStringChars(jstr, nullptr);

  if (NS_WARN_IF(!jchars)) {
    env->ExceptionClear();
    return NS_ERROR_FAILURE;
  }

  auto releaseStr = MakeScopeExit([env, jstr, jchars] {
    env->ReleaseStringChars(jstr, jchars);
    env->ExceptionClear();
  });

  JS::Rooted<JSString*> str(
      aCx,
      JS_NewUCStringCopyN(aCx, reinterpret_cast<const char16_t*>(jchars), len));
  NS_ENSURE_TRUE(CheckJS(aCx, !!str), NS_ERROR_FAILURE);

  aOut.setString(str);
  return NS_OK;
}

nsresult UnboxValue(JSContext* aCx, const jni::Object::LocalRef& aData,
                    JS::MutableHandle<JS::Value> aOut);

nsresult UnboxBundle(JSContext* aCx, const jni::Object::LocalRef& aData,
                     JS::MutableHandle<JS::Value> aOut) {
  if (!aData) {
    aOut.setNull();
    return NS_OK;
  }

  MOZ_ASSERT(aData.IsInstanceOf<java::GeckoBundle>());

  JNIEnv* const env = aData.Env();
  const auto& bundle = java::GeckoBundle::Ref::From(aData);
  jni::ObjectArray::LocalRef keys = bundle->Keys();
  jni::ObjectArray::LocalRef values = bundle->Values();
  const size_t len = keys->Length();
  JS::Rooted<JSObject*> obj(aCx, JS_NewPlainObject(aCx));

  NS_ENSURE_TRUE(CheckJS(aCx, !!obj), NS_ERROR_FAILURE);
  NS_ENSURE_TRUE(values->Length() == len, NS_ERROR_FAILURE);

  for (size_t i = 0; i < len; i++) {
    jni::String::LocalRef key = keys->GetElement(i);
    const size_t keyLen = env->GetStringLength(key.Get());
    const jchar* const keyChars = env->GetStringChars(key.Get(), nullptr);
    if (NS_WARN_IF(!keyChars)) {
      env->ExceptionClear();
      return NS_ERROR_FAILURE;
    }

    auto releaseKeyChars = MakeScopeExit([env, &key, keyChars] {
      env->ReleaseStringChars(key.Get(), keyChars);
      env->ExceptionClear();
    });

    JS::Rooted<JS::Value> value(aCx);
    nsresult rv = UnboxValue(aCx, values->GetElement(i), &value);
    if (rv == NS_ERROR_INVALID_ARG && !JS_IsExceptionPending(aCx)) {
      JS_ReportErrorUTF8(
          aCx, u8"Invalid event data property %s",
          NS_ConvertUTF16toUTF8(
              nsString(reinterpret_cast<const char16_t*>(keyChars), keyLen))
              .get());
    }
    NS_ENSURE_SUCCESS(rv, rv);

    NS_ENSURE_TRUE(
        CheckJS(aCx, JS_SetUCProperty(
                         aCx, obj, reinterpret_cast<const char16_t*>(keyChars),
                         keyLen, value)),
        NS_ERROR_FAILURE);
  }

  aOut.setObject(*obj);
  return NS_OK;
}

template <typename Type, typename JNIType, typename ArrayType,
          JNIType* (JNIEnv::*GetElements)(ArrayType, jboolean*),
          void (JNIEnv::*ReleaseElements)(ArrayType, JNIType*, jint),
          JS::Value (*ToValue)(Type)>
nsresult UnboxArrayPrimitive(JSContext* aCx, const jni::Object::LocalRef& aData,
                             JS::MutableHandle<JS::Value> aOut) {
  JNIEnv* const env = aData.Env();
  const ArrayType jarray = ArrayType(aData.Get());
  JNIType* const array = (env->*GetElements)(jarray, nullptr);
  JS::RootedVector<JS::Value> elements(aCx);

  if (NS_WARN_IF(!array)) {
    env->ExceptionClear();
    return NS_ERROR_FAILURE;
  }

  auto releaseArray = MakeScopeExit([env, jarray, array] {
    (env->*ReleaseElements)(jarray, array, JNI_ABORT);
    env->ExceptionClear();
  });

  const size_t len = env->GetArrayLength(jarray);
  NS_ENSURE_TRUE(elements.initCapacity(len), NS_ERROR_FAILURE);

  for (size_t i = 0; i < len; i++) {
    NS_ENSURE_TRUE(elements.append((*ToValue)(Type(array[i]))),
                   NS_ERROR_FAILURE);
  }

  JS::Rooted<JSObject*> obj(
      aCx, JS::NewArrayObject(aCx, JS::HandleValueArray(elements)));
  NS_ENSURE_TRUE(CheckJS(aCx, !!obj), NS_ERROR_FAILURE);

  aOut.setObject(*obj);
  return NS_OK;
}

struct StringArray : jni::ObjectBase<StringArray> {
  static const char name[];
};

struct GeckoBundleArray : jni::ObjectBase<GeckoBundleArray> {
  static const char name[];
};

const char StringArray::name[] = "[Ljava/lang/String;";
const char GeckoBundleArray::name[] = "[Lorg/mozilla/gecko/util/GeckoBundle;";

template <nsresult (*Unbox)(JSContext*, const jni::Object::LocalRef&,
                            JS::MutableHandle<JS::Value>)>
nsresult UnboxArrayObject(JSContext* aCx, const jni::Object::LocalRef& aData,
                          JS::MutableHandle<JS::Value> aOut) {
  jni::ObjectArray::LocalRef array(aData.Env(),
                                   jni::ObjectArray::Ref::From(aData));
  const size_t len = array->Length();
  JS::Rooted<JSObject*> obj(aCx, JS::NewArrayObject(aCx, len));
  NS_ENSURE_TRUE(CheckJS(aCx, !!obj), NS_ERROR_FAILURE);

  for (size_t i = 0; i < len; i++) {
    jni::Object::LocalRef element = array->GetElement(i);
    JS::Rooted<JS::Value> value(aCx);
    nsresult rv = (*Unbox)(aCx, element, &value);
    NS_ENSURE_SUCCESS(rv, rv);

    NS_ENSURE_TRUE(CheckJS(aCx, JS_SetElement(aCx, obj, i, value)),
                   NS_ERROR_FAILURE);
  }

  aOut.setObject(*obj);
  return NS_OK;
}

nsresult UnboxValue(JSContext* aCx, const jni::Object::LocalRef& aData,
                    JS::MutableHandle<JS::Value> aOut) {
  using jni::Java2Native;

  if (!aData) {
    aOut.setNull();
  } else if (aData.IsInstanceOf<jni::Boolean>()) {
    aOut.setBoolean(Java2Native<bool>(aData, aData.Env()));
  } else if (aData.IsInstanceOf<jni::Integer>()) {
    aOut.setInt32(Java2Native<int>(aData, aData.Env()));
  } else if (aData.IsInstanceOf<jni::Byte>() ||
             aData.IsInstanceOf<jni::Short>()) {
    aOut.setInt32(java::sdk::Number::Ref::From(aData)->IntValue());
  } else if (aData.IsInstanceOf<jni::Double>()) {
    aOut.setNumber(Java2Native<double>(aData, aData.Env()));
  } else if (aData.IsInstanceOf<jni::Float>() ||
             aData.IsInstanceOf<jni::Long>()) {
    aOut.setNumber(java::sdk::Number::Ref::From(aData)->DoubleValue());
  } else if (aData.IsInstanceOf<jni::String>()) {
    return UnboxString(aCx, aData, aOut);
  } else if (aData.IsInstanceOf<jni::Character>()) {
    return UnboxString(aCx, java::sdk::String::ValueOf(aData), aOut);
  } else if (aData.IsInstanceOf<java::GeckoBundle>()) {
    return UnboxBundle(aCx, aData, aOut);

  } else if (aData.IsInstanceOf<jni::BooleanArray>()) {
    return UnboxArrayPrimitive<
        bool, jboolean, jbooleanArray, &JNIEnv::GetBooleanArrayElements,
        &JNIEnv::ReleaseBooleanArrayElements, &JS::BooleanValue>(aCx, aData,
                                                                 aOut);

  } else if (aData.IsInstanceOf<jni::IntArray>()) {
    return UnboxArrayPrimitive<
        int32_t, jint, jintArray, &JNIEnv::GetIntArrayElements,
        &JNIEnv::ReleaseIntArrayElements, &JS::Int32Value>(aCx, aData, aOut);

  } else if (aData.IsInstanceOf<jni::DoubleArray>()) {
    return UnboxArrayPrimitive<
        double, jdouble, jdoubleArray, &JNIEnv::GetDoubleArrayElements,
        &JNIEnv::ReleaseDoubleArrayElements, &JS::DoubleValue>(aCx, aData,
                                                               aOut);

  } else if (aData.IsInstanceOf<StringArray>()) {
    return UnboxArrayObject<&UnboxString>(aCx, aData, aOut);
  } else if (aData.IsInstanceOf<GeckoBundleArray>()) {
    return UnboxArrayObject<&UnboxBundle>(aCx, aData, aOut);
  } else {
    NS_WARNING("Invalid type");
    return NS_ERROR_INVALID_ARG;
  }
  return NS_OK;
}

nsresult UnboxData(jni::String::Param aEvent, JSContext* aCx,
                   jni::Object::Param aData, JS::MutableHandle<JS::Value> aOut,
                   bool aBundleOnly) {
  MOZ_ASSERT(NS_IsMainThread());

  jni::Object::LocalRef jniData(jni::GetGeckoThreadEnv(), aData);
  nsresult rv = NS_ERROR_INVALID_ARG;

  if (!aBundleOnly) {
    rv = UnboxValue(aCx, jniData, aOut);
  } else if (!jniData || jniData.IsInstanceOf<java::GeckoBundle>()) {
    rv = UnboxBundle(aCx, jniData, aOut);
  }
  if (rv != NS_ERROR_INVALID_ARG || !aEvent) {
    return rv;
  }

  nsCString event = aEvent->ToCString();
  if (JS_IsExceptionPending(aCx)) {
    JS::WarnUTF8(aCx, "Error dispatching %s", event.get());
  } else {
    JS_ReportErrorUTF8(aCx, "Invalid event data for %s", event.get());
  }
  return NS_ERROR_INVALID_ARG;
}

class JavaCallbackDelegate final : public nsIAndroidEventCallback {
  const java::EventCallback::GlobalRef mCallback;

  virtual ~JavaCallbackDelegate() {}

  NS_IMETHOD Call(JSContext* aCx, JS::Handle<JS::Value> aData,
                  void (java::EventCallback::*aCall)(jni::Object::Param)
                      const) {
    MOZ_ASSERT(NS_IsMainThread());

    jni::Object::LocalRef data(jni::GetGeckoThreadEnv());
    nsresult rv = BoxData(u"callback"_ns, aCx, aData, data,
                          /* ObjectOnly */ false);
    NS_ENSURE_SUCCESS(rv, rv);

    dom::AutoNoJSAPI nojsapi;

    (java::EventCallback(*mCallback).*aCall)(data);
    return NS_OK;
  }

 public:
  explicit JavaCallbackDelegate(java::EventCallback::Param aCallback)
      : mCallback(jni::GetGeckoThreadEnv(), aCallback) {}

  NS_DECL_ISUPPORTS

  NS_IMETHOD OnSuccess(JS::Handle<JS::Value> aData, JSContext* aCx) override {
    return Call(aCx, aData, &java::EventCallback::SendSuccess);
  }

  NS_IMETHOD OnError(JS::Handle<JS::Value> aData, JSContext* aCx) override {
    return Call(aCx, aData, &java::EventCallback::SendError);
  }
};

NS_IMPL_ISUPPORTS(JavaCallbackDelegate, nsIAndroidEventCallback)

class NativeCallbackDelegateSupport final
    : public java::EventDispatcher::NativeCallbackDelegate ::Natives<
          NativeCallbackDelegateSupport> {
  using CallbackDelegate = java::EventDispatcher::NativeCallbackDelegate;
  using Base = CallbackDelegate::Natives<NativeCallbackDelegateSupport>;

  const nsCOMPtr<nsIAndroidEventCallback> mCallback;
  const nsCOMPtr<nsIAndroidEventFinalizer> mFinalizer;
  const nsCOMPtr<nsIGlobalObject> mGlobalObject;

  void Call(jni::Object::Param aData,
            nsresult (nsIAndroidEventCallback::*aCall)(JS::Handle<JS::Value>,
                                                       JSContext*)) {
    MOZ_ASSERT(NS_IsMainThread());

    // Use either the attached window's realm or a default realm.

    dom::AutoJSAPI jsapi;
    NS_ENSURE_TRUE_VOID(jsapi.Init(mGlobalObject));

    JS::Rooted<JS::Value> data(jsapi.cx());
    nsresult rv = UnboxData(u"callback"_ns, jsapi.cx(), aData, &data,
                            /* BundleOnly */ false);
    NS_ENSURE_SUCCESS_VOID(rv);

    rv = (mCallback->*aCall)(data, jsapi.cx());
    NS_ENSURE_SUCCESS_VOID(rv);
  }

 public:
  using Base::AttachNative;

  template <typename Functor>
  static void OnNativeCall(Functor&& aCall) {
    if (NS_IsMainThread()) {
      // Invoke callbacks synchronously if we're already on Gecko thread.
      return aCall();
    }
    NS_DispatchToMainThread(
        NS_NewRunnableFunction("OnNativeCall", std::move(aCall)));
  }

  static void Finalize(const CallbackDelegate::LocalRef& aInstance) {
    DisposeNative(aInstance);
  }

  NativeCallbackDelegateSupport(nsIAndroidEventCallback* callback,
                                nsIAndroidEventFinalizer* finalizer,
                                nsIGlobalObject* globalObject)
      : mCallback(callback),
        mFinalizer(finalizer),
        mGlobalObject(globalObject) {}

  ~NativeCallbackDelegateSupport() {
    if (mFinalizer) {
      mFinalizer->OnFinalize();
    }
  }

  void SendSuccess(jni::Object::Param aData) {
    Call(aData, &nsIAndroidEventCallback::OnSuccess);
  }

  void SendError(jni::Object::Param aData) {
    Call(aData, &nsIAndroidEventCallback::OnError);
  }
};

class FinalizingCallbackDelegate final : public nsIAndroidEventCallback {
  const nsCOMPtr<nsIAndroidEventCallback> mCallback;
  const nsCOMPtr<nsIAndroidEventFinalizer> mFinalizer;

  virtual ~FinalizingCallbackDelegate() {
    if (mFinalizer) {
      mFinalizer->OnFinalize();
    }
  }

 public:
  FinalizingCallbackDelegate(nsIAndroidEventCallback* aCallback,
                             nsIAndroidEventFinalizer* aFinalizer)
      : mCallback(aCallback), mFinalizer(aFinalizer) {}

  NS_DECL_ISUPPORTS
  NS_FORWARD_NSIANDROIDEVENTCALLBACK(mCallback->);
};

NS_IMPL_ISUPPORTS(FinalizingCallbackDelegate, nsIAndroidEventCallback)

}  // namespace detail

using namespace detail;

NS_IMPL_ISUPPORTS(EventDispatcher, nsIAndroidEventDispatcher)

nsIGlobalObject* EventDispatcher::GetGlobalObject() {
  if (mDOMWindow) {
    return nsGlobalWindowInner::Cast(mDOMWindow->GetCurrentInnerWindow());
  }
  return xpc::NativeGlobal(xpc::PrivilegedJunkScope());
}

nsresult EventDispatcher::DispatchOnGecko(ListenersList* list,
                                          const nsAString& aEvent,
                                          JS::Handle<JS::Value> aData,
                                          nsIAndroidEventCallback* aCallback) {
  MOZ_ASSERT(NS_IsMainThread());
  dom::AutoNoJSAPI nojsapi;

  list->lockCount++;

  auto iteratingScope = MakeScopeExit([list] {
    list->lockCount--;
    if (list->lockCount || !list->unregistering) {
      return;
    }

    list->unregistering = false;
    for (ssize_t i = list->listeners.Count() - 1; i >= 0; i--) {
      if (list->listeners[i]) {
        continue;
      }
      list->listeners.RemoveObjectAt(i);
    }
  });

  const size_t count = list->listeners.Count();
  for (size_t i = 0; i < count; i++) {
    if (!list->listeners[i]) {
      // Unregistered.
      continue;
    }
    const nsresult rv = list->listeners[i]->OnEvent(aEvent, aData, aCallback);
    Unused << NS_WARN_IF(NS_FAILED(rv));
  }
  return NS_OK;
}

java::EventDispatcher::NativeCallbackDelegate::LocalRef
EventDispatcher::WrapCallback(nsIAndroidEventCallback* aCallback,
                              nsIAndroidEventFinalizer* aFinalizer) {
  if (!aCallback) {
    return java::EventDispatcher::NativeCallbackDelegate::LocalRef(
        jni::GetGeckoThreadEnv());
  }

  java::EventDispatcher::NativeCallbackDelegate::LocalRef callback =
      java::EventDispatcher::NativeCallbackDelegate::New();
  NativeCallbackDelegateSupport::AttachNative(
      callback, MakeUnique<NativeCallbackDelegateSupport>(aCallback, aFinalizer,
                                                          GetGlobalObject()));
  return callback;
}

bool EventDispatcher::HasListener(const char16_t* aEvent) {
  java::EventDispatcher::LocalRef dispatcher(mDispatcher);
  if (!dispatcher) {
    return false;
  }

  nsDependentString event(aEvent);
  return dispatcher->HasListener(event);
}

NS_IMETHODIMP
EventDispatcher::Dispatch(JS::Handle<JS::Value> aEvent,
                          JS::Handle<JS::Value> aData,
                          nsIAndroidEventCallback* aCallback,
                          nsIAndroidEventFinalizer* aFinalizer,
                          JSContext* aCx) {
  MOZ_ASSERT(NS_IsMainThread());

  if (!aEvent.isString()) {
    NS_WARNING("Invalid event name");
    return NS_ERROR_INVALID_ARG;
  }

  nsAutoJSString event;
  NS_ENSURE_TRUE(CheckJS(aCx, event.init(aCx, aEvent.toString())),
                 NS_ERROR_OUT_OF_MEMORY);

  // Don't need to lock here because we're on the main thread, and we can't
  // race against Register/UnregisterListener.

  ListenersList* list = mListenersMap.Get(event);
  if (list) {
    if (!aCallback || !aFinalizer) {
      return DispatchOnGecko(list, event, aData, aCallback);
    }
    nsCOMPtr<nsIAndroidEventCallback> callback(
        new FinalizingCallbackDelegate(aCallback, aFinalizer));
    return DispatchOnGecko(list, event, aData, callback);
  }

  java::EventDispatcher::LocalRef dispatcher(mDispatcher);
  if (!dispatcher) {
    return NS_OK;
  }

  jni::Object::LocalRef data(jni::GetGeckoThreadEnv());
  nsresult rv = BoxData(event, aCx, aData, data, /* ObjectOnly */ true);
  // Keep XPConnect from overriding the JSContext exception with one
  // based on the nsresult.
  //
  // XXXbz Does xpconnect still do that?  Needs to be checked/tested.
  NS_ENSURE_SUCCESS(rv, JS_IsExceptionPending(aCx) ? NS_OK : rv);

  dom::AutoNoJSAPI nojsapi;
  dispatcher->DispatchToThreads(event, data,
                                WrapCallback(aCallback, aFinalizer));
  return NS_OK;
}

nsresult EventDispatcher::Dispatch(const char16_t* aEvent,
                                   java::GeckoBundle::Param aData,
                                   nsIAndroidEventCallback* aCallback) {
  nsDependentString event(aEvent);

  ListenersList* list = mListenersMap.Get(event);
  if (list) {
    dom::AutoJSAPI jsapi;
    NS_ENSURE_TRUE(jsapi.Init(GetGlobalObject()), NS_ERROR_FAILURE);
    JS::Rooted<JS::Value> data(jsapi.cx());
    nsresult rv = UnboxData(/* Event */ nullptr, jsapi.cx(), aData, &data,
                            /* BundleOnly */ true);
    NS_ENSURE_SUCCESS(rv, rv);
    return DispatchOnGecko(list, event, data, aCallback);
  }

  java::EventDispatcher::LocalRef dispatcher(mDispatcher);
  if (!dispatcher) {
    return NS_OK;
  }

  dispatcher->DispatchToThreads(event, aData, WrapCallback(aCallback));
  return NS_OK;
}

nsresult EventDispatcher::IterateEvents(JSContext* aCx,
                                        JS::Handle<JS::Value> aEvents,
                                        IterateEventsCallback aCallback,
                                        nsIAndroidEventListener* aListener) {
  MOZ_ASSERT(NS_IsMainThread());

  MutexAutoLock lock(mLock);

  auto processEvent = [this, aCx, aCallback,
                       aListener](JS::Handle<JS::Value> event) -> nsresult {
    nsAutoJSString str;
    NS_ENSURE_TRUE(CheckJS(aCx, str.init(aCx, event.toString())),
                   NS_ERROR_OUT_OF_MEMORY);
    return (this->*aCallback)(str, aListener);
  };

  if (aEvents.isString()) {
    return processEvent(aEvents);
  }

  bool isArray = false;
  NS_ENSURE_TRUE(aEvents.isObject(), NS_ERROR_INVALID_ARG);
  NS_ENSURE_TRUE(CheckJS(aCx, JS::IsArrayObject(aCx, aEvents, &isArray)),
                 NS_ERROR_INVALID_ARG);
  NS_ENSURE_TRUE(isArray, NS_ERROR_INVALID_ARG);

  JS::Rooted<JSObject*> events(aCx, &aEvents.toObject());
  uint32_t length = 0;
  NS_ENSURE_TRUE(CheckJS(aCx, JS::GetArrayLength(aCx, events, &length)),
                 NS_ERROR_INVALID_ARG);
  NS_ENSURE_TRUE(length, NS_ERROR_INVALID_ARG);

  for (size_t i = 0; i < length; i++) {
    JS::Rooted<JS::Value> event(aCx);
    NS_ENSURE_TRUE(CheckJS(aCx, JS_GetElement(aCx, events, i, &event)),
                   NS_ERROR_INVALID_ARG);
    NS_ENSURE_TRUE(event.isString(), NS_ERROR_INVALID_ARG);

    const nsresult rv = processEvent(event);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  return NS_OK;
}

nsresult EventDispatcher::RegisterEventLocked(
    const nsAString& aEvent, nsIAndroidEventListener* aListener) {
  ListenersList* list = mListenersMap.GetOrInsertNew(aEvent);

#ifdef DEBUG
  for (ssize_t i = 0; i < list->listeners.Count(); i++) {
    NS_ENSURE_TRUE(list->listeners[i] != aListener,
                   NS_ERROR_ALREADY_INITIALIZED);
  }
#endif

  list->listeners.AppendObject(aListener);
  return NS_OK;
}

NS_IMETHODIMP
EventDispatcher::RegisterListener(nsIAndroidEventListener* aListener,
                                  JS::Handle<JS::Value> aEvents,
                                  JSContext* aCx) {
  return IterateEvents(aCx, aEvents, &EventDispatcher::RegisterEventLocked,
                       aListener);
}

nsresult EventDispatcher::UnregisterEventLocked(
    const nsAString& aEvent, nsIAndroidEventListener* aListener) {
  ListenersList* list = mListenersMap.Get(aEvent);
#ifdef DEBUG
  NS_ENSURE_TRUE(list, NS_ERROR_NOT_INITIALIZED);
#else
  NS_ENSURE_TRUE(list, NS_OK);
#endif

  DebugOnly<bool> found = false;
  for (ssize_t i = list->listeners.Count() - 1; i >= 0; i--) {
    if (list->listeners[i] != aListener) {
      continue;
    }
    if (list->lockCount) {
      // Only mark for removal when list is locked.
      list->listeners.ReplaceObjectAt(nullptr, i);
      list->unregistering = true;
    } else {
      list->listeners.RemoveObjectAt(i);
    }
    found = true;
  }
#ifdef DEBUG
  return found ? NS_OK : NS_ERROR_NOT_INITIALIZED;
#else
  return NS_OK;
#endif
}

NS_IMETHODIMP
EventDispatcher::UnregisterListener(nsIAndroidEventListener* aListener,
                                    JS::Handle<JS::Value> aEvents,
                                    JSContext* aCx) {
  return IterateEvents(aCx, aEvents, &EventDispatcher::UnregisterEventLocked,
                       aListener);
}

void EventDispatcher::Attach(java::EventDispatcher::Param aDispatcher,
                             nsPIDOMWindowOuter* aDOMWindow) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aDispatcher);

  java::EventDispatcher::LocalRef dispatcher(mDispatcher);

  if (dispatcher) {
    if (dispatcher == aDispatcher) {
      // Only need to update the window.
      mDOMWindow = aDOMWindow;
      return;
    }
    dispatcher->SetAttachedToGecko(java::EventDispatcher::REATTACHING);
  }

  dispatcher = java::EventDispatcher::LocalRef(aDispatcher);
  NativesBase::AttachNative(dispatcher, this);
  mDispatcher = dispatcher;
  mDOMWindow = aDOMWindow;

  dispatcher->SetAttachedToGecko(java::EventDispatcher::ATTACHED);
}

void EventDispatcher::Shutdown() {
  mDispatcher = nullptr;
  mDOMWindow = nullptr;
}

void EventDispatcher::Detach() {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mDispatcher);

  java::EventDispatcher::GlobalRef dispatcher(mDispatcher);

  // SetAttachedToGecko will call disposeNative for us later on the Gecko
  // thread to make sure all pending dispatchToGecko calls have completed.
  if (dispatcher) {
    dispatcher->SetAttachedToGecko(java::EventDispatcher::DETACHED);
  }

  Shutdown();
}

bool EventDispatcher::HasGeckoListener(jni::String::Param aEvent) {
  // Can be called from any thread.
  MutexAutoLock lock(mLock);
  return !!mListenersMap.Get(aEvent->ToString());
}

void EventDispatcher::DispatchToGecko(jni::String::Param aEvent,
                                      jni::Object::Param aData,
                                      jni::Object::Param aCallback) {
  MOZ_ASSERT(NS_IsMainThread());

  // Don't need to lock here because we're on the main thread, and we can't
  // race against Register/UnregisterListener.

  nsString event = aEvent->ToString();
  ListenersList* list = mListenersMap.Get(event);
  if (!list || list->listeners.IsEmpty()) {
    return;
  }

  // Use the same compartment as the attached window if possible, otherwise
  // use a default compartment.
  dom::AutoJSAPI jsapi;
  NS_ENSURE_TRUE_VOID(jsapi.Init(GetGlobalObject()));

  JS::Rooted<JS::Value> data(jsapi.cx());
  nsresult rv = UnboxData(aEvent, jsapi.cx(), aData, &data,
                          /* BundleOnly */ true);
  NS_ENSURE_SUCCESS_VOID(rv);

  nsCOMPtr<nsIAndroidEventCallback> callback;
  if (aCallback) {
    callback =
        new JavaCallbackDelegate(java::EventCallback::Ref::From(aCallback));
  }

  DispatchOnGecko(list, event, data, callback);
}

/* static */
nsresult EventDispatcher::UnboxBundle(JSContext* aCx, jni::Object::Param aData,
                                      JS::MutableHandle<JS::Value> aOut) {
  return detail::UnboxBundle(aCx, aData, aOut);
}

}  // namespace widget
}  // namespace mozilla

#if defined(__clang__)
#  pragma clang diagnostic pop
#endif

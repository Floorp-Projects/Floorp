/* -*- Mode: c++; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * vim: set sw=4 ts=4 expandtab:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EventDispatcher.h"

#include "nsAppShell.h"
#include "nsIXPConnect.h"
#include "nsJSUtils.h"
#include "xpcpublic.h"

#include "mozilla/ScopeExit.h"
#include "mozilla/dom/ScriptSettings.h"

namespace mozilla {
namespace widget {

namespace detail {

bool
CheckJS(JSContext* aCx, bool aResult)
{
    if (!aResult) {
        JS_ClearPendingException(aCx);
    }
    return aResult;
}

nsresult
BoxString(JSContext* aCx, JS::HandleValue aData, jni::Object::LocalRef& aOut)
{
    if (aData.isNullOrUndefined()) {
        aOut = nullptr;
        return NS_OK;
    }

    MOZ_ASSERT(aData.isString());

    JS::RootedString str(aCx, aData.toString());

    if (JS_StringHasLatin1Chars(str)) {
        nsAutoJSString autoStr;
        NS_ENSURE_TRUE(CheckJS(aCx, autoStr.init(aCx, str)), NS_ERROR_FAILURE);

        // StringParam can automatically convert a nsString to jstring.
        aOut = jni::StringParam(autoStr, aOut.Env());
        return NS_OK;
    }

    // Two-byte string
    JNIEnv* const env = aOut.Env();
    const char16_t* chars;
    {
        JS::AutoCheckCannotGC nogc;
        size_t len = 0;
        chars = JS_GetTwoByteStringCharsAndLength(aCx, nogc, str, &len);
        if (chars) {
            aOut = jni::String::LocalRef::Adopt(env,
                    env->NewString(reinterpret_cast<const jchar*>(chars), len));
        }
    }
    if (NS_WARN_IF(!CheckJS(aCx, !!chars) || !aOut)) {
        env->ExceptionClear();
        return NS_ERROR_FAILURE;
    }
    return NS_OK;
}

nsresult
BoxObject(JSContext* aCx, JS::HandleValue aData, jni::Object::LocalRef& aOut);

template<typename Type,
         bool (JS::Value::*IsType)() const,
         Type (JS::Value::*ToType)() const,
         class ArrayType,
         typename ArrayType::LocalRef (*NewArray)(const Type*, size_t)> nsresult
BoxArrayPrimitive(JSContext* aCx, JS::HandleObject aData,
                  jni::Object::LocalRef& aOut, size_t aLength,
                  JS::HandleValue aElement)
{
    JS::RootedValue element(aCx);
    auto data = MakeUnique<Type[]>(aLength);
    data[0] = (aElement.get().*ToType)();

    for (size_t i = 1; i < aLength; i++) {
        NS_ENSURE_TRUE(CheckJS(aCx, JS_GetElement(aCx, aData, i, &element)),
                       NS_ERROR_FAILURE);
        NS_ENSURE_TRUE((element.get().*IsType)(), NS_ERROR_INVALID_ARG);

        data[i] = (element.get().*ToType)();
    }
    aOut = (*NewArray)(data.get(), aLength);
    return NS_OK;
}

template<class Type,
         nsresult (*Box)(JSContext*, JS::HandleValue, jni::Object::LocalRef&),
         typename IsType>
nsresult
BoxArrayObject(JSContext* aCx, JS::HandleObject aData,
               jni::Object::LocalRef& aOut, size_t aLength,
               JS::HandleValue aElement,
               IsType&& aIsType)
{
    auto out = jni::ObjectArray::New<Type>(aLength);
    JS::RootedValue element(aCx);
    jni::Object::LocalRef jniElement(aOut.Env());

    nsresult rv = (*Box)(aCx, aElement, jniElement);
    NS_ENSURE_SUCCESS(rv, rv);
    out->SetElement(0, jniElement);

    for (size_t i = 1; i < aLength; i++) {
        NS_ENSURE_TRUE(CheckJS(aCx, JS_GetElement(aCx, aData, i, &element)),
                       NS_ERROR_FAILURE);
        NS_ENSURE_TRUE(element.isNullOrUndefined() || aIsType(element),
                       NS_ERROR_INVALID_ARG);

        rv = (*Box)(aCx, element, jniElement);
        NS_ENSURE_SUCCESS(rv, rv);
        out->SetElement(i, jniElement);
    }
    aOut = out;
    return NS_OK;
}

nsresult
BoxArray(JSContext* aCx, JS::HandleObject aData, jni::Object::LocalRef& aOut)
{
    uint32_t length = 0;
    NS_ENSURE_TRUE(CheckJS(aCx, JS_GetArrayLength(aCx, aData, &length)),
                   NS_ERROR_FAILURE);

    if (!length) {
        // Always represent empty arrays as an empty boolean array.
        aOut = java::GeckoBundle::EMPTY_BOOLEAN_ARRAY();
        return NS_OK;
    }

    // We only check the first element's type. If the array has mixed types,
    // we'll throw an error during actual conversion.
    JS::RootedValue element(aCx);
    NS_ENSURE_TRUE(CheckJS(aCx, JS_GetElement(aCx, aData, 0, &element)),
                   NS_ERROR_FAILURE);

    if (element.isBoolean()) {
        return BoxArrayPrimitive<bool, &JS::Value::isBoolean, &JS::Value::toBoolean,
                                 jni::BooleanArray, &jni::BooleanArray::New>(
                aCx, aData, aOut, length, element);
    }

    if (element.isInt32()) {
        nsresult rv = BoxArrayPrimitive<
                int32_t, &JS::Value::isInt32, &JS::Value::toInt32,
                jni::IntArray, &jni::IntArray::New>(aCx, aData, aOut,
                                                    length, element);
        if (rv != NS_ERROR_INVALID_ARG) {
            return rv;
        }
        // Not int32, but we can still try a double array.
    }

    if (element.isNumber()) {
        return BoxArrayPrimitive<
                double, &JS::Value::isNumber, &JS::Value::toNumber,
                jni::DoubleArray, &jni::DoubleArray::New>(aCx, aData, aOut,
                                                          length, element);
    }

    if (element.isNullOrUndefined() || element.isString()) {
        const auto isString = [] (JS::HandleValue val) -> bool {
            return val.isString();
        };
        nsresult rv = BoxArrayObject<jni::String, &BoxString>(
                aCx, aData, aOut, length, element, isString);
        if (element.isString() || rv != NS_ERROR_INVALID_ARG) {
            return rv;
        }
        // First element was null/undefined, so it may still be an object array.
    }

    const auto isObject = [aCx] (JS::HandleValue val) -> bool {
        if (!val.isObject()) {
            return false;
        }
        bool array = false;
        JS::RootedObject obj(aCx, &val.toObject());
        // We don't support array of arrays.
        return CheckJS(aCx, JS_IsArrayObject(aCx, obj, &array)) && !array;
    };

    if (element.isNullOrUndefined() || isObject(element)) {
        return BoxArrayObject<java::GeckoBundle, &BoxObject>(
                aCx, aData, aOut, length, element, isObject);
    }

    NS_WARNING("Unknown type");
    return NS_ERROR_INVALID_ARG;
}

nsresult
BoxValue(JSContext* aCx, JS::HandleValue aData, jni::Object::LocalRef& aOut);

nsresult
BoxObject(JSContext* aCx, JS::HandleValue aData, jni::Object::LocalRef& aOut)
{
    if (aData.isNullOrUndefined()) {
        aOut = nullptr;
        return NS_OK;
    }

    MOZ_ASSERT(aData.isObject());

    JS::Rooted<JS::IdVector> ids(aCx, JS::IdVector(aCx));
    JS::RootedObject obj(aCx, &aData.toObject());

    bool isArray = false;
    if (CheckJS(aCx, JS_IsArrayObject(aCx, obj, &isArray)) && isArray) {
        return BoxArray(aCx, obj, aOut);
    }

    NS_ENSURE_TRUE(CheckJS(aCx, JS_Enumerate(aCx, obj, &ids)),
                   NS_ERROR_FAILURE);

    const size_t length = ids.length();
    auto keys = jni::ObjectArray::New<jni::String>(length);
    auto values = jni::ObjectArray::New<jni::Object>(length);

    // Iterate through each property of the JS object.
    for (size_t i = 0; i < ids.length(); i++) {
        const JS::RootedId id(aCx, ids[i]);
        JS::RootedValue idVal(aCx);
        JS::RootedValue val(aCx);
        jni::Object::LocalRef key(aOut.Env());
        jni::Object::LocalRef value(aOut.Env());

        NS_ENSURE_TRUE(CheckJS(aCx, JS_IdToValue(aCx, id, &idVal)),
                       NS_ERROR_FAILURE);

        JS::RootedString idStr(aCx, JS::ToString(aCx, idVal));
        NS_ENSURE_TRUE(CheckJS(aCx, !!idStr), NS_ERROR_FAILURE);

        idVal.setString(idStr);
        NS_ENSURE_SUCCESS(BoxString(aCx, idVal, key), NS_ERROR_FAILURE);
        NS_ENSURE_TRUE(CheckJS(aCx, JS_GetPropertyById(aCx, obj, id, &val)),
                       NS_ERROR_FAILURE);

        nsresult rv = BoxValue(aCx, val, value);
        if (rv == NS_ERROR_INVALID_ARG && !JS_IsExceptionPending(aCx)) {
            nsAutoJSString autoStr;
            if (CheckJS(aCx, autoStr.init(aCx, idVal.toString()))) {
                JS_ReportErrorUTF8(
                        aCx, u8"Invalid event data property %s",
                        NS_ConvertUTF16toUTF8(autoStr).get());
            }
        }
        NS_ENSURE_SUCCESS(rv, rv);

        keys->SetElement(i, key);
        values->SetElement(i, value);
    }

    aOut = java::GeckoBundle::New(keys, values);
    return NS_OK;
}

nsresult
BoxValue(JSContext* aCx, JS::HandleValue aData, jni::Object::LocalRef& aOut)
{
    if (aData.isNullOrUndefined()) {
        aOut = nullptr;
    } else if (aData.isBoolean()) {
        aOut = java::GeckoBundle::Box(aData.toBoolean());
    } else if (aData.isInt32()) {
        aOut = java::GeckoBundle::Box(aData.toInt32());
    } else if (aData.isNumber()) {
        aOut = java::GeckoBundle::Box(aData.toNumber());
    } else if (aData.isString()) {
        return BoxString(aCx, aData, aOut);
    } else if (aData.isObject()) {
        return BoxObject(aCx, aData, aOut);
    } else {
        NS_WARNING("Unknown type");
        return NS_ERROR_INVALID_ARG;
    }
    return NS_OK;
}

nsresult
BoxData(const nsAString& aEvent, JSContext* aCx, JS::HandleValue aData,
        jni::Object::LocalRef& aOut, bool aObjectOnly)
{
    nsresult rv = NS_ERROR_INVALID_ARG;

    if (!aObjectOnly) {
        rv = BoxValue(aCx, aData, aOut);
    } else if (aData.isObject() || aData.isNullOrUndefined()) {
        rv = BoxObject(aCx, aData, aOut);
    }
    if (rv != NS_ERROR_INVALID_ARG) {
        return rv;
    }

    NS_ConvertUTF16toUTF8 event(aEvent);
    if (JS_IsExceptionPending(aCx)) {
        JS_ReportWarningUTF8(aCx, "Error dispatching %s", event.get());
    } else {
        JS_ReportErrorUTF8(aCx, "Invalid event data for %s", event.get());
    }
    return NS_ERROR_INVALID_ARG;
}

nsresult
UnboxString(JSContext* aCx, const jni::Object::LocalRef& aData,
            JS::MutableHandleValue aOut)
{
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

    JS::RootedString str(aCx, JS_NewUCStringCopyN(
            aCx, reinterpret_cast<const char16_t*>(jchars), len));
    NS_ENSURE_TRUE(CheckJS(aCx, !!str), NS_ERROR_FAILURE);

    aOut.setString(str);
    return NS_OK;
}

nsresult
UnboxValue(JSContext* aCx, const jni::Object::LocalRef& aData,
           JS::MutableHandleValue aOut);

nsresult
UnboxBundle(JSContext* aCx, const jni::Object::LocalRef& aData,
            JS::MutableHandleValue aOut)
{
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
    JS::RootedObject obj(aCx, JS_NewPlainObject(aCx));

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

        JS::RootedValue value(aCx);
        nsresult rv = UnboxValue(aCx, values->GetElement(i), &value);
        if (rv == NS_ERROR_INVALID_ARG && !JS_IsExceptionPending(aCx)) {
            JS_ReportErrorUTF8(
                    aCx, u8"Invalid event data property %s",
                    NS_ConvertUTF16toUTF8(nsString(reinterpret_cast<
                            const char16_t*>(keyChars), keyLen)).get());
        }
        NS_ENSURE_SUCCESS(rv, rv);

        NS_ENSURE_TRUE(CheckJS(aCx, JS_SetUCProperty(aCx, obj, reinterpret_cast<
                               const char16_t*>(keyChars), keyLen, value)),
                       NS_ERROR_FAILURE);
    }

    aOut.setObject(*obj);
    return NS_OK;
}

template<typename Type, typename JNIType, typename ArrayType,
         JNIType* (JNIEnv::*GetElements)(ArrayType, jboolean*),
         void (JNIEnv::*ReleaseElements)(ArrayType, JNIType*, jint),
         JS::Value (*ToValue)(Type)> nsresult
UnboxArrayPrimitive(JSContext* aCx, const jni::Object::LocalRef& aData,
                    JS::MutableHandleValue aOut)
{
    JNIEnv* const env = aData.Env();
    const ArrayType jarray = ArrayType(aData.Get());
    JNIType* const array = (env->*GetElements)(jarray, nullptr);
    JS::AutoValueVector elements(aCx);

    if (NS_WARN_IF(!array)) {
        env->ExceptionClear();
        return NS_ERROR_FAILURE;
    }

    auto releaseArray = MakeScopeExit([env, jarray, array] {
        (env->*ReleaseElements)(jarray, array, JNI_ABORT);
        env->ExceptionClear();
    });

    const size_t len = env->GetArrayLength(jarray);
    elements.initCapacity(len);

    for (size_t i = 0; i < len; i++) {
        NS_ENSURE_TRUE(elements.append((*ToValue)(Type(array[i]))),
                       NS_ERROR_FAILURE);
    }

    JS::RootedObject obj(aCx, JS_NewArrayObject(
            aCx, JS::HandleValueArray(elements)));
    NS_ENSURE_TRUE(CheckJS(aCx, !!obj), NS_ERROR_FAILURE);

    aOut.setObject(*obj);
    return NS_OK;
}

struct StringArray : jni::ObjectBase<StringArray>
{
    static const char name[];
};

struct GeckoBundleArray : jni::ObjectBase<GeckoBundleArray>
{
    static const char name[];
};

const char StringArray::name[] = "[Ljava/lang/String;";
const char GeckoBundleArray::name[] = "[Lorg/mozilla/gecko/util/GeckoBundle;";

template<nsresult (*Unbox)(JSContext*, const jni::Object::LocalRef&,
                           JS::MutableHandleValue)> nsresult
UnboxArrayObject(JSContext* aCx, const jni::Object::LocalRef& aData,
                 JS::MutableHandleValue aOut)
{
    jni::ObjectArray::LocalRef array(
            aData.Env(), jni::ObjectArray::Ref::From(aData));
    const size_t len = array->Length();
    JS::RootedObject obj(aCx, JS_NewArrayObject(aCx, len));
    NS_ENSURE_TRUE(CheckJS(aCx, !!obj), NS_ERROR_FAILURE);

    for (size_t i = 0; i < len; i++) {
        jni::Object::LocalRef element = array->GetElement(i);
        JS::RootedValue value(aCx);
        nsresult rv = (*Unbox)(aCx, element, &value);
        NS_ENSURE_SUCCESS(rv, rv);

        NS_ENSURE_TRUE(CheckJS(aCx, JS_SetElement(aCx, obj, i, value)),
                       NS_ERROR_FAILURE);
    }

    aOut.setObject(*obj);
    return NS_OK;
}

nsresult
UnboxValue(JSContext* aCx, const jni::Object::LocalRef& aData,
           JS::MutableHandleValue aOut)
{
    if (!aData) {
        aOut.setNull();
    } else if (aData.IsInstanceOf<jni::Boolean>()) {
        aOut.setBoolean(java::GeckoBundle::UnboxBoolean(aData));
    } else if (aData.IsInstanceOf<jni::Integer>() ||
            aData.IsInstanceOf<jni::Byte>() ||
            aData.IsInstanceOf<jni::Short>()) {
        aOut.setInt32(java::GeckoBundle::UnboxInteger(aData));
    } else if (aData.IsInstanceOf<jni::Double>() ||
            aData.IsInstanceOf<jni::Float>() ||
            aData.IsInstanceOf<jni::Long>()) {
        aOut.setNumber(java::GeckoBundle::UnboxDouble(aData));
    } else if (aData.IsInstanceOf<jni::String>()) {
        return UnboxString(aCx, aData, aOut);
    } else if (aData.IsInstanceOf<jni::Character>()) {
        return UnboxString(aCx, java::GeckoBundle::UnboxString(aData), aOut);
    } else if (aData.IsInstanceOf<java::GeckoBundle>()) {
        return UnboxBundle(aCx, aData, aOut);

    } else if (aData.IsInstanceOf<jni::BooleanArray>()) {
        return UnboxArrayPrimitive<bool, jboolean, jbooleanArray,
                &JNIEnv::GetBooleanArrayElements,
                &JNIEnv::ReleaseBooleanArrayElements,
                &JS::BooleanValue>(aCx, aData, aOut);

    } else if (aData.IsInstanceOf<jni::IntArray>()) {
        return UnboxArrayPrimitive<int32_t, jint, jintArray,
                &JNIEnv::GetIntArrayElements,
                &JNIEnv::ReleaseIntArrayElements,
                &JS::Int32Value>(aCx, aData, aOut);

    } else if (aData.IsInstanceOf<jni::DoubleArray>()) {
        return UnboxArrayPrimitive<double, jdouble, jdoubleArray,
                &JNIEnv::GetDoubleArrayElements,
                &JNIEnv::ReleaseDoubleArrayElements,
                &JS::DoubleValue>(aCx, aData, aOut);

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

nsresult
UnboxData(jni::String::Param aEvent, JSContext* aCx, jni::Object::Param aData,
          JS::MutableHandleValue aOut, bool aBundleOnly)
{
    MOZ_ASSERT(NS_IsMainThread());

    jni::Object::LocalRef jniData(jni::GetGeckoThreadEnv(), aData);
    nsresult rv = NS_ERROR_INVALID_ARG;

    if (!aBundleOnly) {
        rv = UnboxValue(aCx, jniData, aOut);
    } else if (!jniData || jniData.IsInstanceOf<java::GeckoBundle>()) {
        rv = UnboxBundle(aCx, jniData, aOut);
    }
    if (rv != NS_ERROR_INVALID_ARG) {
        return rv;
    }

    nsCString event = aEvent->ToCString();
    if (JS_IsExceptionPending(aCx)) {
        JS_ReportWarningUTF8(aCx, "Error dispatching %s", event.get());
    } else {
        JS_ReportErrorUTF8(aCx, "Invalid event data for %s", event.get());
    }
    return NS_ERROR_INVALID_ARG;
}

class JavaCallbackDelegate final : public nsIAndroidEventCallback
{
    java::EventCallback::GlobalRef mCallback;

    virtual ~JavaCallbackDelegate() {}

    NS_IMETHOD Call(JS::HandleValue aData,
                    void (java::EventCallback::*aCall)(
                            jni::Object::Param) const)
    {
        MOZ_ASSERT(NS_IsMainThread());
        AutoJSContext cx;

        jni::Object::LocalRef data(jni::GetGeckoThreadEnv());
        nsresult rv = BoxData(NS_LITERAL_STRING("callback"), cx, aData, data,
                              /* ObjectOnly */ false);
        NS_ENSURE_SUCCESS(rv, JS_IsExceptionPending(cx) ? NS_OK : rv);

        dom::AutoNoJSAPI nojsapi;
        (java::EventCallback(*mCallback).*aCall)(data);
        return NS_OK;
    }

public:
    JavaCallbackDelegate(java::EventCallback::Param aCallback)
        : mCallback(jni::GetGeckoThreadEnv(), aCallback)
    {}

    NS_DECL_ISUPPORTS

    NS_IMETHOD OnSuccess(JS::HandleValue aData) override
    {
        return Call(aData, &java::EventCallback::SendSuccess);
    }

    NS_IMETHOD OnError(JS::HandleValue aData) override
    {
        return Call(aData, &java::EventCallback::SendError);
    }
};

NS_IMPL_ISUPPORTS(JavaCallbackDelegate, nsIAndroidEventCallback)

class NativeCallbackDelegateSupport final :
    public java::EventDispatcher::NativeCallbackDelegate
            ::Natives<NativeCallbackDelegateSupport>
{
    using CallbackDelegate = java::EventDispatcher::NativeCallbackDelegate;
    using Base = CallbackDelegate::Natives<NativeCallbackDelegateSupport>;

    const nsCOMPtr<nsIAndroidEventCallback> mCallback;
    const nsCOMPtr<nsPIDOMWindowOuter> mWindow;

    void Call(jni::Object::Param aData,
              nsresult (nsIAndroidEventCallback::*aCall)(JS::HandleValue))
    {
        MOZ_ASSERT(NS_IsMainThread());

        // Use the same compartment as the wrapped JS object if possible,
        // otherwise use either the attached window's compartment or a default
        // compartment.
        nsCOMPtr<nsIXPConnectWrappedJS> wrappedJS(do_QueryInterface(mCallback));

        dom::AutoJSAPI jsapi;
        if (!wrappedJS && mWindow) {
            NS_ENSURE_TRUE_VOID(jsapi.Init(mWindow->GetCurrentInnerWindow()));
        } else {
            NS_ENSURE_TRUE_VOID(jsapi.Init(wrappedJS ?
                    wrappedJS->GetJSObject() : xpc::PrivilegedJunkScope()));
        }

        JS::RootedValue data(jsapi.cx());
        nsresult rv = UnboxData(NS_LITERAL_STRING("callback"), jsapi.cx(),
                                aData, &data, /* BundleOnly */ false);
        NS_ENSURE_SUCCESS_VOID(rv);

        dom::AutoNoJSAPI nojsapi;
        rv = (mCallback->*aCall)(data);
        NS_ENSURE_SUCCESS_VOID(rv);
    }

public:
    using Base::AttachNative;

    template<typename Functor>
    static void OnNativeCall(Functor&& aCall)
    {
        if (NS_IsMainThread()) {
            // Invoke callbacks synchronously if we're already on Gecko thread.
            return aCall();
        }
        nsAppShell::PostEvent(Move(aCall));
    }

    static void Finalize(const CallbackDelegate::LocalRef& aInstance)
    {
        DisposeNative(aInstance);
    }

    NativeCallbackDelegateSupport(nsIAndroidEventCallback* callback,
                                  nsPIDOMWindowOuter* domWindow)
        : mCallback(callback)
        , mWindow(domWindow)
    {}

    void SendSuccess(jni::Object::Param aData)
    {
        Call(aData, &nsIAndroidEventCallback::OnSuccess);
    }

    void SendError(jni::Object::Param aData)
    {
        Call(aData, &nsIAndroidEventCallback::OnError);
    }
};

} // namespace detail

using namespace detail;

NS_IMPL_ISUPPORTS(EventDispatcher, nsIAndroidEventDispatcher)

nsresult
EventDispatcher::DispatchOnGecko(ListenersList* list, const nsAString& aEvent,
                                 JS::HandleValue aData,
                                 nsIAndroidEventCallback* aCallback)
{
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
        const nsresult rv = list->listeners[i]->OnEvent(
                aEvent, aData, aCallback);
        NS_ENSURE_SUCCESS(rv, rv);
    }
    return NS_OK;
}

NS_IMETHODIMP
EventDispatcher::Dispatch(JS::HandleValue aEvent, JS::HandleValue aData,
                          nsIAndroidEventCallback* aCallback, JSContext* aCx)
{
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
        return DispatchOnGecko(list, event, aData, aCallback);
    }

    if (!mDispatcher) {
        return NS_OK;
    }

    jni::Object::LocalRef data(jni::GetGeckoThreadEnv());
    nsresult rv = BoxData(event, aCx, aData, data, /* ObjectOnly */ true);
    NS_ENSURE_SUCCESS(rv, JS_IsExceptionPending(aCx) ? NS_OK : rv);

    dom::AutoNoJSAPI nojsapi;

    java::EventDispatcher::NativeCallbackDelegate::LocalRef
            callback(data.Env());
    if (aCallback) {
        callback = java::EventDispatcher::NativeCallbackDelegate::New();
        NativeCallbackDelegateSupport::AttachNative(
                callback,
                MakeUnique<NativeCallbackDelegateSupport>(
                        aCallback, mDOMWindow));
    }

    mDispatcher->DispatchToThreads(event, data, callback);
    return NS_OK;
}

nsresult
EventDispatcher::IterateEvents(JSContext* aCx, JS::HandleValue aEvents,
                               IterateEventsCallback aCallback,
                               nsIAndroidEventListener* aListener)
{
    MOZ_ASSERT(NS_IsMainThread());

    MutexAutoLock lock(mLock);

    auto processEvent = [this, aCx, aCallback, aListener]
            (JS::HandleValue event) -> nsresult {
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
    NS_ENSURE_TRUE(CheckJS(aCx, JS_IsArrayObject(aCx, aEvents, &isArray)),
                   NS_ERROR_INVALID_ARG);
    NS_ENSURE_TRUE(isArray, NS_ERROR_INVALID_ARG);

    JS::RootedObject events(aCx, &aEvents.toObject());
    uint32_t length = 0;
    NS_ENSURE_TRUE(CheckJS(aCx, JS_GetArrayLength(aCx, events, &length)),
                   NS_ERROR_INVALID_ARG);
    NS_ENSURE_TRUE(length, NS_ERROR_INVALID_ARG);

    for (size_t i = 0; i < length; i++) {
        JS::RootedValue event(aCx);
        NS_ENSURE_TRUE(CheckJS(aCx, JS_GetElement(aCx, events, i, &event)),
                       NS_ERROR_INVALID_ARG);
        NS_ENSURE_TRUE(event.isString(), NS_ERROR_INVALID_ARG);

        const nsresult rv = processEvent(event);
        NS_ENSURE_SUCCESS(rv, rv);
    }
    return NS_OK;
}

nsresult
EventDispatcher::RegisterEventLocked(const nsAString& aEvent,
                                     nsIAndroidEventListener* aListener)
{
    ListenersList* list = mListenersMap.Get(aEvent);
    if (!list) {
        list = new ListenersList();
        mListenersMap.Put(aEvent, list);
    }

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
                                  JS::HandleValue aEvents, JSContext *aCx)
{
    return IterateEvents(aCx, aEvents, &EventDispatcher::RegisterEventLocked,
                         aListener);
}

nsresult
EventDispatcher::UnregisterEventLocked(const nsAString& aEvent,
                                       nsIAndroidEventListener* aListener)
{
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
                                    JS::HandleValue aEvents, JSContext *aCx)
{
    return IterateEvents(aCx, aEvents, &EventDispatcher::UnregisterEventLocked,
                         aListener);
}

void
EventDispatcher::Attach(java::EventDispatcher::Param aDispatcher,
                        nsPIDOMWindowOuter* aDOMWindow)
{
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(aDispatcher);

    if (mDispatcher) {
        if (mDispatcher == aDispatcher) {
            // Only need to update the window.
            mDOMWindow = aDOMWindow;
            return;
        }
        mDispatcher->SetAttachedToGecko(java::EventDispatcher::REATTACHING);
    }

    java::EventDispatcher::LocalRef dispatcher(aDispatcher);

    NativesBase::AttachNative(dispatcher, this);
    mDispatcher = dispatcher;
    mDOMWindow = aDOMWindow;

    dispatcher->SetAttachedToGecko(java::EventDispatcher::ATTACHED);
}

void
EventDispatcher::Detach()
{
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(mDispatcher);

    // SetAttachedToGecko will call disposeNative for us. disposeNative will be
    // called later on the Gecko thread to make sure all pending
    // dispatchToGecko calls have completed.
    mDispatcher->SetAttachedToGecko(java::EventDispatcher::DETACHED);
    mDispatcher = nullptr;
    mDOMWindow = nullptr;
}

bool
EventDispatcher::HasGeckoListener(jni::String::Param aEvent)
{
    // Can be called from any thread.
    MutexAutoLock lock(mLock);
    return !!mListenersMap.Get(aEvent->ToString());
}

void
EventDispatcher::DispatchToGecko(jni::String::Param aEvent,
                                 jni::Object::Param aData,
                                 jni::Object::Param aCallback)
{
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
    if (mDOMWindow) {
        NS_ENSURE_TRUE_VOID(jsapi.Init(mDOMWindow->GetCurrentInnerWindow()));
    } else {
        NS_ENSURE_TRUE_VOID(jsapi.Init(xpc::PrivilegedJunkScope()));
    }

    JS::RootedValue data(jsapi.cx());
    nsresult rv = UnboxData(aEvent, jsapi.cx(), aData, &data,
                            /* BundleOnly */ true);
    NS_ENSURE_SUCCESS_VOID(rv);

    nsCOMPtr<nsIAndroidEventCallback> callback;
    if (aCallback) {
        callback = new JavaCallbackDelegate(
                java::EventCallback::Ref::From(aCallback));
    }

    DispatchOnGecko(list, event, data, callback);
}

} // namespace widget
} // namespace mozilla

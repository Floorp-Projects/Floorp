/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/jni/GeckoBundleUtils.h"

#include "JavaBuiltins.h"
#include "js/Warnings.h"
#include "nsJSUtils.h"

#include "js/Array.h"

namespace mozilla::jni {
namespace detail {
bool CheckJS(JSContext* aCx, bool aResult) {
  if (!aResult) {
    JS_ClearPendingException(aCx);
  }
  return aResult;
}

nsresult BoxString(JSContext* aCx, JS::HandleValue aData,
                   jni::Object::LocalRef& aOut) {
  if (aData.isNullOrUndefined()) {
    aOut = nullptr;
    return NS_OK;
  }

  MOZ_ASSERT(aData.isString());

  JS::RootedString str(aCx, aData.toString());

  if (JS::StringHasLatin1Chars(str)) {
    nsAutoJSString autoStr;
    NS_ENSURE_TRUE(CheckJS(aCx, autoStr.init(aCx, str)), NS_ERROR_FAILURE);

    // StringParam can automatically convert a nsString to jstring.
    aOut = jni::StringParam(autoStr, aOut.Env(), fallible);
    if (!aOut) {
      return NS_ERROR_FAILURE;
    }
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
      aOut = jni::String::LocalRef::Adopt(
          env, env->NewString(reinterpret_cast<const jchar*>(chars), len));
    }
  }
  if (NS_WARN_IF(!CheckJS(aCx, !!chars) || !aOut)) {
    env->ExceptionClear();
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

nsresult BoxObject(JSContext* aCx, JS::HandleValue aData,
                   jni::Object::LocalRef& aOut);

template <typename Type, bool (JS::Value::*IsType)() const,
          Type (JS::Value::*ToType)() const, class ArrayType,
          typename ArrayType::LocalRef (*NewArray)(const Type*, size_t)>
nsresult BoxArrayPrimitive(JSContext* aCx, JS::HandleObject aData,
                           jni::Object::LocalRef& aOut, size_t aLength,
                           JS::HandleValue aElement) {
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

template <class Type,
          nsresult (*Box)(JSContext*, JS::HandleValue, jni::Object::LocalRef&),
          typename IsType>
nsresult BoxArrayObject(JSContext* aCx, JS::HandleObject aData,
                        jni::Object::LocalRef& aOut, size_t aLength,
                        JS::HandleValue aElement, IsType&& aIsType) {
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

nsresult BoxArray(JSContext* aCx, JS::HandleObject aData,
                  jni::Object::LocalRef& aOut) {
  uint32_t length = 0;
  NS_ENSURE_TRUE(CheckJS(aCx, JS::GetArrayLength(aCx, aData, &length)),
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
    nsresult rv =
        BoxArrayPrimitive<int32_t, &JS::Value::isInt32, &JS::Value::toInt32,
                          jni::IntArray, &jni::IntArray::New>(aCx, aData, aOut,
                                                              length, element);
    if (rv != NS_ERROR_INVALID_ARG) {
      return rv;
    }
    // Not int32, but we can still try a double array.
  }

  if (element.isNumber()) {
    return BoxArrayPrimitive<double, &JS::Value::isNumber, &JS::Value::toNumber,
                             jni::DoubleArray, &jni::DoubleArray::New>(
        aCx, aData, aOut, length, element);
  }

  if (element.isNullOrUndefined() || element.isString()) {
    const auto isString = [](JS::HandleValue val) -> bool {
      return val.isString();
    };
    nsresult rv = BoxArrayObject<jni::String, &BoxString>(
        aCx, aData, aOut, length, element, isString);
    if (element.isString() || rv != NS_ERROR_INVALID_ARG) {
      return rv;
    }
    // First element was null/undefined, so it may still be an object array.
  }

  const auto isObject = [aCx](JS::HandleValue val) -> bool {
    if (!val.isObject()) {
      return false;
    }
    bool array = false;
    JS::RootedObject obj(aCx, &val.toObject());
    // We don't support array of arrays.
    return CheckJS(aCx, JS::IsArrayObject(aCx, obj, &array)) && !array;
  };

  if (element.isNullOrUndefined() || isObject(element)) {
    return BoxArrayObject<java::GeckoBundle, &BoxObject>(
        aCx, aData, aOut, length, element, isObject);
  }

  NS_WARNING("Unknown type");
  return NS_ERROR_INVALID_ARG;
}

nsresult BoxValue(JSContext* aCx, JS::HandleValue aData,
                  jni::Object::LocalRef& aOut);

nsresult BoxObject(JSContext* aCx, JS::HandleValue aData,
                   jni::Object::LocalRef& aOut) {
  if (aData.isNullOrUndefined()) {
    aOut = nullptr;
    return NS_OK;
  }

  MOZ_ASSERT(aData.isObject());

  JS::Rooted<JS::IdVector> ids(aCx, JS::IdVector(aCx));
  JS::RootedObject obj(aCx, &aData.toObject());

  bool isArray = false;
  if (CheckJS(aCx, JS::IsArrayObject(aCx, obj, &isArray)) && isArray) {
    return BoxArray(aCx, obj, aOut);
  }

  NS_ENSURE_TRUE(CheckJS(aCx, JS_Enumerate(aCx, obj, &ids)), NS_ERROR_FAILURE);

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
        JS_ReportErrorUTF8(aCx, u8"Invalid event data property %s",
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

nsresult BoxValue(JSContext* aCx, JS::HandleValue aData,
                  jni::Object::LocalRef& aOut) {
  if (aData.isNullOrUndefined()) {
    aOut = nullptr;
  } else if (aData.isBoolean()) {
    aOut = aData.toBoolean() ? java::sdk::Boolean::TRUE()
                             : java::sdk::Boolean::FALSE();
  } else if (aData.isInt32()) {
    aOut = java::sdk::Integer::ValueOf(aData.toInt32());
  } else if (aData.isNumber()) {
    aOut = java::sdk::Double::New(aData.toNumber());
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

}  // namespace detail

nsresult BoxData(JSContext* aCx, JS::HandleValue aData,
                 jni::Object::LocalRef& aOut, bool aObjectOnly) {
  nsresult rv = NS_ERROR_INVALID_ARG;

  if (!aObjectOnly) {
    rv = detail::BoxValue(aCx, aData, aOut);
  } else if (aData.isObject() || aData.isNullOrUndefined()) {
    rv = detail::BoxObject(aCx, aData, aOut);
  }

  return rv;
}
}  // namespace mozilla::jni

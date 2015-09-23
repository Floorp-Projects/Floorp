/* -*- Mode: c++; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "NativeJSContainer.h"

#include <jni.h>

#include "Bundle.h"
#include "GeneratedJNINatives.h"
#include "MainThreadUtils.h"
#include "jsapi.h"
#include "nsJSUtils.h"

#include <mozilla/Vector.h>
#include <mozilla/jni/Accessors.h>
#include <mozilla/jni/Refs.h>
#include <mozilla/jni/Utils.h>

/**
 * NativeJSContainer.cpp implements the native methods in both
 * NativeJSContainer and NativeJSObject, using JSAPI to retrieve values from a
 * JSObject and using JNI to return those values to Java code.
 */

namespace mozilla {
namespace widget {

namespace {

bool CheckThread()
{
    if (!NS_IsMainThread()) {
        jni::ThrowException("java/lang/IllegalThreadStateException",
                            "Not on Gecko thread");
        return false;
    }
    return true;
}

bool
CheckJSCall(bool result)
{
    if (!result) {
        jni::ThrowException("java/lang/UnsupportedOperationException",
                            "JSAPI call failed");
    }
    return result;
}

template<class C> bool
CheckJNIArgument(const jni::Ref<C>& arg)
{
    if (!arg) {
        jni::ThrowException("java/lang/IllegalArgumentException",
                            "Null argument");
    }
    return !!arg;
}

nsresult
CheckSDKCall(nsresult rv)
{
    if (NS_FAILED(rv)) {
        jni::ThrowException("java/lang/UnsupportedOperationException",
                            "SDK JNI call failed");
    }
    return rv;
}

// Convert a JNI string to a char16_t string that JSAPI expects.
class JSJNIString final
{
    JNIEnv* const mEnv;
    jni::String::Param mJNIString;
    const char16_t* const mJSString;

public:
    JSJNIString(JNIEnv* env, jni::String::Param str)
        : mEnv(env)
        , mJNIString(str)
        , mJSString(!str ? nullptr : reinterpret_cast<const char16_t*>(
                mEnv->GetStringChars(str.Get(), nullptr)))
    {}

    ~JSJNIString() {
        if (mJNIString) {
            mEnv->ReleaseStringChars(mJNIString.Get(),
                reinterpret_cast<const jchar*>(mJSString));
        }
    }

    operator const char16_t*() const {
        return mJSString;
    }

    size_t Length() const {
        return static_cast<size_t>(mEnv->GetStringLength(mJNIString.Get()));
    }
};

} // namepsace

class NativeJSContainerImpl final
    : public NativeJSObject::Natives<NativeJSContainerImpl>
    , public NativeJSContainer::Natives<NativeJSContainerImpl>
{
    typedef NativeJSContainerImpl Self;
    typedef NativeJSContainer::Natives<NativeJSContainerImpl> ContainerBase;
    typedef NativeJSObject::Natives<NativeJSContainerImpl> ObjectBase;

    typedef JS::PersistentRooted<JSObject*> PersistentObject;

    JNIEnv* const mEnv;
    // Context that the object is valid in
    JSContext* const mJSContext;
    // Root JS object
    PersistentObject mJSObject;
    // Children objects
    Vector<NativeJSObject::GlobalRef, 0> mChildren;

    bool CheckObject() const
    {
        if (!mJSObject) {
            jni::ThrowException("java/lang/NullPointerException",
                                "Null JSObject");
        }
        return !!mJSObject;
    }

    // Check that a JS Value contains a particular property type as indicaed by
    // the property's InValue method (e.g. StringProperty::InValue).
    bool CheckProperty(bool (Self::*InValue)(JS::HandleValue) const,
                       JS::HandleValue val) const
    {
        if (!(this->*InValue)(val)) {
            // XXX this can happen when converting a double array inside a
            // Bundle, because double arrays can be misidentified as an int
            // array. The workaround is to add a dummy first element to the
            // array that is a floating point value, i.e. [0.5, ...].
            jni::ThrowException(
                "org/mozilla/gecko/util/NativeJSObject$InvalidPropertyException",
                "Property type mismatch");
            return false;
        }
        return true;
    }

    // Primitive properties

    template<bool (JS::Value::*IsType)() const> bool
    PrimitiveInValue(JS::HandleValue val) const
    {
        return (static_cast<const JS::Value&>(val).*IsType)();
    }

    template<typename U, U (JS::Value::*ToType)() const> U
    PrimitiveFromValue(JS::HandleValue val) const
    {
        return (static_cast<const JS::Value&>(val).*ToType)();
    }

    template<class Prop> typename Prop::NativeArray
    PrimitiveNewArray(JS::HandleObject array, size_t length) const
    {
        typedef typename Prop::JNIType JNIType;

        // Fill up a temporary buffer for our array, then use
        // JNIEnv::Set*ArrayRegion to fill out array in one go.

        UniquePtr<JNIType[]> buffer = MakeUnique<JNIType[]>(length);
        for (size_t i = 0; i < length; i++) {
            JS::RootedValue elem(mJSContext);
            if (!CheckJSCall(JS_GetElement(mJSContext, array, i, &elem)) ||
                !CheckProperty(Prop::InValue, elem)) {
                return nullptr;
            }
            buffer[i] = JNIType((this->*Prop::FromValue)(elem));
        }
        auto jarray = Prop::NativeArray::Adopt(
                mEnv, (mEnv->*Prop::NewJNIArray)(length));
        if (!jarray) {
            return nullptr;
        }
        (mEnv->*Prop::SetJNIArrayRegion)(
                jarray.Get(), 0, length, buffer.get());
        if (mEnv->ExceptionCheck()) {
            return nullptr;
        }
        return jarray;
    }

    template<typename U, typename UA, typename V, typename VA,
             bool (JS::Value::*IsType)() const,
             U (JS::Value::*ToType)() const,
             VA (JNIEnv::*NewArray_)(jsize),
             void (JNIEnv::*SetArrayRegion_)(VA, jsize, jsize, const V*)>
    struct PrimitiveProperty
    {
        // C++ type for a primitive property (e.g. bool)
        typedef U NativeType;
        // C++ type for the fallback value used in opt* methods
        typedef U NativeFallback;
        // Type for an array of the primitive type (e.g. BooleanArray::LocalRef)
        typedef typename UA::LocalRef NativeArray;
        // Type for the fallback value used in opt*Array methods
        typedef const typename UA::Ref ArrayFallback;
        // JNI type (e.g. jboolean)
        typedef V JNIType;

        // JNIEnv function to create a new JNI array of the primiive type
        typedef decltype(NewArray_) NewJNIArray_t;
        static constexpr NewJNIArray_t NewJNIArray = NewArray_;

        // JNIEnv function to fill a JNI array of the primiive type
        typedef decltype(SetArrayRegion_) SetJNIArrayRegion_t;
        static constexpr SetJNIArrayRegion_t SetJNIArrayRegion = SetArrayRegion_;

        // Function to determine if a JS Value contains the primitive type
        typedef decltype(&Self::PrimitiveInValue<IsType>) InValue_t;
        static constexpr InValue_t InValue = &Self::PrimitiveInValue<IsType>;

        // Function to convert a JS Value to the primitive type
        typedef decltype(&Self::PrimitiveFromValue<U, ToType>) FromValue_t;
        static constexpr FromValue_t FromValue
                = &Self::PrimitiveFromValue<U, ToType>;

        // Function to convert a JS array to a JNI array
        typedef decltype(&Self::PrimitiveNewArray<PrimitiveProperty>) NewArray_t;
        static constexpr NewArray_t NewArray
                = &Self::PrimitiveNewArray<PrimitiveProperty>;
    };

    // String properties

    bool StringInValue(JS::HandleValue val) const
    {
        return val.isString();
    }

    jni::String::LocalRef
    StringFromValue(const JS::HandleString str) const
    {
        nsAutoJSString autoStr;
        if (!CheckJSCall(autoStr.init(mJSContext, str))) {
            return nullptr;
        }
        // Param<String> can automatically convert a nsString to jstring.
        return jni::Param<jni::String>(autoStr, mEnv);
    }

    jni::String::LocalRef
    StringFromValue(JS::HandleValue val)
    {
        const JS::RootedString str(mJSContext, val.toString());
        return StringFromValue(str);
    }

    // Bundle properties

    sdk::Bundle::LocalRef
    BundleFromValue(const JS::HandleObject obj)
    {
        JS::Rooted<JS::IdVector> ids(mJSContext, JS::IdVector(mJSContext));
        if (!CheckJSCall(JS_Enumerate(mJSContext, obj, &ids))) {
            return nullptr;
        }

        const size_t length = ids.length();
        sdk::Bundle::LocalRef newBundle(mEnv);
        NS_ENSURE_SUCCESS(CheckSDKCall(
                sdk::Bundle::New(length, &newBundle)), nullptr);

        // Iterate through each property of the JS object. For each property,
        // determine its type from a list of supported types, and convert that
        // proeprty to the supported type.

        for (size_t i = 0; i < ids.length(); i++) {
            const JS::RootedId id(mJSContext, ids[i]);
            JS::RootedValue idVal(mJSContext);
            if (!CheckJSCall(JS_IdToValue(mJSContext, id, &idVal))) {
                return nullptr;
            }

            const JS::RootedString idStr(mJSContext,
                                         JS::ToString(mJSContext, idVal));
            if (!CheckJSCall(!!idStr)) {
                return nullptr;
            }

            jni::String::LocalRef name = StringFromValue(idStr);
            JS::RootedValue val(mJSContext);
            if (!name ||
                !CheckJSCall(JS_GetPropertyById(mJSContext, obj, id, &val))) {
                return nullptr;
            }

#define PUT_IN_BUNDLE_IF_TYPE_IS(TYPE)                                  \
            if ((this->*TYPE##Property::InValue)(val)) {                \
                auto jval = (this->*TYPE##Property::FromValue)(val);    \
                if (mEnv->ExceptionCheck()) {                           \
                    return nullptr;                                     \
                }                                                       \
                NS_ENSURE_SUCCESS(CheckSDKCall(                         \
                        newBundle->Put##TYPE(name, jval)), nullptr);    \
                continue;                                               \
            }                                                           \
            ((void) 0) // Accommodate trailing semicolon.

            // Scalar values are faster to check, so check them first.
            PUT_IN_BUNDLE_IF_TYPE_IS(Boolean);
            // Int can be casted to double, so check int first.
            PUT_IN_BUNDLE_IF_TYPE_IS(Int);
            PUT_IN_BUNDLE_IF_TYPE_IS(Double);
            PUT_IN_BUNDLE_IF_TYPE_IS(String);
            // There's no "putObject", so don't check ObjectProperty

            // Check for array types if scalar checks all failed.
            // XXX empty arrays are treated as boolean arrays. Workaround is
            // to always have a dummy element to create a non-empty array.
            PUT_IN_BUNDLE_IF_TYPE_IS(BooleanArray);
            // XXX because we only check the first element of an array,
            // a double array can potentially be seen as an int array.
            // When that happens, the Bundle conversion will fail.
            PUT_IN_BUNDLE_IF_TYPE_IS(IntArray);
            PUT_IN_BUNDLE_IF_TYPE_IS(DoubleArray);
            PUT_IN_BUNDLE_IF_TYPE_IS(StringArray);
            // There's no "putObjectArray", so don't check ObjectArrayProperty
            // There's no "putBundleArray", so don't check BundleArrayProperty

            // Use Bundle as the default catch-all for objects
            PUT_IN_BUNDLE_IF_TYPE_IS(Bundle);

#undef PUT_IN_BUNDLE_IF_TYPE_IS

            // We tried all supported types; just bail.
            jni::ThrowException("java/lang/UnsupportedOperationException",
                                "Unsupported property type");
            return nullptr;
        }
        return jni::Object::LocalRef::Adopt(newBundle.Env(),
                                            newBundle.Forget());
    }

    sdk::Bundle::LocalRef
    BundleFromValue(JS::HandleValue val)
    {
        if (val.isNull()) {
            return nullptr;
        }
        JS::RootedObject object(mJSContext, &val.toObject());
        return BundleFromValue(object);
    }

    // Object properties

    bool ObjectInValue(JS::HandleValue val) const
    {
        return val.isObjectOrNull();
    }

    NativeJSObject::LocalRef
    ObjectFromValue(JS::HandleValue val)
    {
        if (val.isNull()) {
            return nullptr;
        }
        JS::RootedObject object(mJSContext, &val.toObject());
        return CreateChild(object);
    }

    template<class Prop> typename Prop::NativeArray
    ObjectNewArray(JS::HandleObject array, size_t length)
    {
        auto jarray = Prop::NativeArray::Adopt(mEnv, mEnv->NewObjectArray(
                length,
                jni::Accessor::EnsureClassRef<typename Prop::ClassType>(mEnv),
                nullptr));
        if (!jarray) {
            return nullptr;
        }

        // For object arrays, we have to set each element separately.
        for (size_t i = 0; i < length; i++) {
            JS::RootedValue elem(mJSContext);
            if (!CheckJSCall(JS_GetElement(mJSContext, array, i, &elem)) ||
                !CheckProperty(Prop::InValue, elem)) {
                return nullptr;
            }
            mEnv->SetObjectArrayElement(
                    jarray.Get(), i, (this->*Prop::FromValue)(elem).Get());
            if (mEnv->ExceptionCheck()) {
                return nullptr;
            }
        }
        return jarray;
    }

    template<class Class,
             bool (Self::*InValue_)(JS::HandleValue) const,
             typename Class::LocalRef (Self::*FromValue_)(JS::HandleValue)>
    struct BaseObjectProperty
    {
        // JNI class for the object type (e.g. jni::String)
        typedef Class ClassType;

        // See comments in PrimitiveProperty.
        typedef typename ClassType::LocalRef NativeType;
        typedef const typename ClassType::Ref NativeFallback;
        typedef typename jni::ObjectArray::LocalRef NativeArray;
        typedef const jni::ObjectArray::Ref ArrayFallback;

        typedef decltype(InValue_) InValue_t;
        static constexpr InValue_t InValue = InValue_;

        typedef decltype(FromValue_) FromValue_t;
        static constexpr FromValue_t FromValue = FromValue_;

        typedef decltype(&Self::ObjectNewArray<BaseObjectProperty>) NewArray_t;
        static constexpr NewArray_t NewArray
                = &Self::ObjectNewArray<BaseObjectProperty>;
    };

    // Array properties

    template<class Prop> bool
    ArrayInValue(JS::HandleValue val) const
    {
        if (!val.isObject()) {
            return false;
        }
        JS::RootedObject obj(mJSContext, &val.toObject());
        bool isArray;
        uint32_t length = 0;
        if (!JS_IsArrayObject(mJSContext, obj, &isArray) ||
            !isArray ||
            !JS_GetArrayLength(mJSContext, obj, &length)) {
            return false;
        }
        if (!length) {
            // Empty arrays are always okay.
            return true;
        }
        // We only check to see the first element is the target type. If the
        // array has mixed types, we'll throw an error during actual conversion.
        JS::RootedValue element(mJSContext);
        return JS_GetElement(mJSContext, obj, 0, &element) &&
               (this->*Prop::InValue)(element);
    }

    template<class Prop> typename Prop::NativeArray
    ArrayFromValue(JS::HandleValue val)
    {
        JS::RootedObject obj(mJSContext, &val.toObject());
        uint32_t length = 0;
        if (!CheckJSCall(JS_GetArrayLength(mJSContext, obj, &length))) {
            return nullptr;
        }
        return (this->*Prop::NewArray)(obj, length);
    }

    template<class Prop>
    struct ArrayProperty
    {
        // See comments in PrimitiveProperty.
        typedef typename Prop::NativeArray NativeType;
        typedef typename Prop::ArrayFallback NativeFallback;

        typedef decltype(&Self::ArrayInValue<Prop>) InValue_t;
        static constexpr InValue_t InValue
                = &Self::ArrayInValue<Prop>;

        typedef decltype(&Self::ArrayFromValue<Prop>) FromValue_t;
        static constexpr FromValue_t FromValue
                = &Self::ArrayFromValue<Prop>;
    };

    // "Has" property is a special property type that is used to implement
    // NativeJSObject.has, by returning true from InValue and FromValue for
    // every existing property, and having false as the fallback value for
    // when a property doesn't exist.

    bool HasValue(JS::HandleValue val) const
    {
        return true;
    }

    struct HasProperty
    {
        // See comments in PrimitiveProperty.
        typedef bool NativeType;
        typedef bool NativeFallback;

        typedef decltype(&Self::HasValue) HasValue_t;
        static constexpr HasValue_t InValue = &Self::HasValue;
        static constexpr HasValue_t FromValue = &Self::HasValue;
    };

    // Statically cast from bool to jboolean (unsigned char); it works
    // since false and JNI_FALSE have the same value (0), and true and
    // JNI_TRUE have the same value (1).
    typedef PrimitiveProperty<
            bool, jni::BooleanArray, jboolean, jbooleanArray,
            &JS::Value::isBoolean, &JS::Value::toBoolean,
            &JNIEnv::NewBooleanArray, &JNIEnv::SetBooleanArrayRegion>
        BooleanProperty;

    typedef PrimitiveProperty<
            double, jni::DoubleArray, jdouble, jdoubleArray,
            &JS::Value::isNumber, &JS::Value::toNumber,
            &JNIEnv::NewDoubleArray, &JNIEnv::SetDoubleArrayRegion>
        DoubleProperty;

    typedef PrimitiveProperty<
            int32_t, jni::IntArray, jint, jintArray,
            &JS::Value::isInt32, &JS::Value::toInt32,
            &JNIEnv::NewIntArray, &JNIEnv::SetIntArrayRegion>
        IntProperty;

    typedef BaseObjectProperty<
            jni::String, &Self::StringInValue, &Self::StringFromValue>
        StringProperty;

    typedef BaseObjectProperty<
            sdk::Bundle, &Self::ObjectInValue, &Self::BundleFromValue>
        BundleProperty;

    typedef BaseObjectProperty<
            NativeJSObject, &Self::ObjectInValue, &Self::ObjectFromValue>
        ObjectProperty;

    typedef ArrayProperty<BooleanProperty> BooleanArrayProperty;
    typedef ArrayProperty<DoubleProperty> DoubleArrayProperty;
    typedef ArrayProperty<IntProperty> IntArrayProperty;
    typedef ArrayProperty<StringProperty> StringArrayProperty;
    typedef ArrayProperty<BundleProperty> BundleArrayProperty;
    typedef ArrayProperty<ObjectProperty> ObjectArrayProperty;

    template<class Prop>
    typename Prop::NativeType
    GetProperty(jni::String::Param name,
                typename Prop::NativeFallback* fallback = nullptr)
    {
        if (!CheckThread() || !CheckObject()) {
            return typename Prop::NativeType();
        }

        const JSJNIString nameStr(mEnv, name);
        JS::RootedValue val(mJSContext);

        if (!CheckJNIArgument(name) ||
            !CheckJSCall(JS_GetUCProperty(
                    mJSContext, mJSObject, nameStr, nameStr.Length(), &val))) {
            return typename Prop::NativeType();
        }

        // Strictly, null is different from undefined in JS. However, in
        // practice, null is often used to indicate a property doesn't exist in
        // the same manner as undefined. Therefore, we treat null in the same
        // way as undefined when checking property existence (bug 1014965).
        if (val.isUndefined() || val.isNull()) {
            if (fallback) {
                return mozilla::Move(*fallback);
            }
            jni::ThrowException(
                "org/mozilla/gecko/util/NativeJSObject$InvalidPropertyException",
                "Property does not exist");
            return typename Prop::NativeType();
        }

        if (!CheckProperty(Prop::InValue, val)) {
            return typename Prop::NativeType();
        }
        return (this->*Prop::FromValue)(val);
    }

    NativeJSObject::LocalRef CreateChild(JS::HandleObject object)
    {
        auto instance = NativeJSObject::New();
        mozilla::UniquePtr<NativeJSContainerImpl> impl(
                new NativeJSContainerImpl(instance.Env(), mJSContext, object));

        ObjectBase::AttachNative(instance, mozilla::Move(impl));
        mChildren.append(NativeJSObject::GlobalRef(instance));
        return instance;
    }

    NativeJSContainerImpl(JNIEnv* env, JSContext* cx, JS::HandleObject object)
        : mEnv(env)
        , mJSContext(cx)
        , mJSObject(cx, object)
    {}

public:
    ~NativeJSContainerImpl()
    {
        // Dispose of all children on destruction. The children will in turn
        // dispose any of their children (i.e. our grandchildren) and so on.
        NativeJSObject::LocalRef child(mEnv);
        for (size_t i = 0; i < mChildren.length(); i++) {
            child = mChildren[i];
            ObjectBase::GetNative(child)->ObjectBase::DisposeNative(child);
        }
    }

    static NativeJSContainer::LocalRef
    CreateInstance(JSContext* cx, JS::HandleObject object)
    {
        auto instance = NativeJSContainer::New();
        mozilla::UniquePtr<NativeJSContainerImpl> impl(
                new NativeJSContainerImpl(instance.Env(), cx, object));

        ContainerBase::AttachNative(instance, mozilla::Move(impl));
        return instance;
    }

    // NativeJSContainer methods

    void DisposeNative(const NativeJSContainer::LocalRef& instance)
    {
        if (!CheckThread()) {
            return;
        }
        ContainerBase::DisposeNative(instance);
    }

    NativeJSContainer::LocalRef Clone()
    {
        if (!CheckThread()) {
            return nullptr;
        }
        return CreateInstance(mJSContext, mJSObject);
    }

    // NativeJSObject methods

    bool GetBoolean(jni::String::Param name)
    {
        return GetProperty<BooleanProperty>(name);
    }

    bool OptBoolean(jni::String::Param name, bool fallback)
    {
        return GetProperty<BooleanProperty>(name, &fallback);
    }

    jni::BooleanArray::LocalRef
    GetBooleanArray(jni::String::Param name)
    {
        return GetProperty<BooleanArrayProperty>(name);
    }

    jni::BooleanArray::LocalRef
    OptBooleanArray(jni::String::Param name, jni::BooleanArray::Param fallback)
    {
        return GetProperty<BooleanArrayProperty>(name, &fallback);
    }

    jni::Object::LocalRef
    GetBundle(jni::String::Param name)
    {
        return GetProperty<BundleProperty>(name);
    }

    jni::Object::LocalRef
    OptBundle(jni::String::Param name, jni::Object::Param fallback)
    {
        // Because the GetProperty expects a sdk::Bundle::Param,
        // we have to do conversions here from jni::Object::Param.
        const auto& fb = sdk::Bundle::Ref::From(fallback.Get());
        return GetProperty<BundleProperty>(name, &fb);
    }

    jni::ObjectArray::LocalRef
    GetBundleArray(jni::String::Param name)
    {
        return GetProperty<BundleArrayProperty>(name);
    }

    jni::ObjectArray::LocalRef
    OptBundleArray(jni::String::Param name, jni::ObjectArray::Param fallback)
    {
        return GetProperty<BundleArrayProperty>(name, &fallback);
    }

    double GetDouble(jni::String::Param name)
    {
        return GetProperty<DoubleProperty>(name);
    }

    double OptDouble(jni::String::Param name, double fallback)
    {
        return GetProperty<DoubleProperty>(name, &fallback);
    }

    jni::DoubleArray::LocalRef
    GetDoubleArray(jni::String::Param name)
    {
        return GetProperty<DoubleArrayProperty>(name);
    }

    jni::DoubleArray::LocalRef
    OptDoubleArray(jni::String::Param name, jni::DoubleArray::Param fallback)
    {
        jni::DoubleArray::LocalRef fb(fallback);
        return GetProperty<DoubleArrayProperty>(name, &fb);
    }

    int GetInt(jni::String::Param name)
    {
        return GetProperty<IntProperty>(name);
    }

    int OptInt(jni::String::Param name, int fallback)
    {
        return GetProperty<IntProperty>(name, &fallback);
    }

    jni::IntArray::LocalRef
    GetIntArray(jni::String::Param name)
    {
        return GetProperty<IntArrayProperty>(name);
    }

    jni::IntArray::LocalRef
    OptIntArray(jni::String::Param name, jni::IntArray::Param fallback)
    {
        jni::IntArray::LocalRef fb(fallback);
        return GetProperty<IntArrayProperty>(name, &fb);
    }

    NativeJSObject::LocalRef
    GetObject(jni::String::Param name)
    {
        return GetProperty<ObjectProperty>(name);
    }

    NativeJSObject::LocalRef
    OptObject(jni::String::Param name, NativeJSObject::Param fallback)
    {
        return GetProperty<ObjectProperty>(name, &fallback);
    }

    jni::ObjectArray::LocalRef
    GetObjectArray(jni::String::Param name)
    {
        return GetProperty<ObjectArrayProperty>(name);
    }

    jni::ObjectArray::LocalRef
    OptObjectArray(jni::String::Param name, jni::ObjectArray::Param fallback)
    {
        return GetProperty<ObjectArrayProperty>(name, &fallback);
    }

    jni::String::LocalRef
    GetString(jni::String::Param name)
    {
        return GetProperty<StringProperty>(name);
    }

    jni::String::LocalRef
    OptString(jni::String::Param name, jni::String::Param fallback)
    {
        return GetProperty<StringProperty>(name, &fallback);
    }

    jni::ObjectArray::LocalRef
    GetStringArray(jni::String::Param name)
    {
        return GetProperty<StringArrayProperty>(name);
    }

    jni::ObjectArray::LocalRef
    OptStringArray(jni::String::Param name, jni::ObjectArray::Param fallback)
    {
        return GetProperty<StringArrayProperty>(name, &fallback);
    }

    bool Has(jni::String::Param name)
    {
        bool no = false;
        // Fallback to false indicating no such property.
        return GetProperty<HasProperty>(name, &no);
    }

    jni::Object::LocalRef ToBundle()
    {
        if (!CheckThread() || !CheckObject()) {
            return nullptr;
        }
        return BundleFromValue(mJSObject);
    }

private:
    static bool AppendJSON(const char16_t* buf, uint32_t len, void* data)
    {
        static_cast<nsAutoString*>(data)->Append(buf, len);
        return true;
    }

public:
    jni::String::LocalRef ToString()
    {
        if (!CheckThread() || !CheckObject()) {
            return nullptr;
        }

        JS::RootedValue value(mJSContext, JS::ObjectValue(*mJSObject));
        nsAutoString json;
        if (!CheckJSCall(JS_Stringify(mJSContext, &value, nullptr,
                                      JS::NullHandleValue, AppendJSON, &json))) {
            return nullptr;
        }
        return jni::Param<jni::String>(json, mEnv);
    }
};


// Define the "static constexpr" members of our property types (e.g.
// PrimitiveProperty<>::InValue). This is tricky because there are a lot of
// template parameters, so we use macros to make it simpler.

#define DEFINE_PRIMITIVE_PROPERTY_MEMBER(Name) \
    template<typename U, typename UA, typename V, typename VA, \
             bool (JS::Value::*I)() const, \
             U (JS::Value::*T)() const, \
             VA (JNIEnv::*N)(jsize), \
             void (JNIEnv::*S)(VA, jsize, jsize, const V*)> \
    constexpr typename NativeJSContainerImpl \
        ::PrimitiveProperty<U, UA, V, VA, I, T, N, S>::Name##_t \
    NativeJSContainerImpl::PrimitiveProperty<U, UA, V, VA, I, T, N, S>::Name

DEFINE_PRIMITIVE_PROPERTY_MEMBER(NewJNIArray);
DEFINE_PRIMITIVE_PROPERTY_MEMBER(SetJNIArrayRegion);
DEFINE_PRIMITIVE_PROPERTY_MEMBER(InValue);
DEFINE_PRIMITIVE_PROPERTY_MEMBER(FromValue);
DEFINE_PRIMITIVE_PROPERTY_MEMBER(NewArray);

#undef DEFINE_PRIMITIVE_PROPERTY_MEMBER

#define DEFINE_OBJECT_PROPERTY_MEMBER(Name) \
    template<class C, \
             bool (NativeJSContainerImpl::*I)(JS::HandleValue) const, \
             typename C::LocalRef (NativeJSContainerImpl::*F)(JS::HandleValue)> \
    constexpr typename NativeJSContainerImpl \
        ::BaseObjectProperty<C, I, F>::Name##_t \
    NativeJSContainerImpl::BaseObjectProperty<C, I, F>::Name

DEFINE_OBJECT_PROPERTY_MEMBER(InValue);
DEFINE_OBJECT_PROPERTY_MEMBER(FromValue);
DEFINE_OBJECT_PROPERTY_MEMBER(NewArray);

#undef DEFINE_OBJECT_PROPERTY_MEMBER

template<class P> constexpr typename NativeJSContainerImpl::ArrayProperty<P>
        ::InValue_t NativeJSContainerImpl::ArrayProperty<P>::InValue;
template<class P> constexpr typename NativeJSContainerImpl::ArrayProperty<P>
        ::FromValue_t NativeJSContainerImpl::ArrayProperty<P>::FromValue;

constexpr NativeJSContainerImpl::HasProperty::HasValue_t
        NativeJSContainerImpl::HasProperty::InValue;
constexpr NativeJSContainerImpl::HasProperty::HasValue_t
        NativeJSContainerImpl::HasProperty::FromValue;


NativeJSContainer::LocalRef
CreateNativeJSContainer(JSContext* cx, JS::HandleObject object)
{
    return NativeJSContainerImpl::CreateInstance(cx, object);
}

} // namespace widget
} // namespace mozilla


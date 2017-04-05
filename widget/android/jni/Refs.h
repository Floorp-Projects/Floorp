#ifndef mozilla_jni_Refs_h__
#define mozilla_jni_Refs_h__

#include <jni.h>

#include "mozilla/Move.h"
#include "mozilla/jni/Utils.h"

#include "nsError.h" // for nsresult
#include "nsString.h"
#include "nsTArray.h"

namespace mozilla {
namespace jni {

// Wrapped object reference (e.g. jobject, jclass, etc...)
template<class Cls, typename JNIType> class Ref;
// Represents a calling context for JNI methods.
template<class Cls, typename JNIType> class Context;
// Wrapped local reference that inherits from Ref.
template<class Cls> class LocalRef;
// Wrapped global reference that inherits from Ref.
template<class Cls> class GlobalRef;
// Wrapped dangling reference that's owned by someone else.
template<class Cls> class DependentRef;


// Class to hold the native types of a method's arguments.
// For example, if a method has signature (ILjava/lang/String;)V,
// its arguments class would be jni::Args<int32_t, jni::String::Param>
template<typename...>
struct Args {};


class Object;

// Base class for Ref and its specializations.
template<class Cls, typename Type>
class Ref
{
    template<class C, typename T> friend class Ref;

    using Self = Ref<Cls, Type>;
    using bool_type = void (Self::*)() const;
    void non_null_reference() const {}

    // A Cls-derivative that allows copying
    // (e.g. when acting as a return value).
    struct CopyableCtx : public Context<Cls, Type>
    {
        CopyableCtx(JNIEnv* env, Type instance)
            : Context<Cls, Type>(env, instance)
        {}

        CopyableCtx(const CopyableCtx& cls)
            : Context<Cls, Type>(cls.Env(), cls.Get())
        {}
    };

    // Private copy constructor so that there's no danger of assigning a
    // temporary LocalRef/GlobalRef to a Ref, and potentially use the Ref
    // after the source had been freed.
    Ref(const Ref&) = default;

protected:
    static JNIEnv* FindEnv()
    {
        return Cls::callingThread == CallingThread::GECKO ?
                GetGeckoThreadEnv() : GetEnvForThread();
    }

    Type mInstance;

    // Protected jobject constructor because outside code should be using
    // Ref::From. Using Ref::From makes it very easy to see which code is using
    // raw JNI types for future refactoring.
    explicit Ref(Type instance) : mInstance(instance) {}

public:
    using JNIType = Type;

    class AutoLock
    {
        friend class Ref<Cls, Type>;

        JNIEnv* const mEnv;
        Type mInstance;

        AutoLock(Type aInstance)
            : mEnv(FindEnv())
            , mInstance(mEnv->NewLocalRef(aInstance))
        {
            mEnv->MonitorEnter(mInstance);
            MOZ_CATCH_JNI_EXCEPTION(mEnv);
        }

    public:
        AutoLock(AutoLock&& aOther)
            : mEnv(aOther.mEnv)
            , mInstance(aOther.mInstance)
        {
            aOther.mInstance = nullptr;
        }

        ~AutoLock()
        {
            Unlock();
        }

        void Unlock()
        {
            if (mInstance) {
                mEnv->MonitorExit(mInstance);
                mEnv->DeleteLocalRef(mInstance);
                MOZ_CATCH_JNI_EXCEPTION(mEnv);
                mInstance = nullptr;
            }
        }
    };

    // Construct a Ref form a raw JNI reference.
    static Ref<Cls, Type> From(JNIType obj)
    {
        return Ref<Cls, Type>(obj);
    }

    // Construct a Ref form a generic object reference.
    static Ref<Cls, Type> From(const Ref<Object, jobject>& obj)
    {
        return Ref<Cls, Type>(JNIType(obj.Get()));
    }

    MOZ_IMPLICIT Ref(decltype(nullptr)) : mInstance(nullptr) {}

    // Get the raw JNI reference.
    JNIType Get() const
    {
        return mInstance;
    }

    template<class T>
    bool IsInstanceOf() const
    {
        return FindEnv()->IsInstanceOf(
                mInstance, typename T::Context().ClassRef());
    }

    template<class T>
    typename T::Ref Cast() const
    {
#ifdef MOZ_CHECK_JNI
        MOZ_RELEASE_ASSERT(FindEnv()->IsAssignableFrom(
                Context<Cls, Type>().ClassRef(),
                typename T::Context().ClassRef()));
#endif
        return T::Ref::From(*this);
    }

    AutoLock Lock() const
    {
        return AutoLock(mInstance);
    }

    bool operator==(const Ref& other) const
    {
        // Treat two references of the same object as being the same.
        return mInstance == other.mInstance || JNI_FALSE !=
                FindEnv()->IsSameObject(mInstance, other.mInstance);
    }

    bool operator!=(const Ref& other) const
    {
        return !operator==(other);
    }

    bool operator==(decltype(nullptr)) const
    {
        return !mInstance;
    }

    bool operator!=(decltype(nullptr)) const
    {
        return !!mInstance;
    }

    CopyableCtx operator->() const
    {
        return CopyableCtx(FindEnv(), mInstance);
    }

    CopyableCtx operator*() const
    {
        return operator->();
    }

    // Any ref can be cast to an object ref.
    operator Ref<Object, jobject>() const
    {
        return Ref<Object, jobject>(mInstance);
    }

    // Null checking (e.g. !!ref) using the safe-bool idiom.
    operator bool_type() const
    {
        return mInstance ? &Self::non_null_reference : nullptr;
    }

    // We don't allow implicit conversion to jobject because that can lead
    // to easy mistakes such as assigning a temporary LocalRef to a jobject,
    // and using the jobject after the LocalRef has been freed.

    // We don't allow explicit conversion, to make outside code use Ref::Get.
    // Using Ref::Get makes it very easy to see which code is using raw JNI
    // types to make future refactoring easier.

    // operator JNIType() const = delete;
};


// Represents a calling context for JNI methods.
template<class Cls, typename Type>
class Context : public Ref<Cls, Type>
{
    using Ref = jni::Ref<Cls, Type>;

    static jclass sClassRef; // global reference

protected:
    JNIEnv* const mEnv;

public:
    Context()
        : Ref(nullptr)
        , mEnv(Ref::FindEnv())
    {}

    Context(JNIEnv* env, Type instance)
        : Ref(instance)
        , mEnv(env)
    {}

    jclass ClassRef() const
    {
        if (!sClassRef) {
            const jclass cls = GetClassRef(mEnv, Cls::name);
            sClassRef = jclass(mEnv->NewGlobalRef(cls));
            mEnv->DeleteLocalRef(cls);
        }
        return sClassRef;
    }

    JNIEnv* Env() const
    {
        return mEnv;
    }

    template<class T>
    bool IsInstanceOf() const
    {
        return mEnv->IsInstanceOf(
                Ref::mInstance, typename T::Context(mEnv, nullptr).ClassRef());
    }

    bool operator==(const Ref& other) const
    {
        // Treat two references of the same object as being the same.
        return Ref::mInstance == other.mInstance || JNI_FALSE !=
                mEnv->IsSameObject(Ref::mInstance, other.mInstance);
    }

    bool operator!=(const Ref& other) const
    {
        return !operator==(other);
    }

    bool operator==(decltype(nullptr)) const
    {
        return !Ref::mInstance;
    }

    bool operator!=(decltype(nullptr)) const
    {
        return !!Ref::mInstance;
    }

    Cls operator->() const
    {
        MOZ_ASSERT(Ref::mInstance, "Null jobject");
        return Cls(*this);
    }

    const Context<Cls, Type>& operator*() const
    {
        return *this;
    }
};


template<class Cls, typename Type = jobject>
class ObjectBase
{
protected:
    const jni::Context<Cls, Type>& mCtx;

    jclass ClassRef() const { return mCtx.ClassRef(); }
    JNIEnv* Env() const { return mCtx.Env(); }
    Type Instance() const { return mCtx.Get(); }

public:
    using Ref = jni::Ref<Cls, Type>;
    using Context = jni::Context<Cls, Type>;
    using LocalRef = jni::LocalRef<Cls>;
    using GlobalRef = jni::GlobalRef<Cls>;
    using Param = const Ref&;

    static const CallingThread callingThread = CallingThread::ANY;
    static const char name[];

    explicit ObjectBase(const Context& ctx) : mCtx(ctx) {}

    Cls* operator->()
    {
        return static_cast<Cls*>(this);
    }
};

// Binding for a plain jobject.
class Object : public ObjectBase<Object, jobject>
{
public:
    explicit Object(const Context& ctx) : ObjectBase<Object, jobject>(ctx) {}
};

// Binding for a built-in object reference other than jobject.
template<typename T>
class TypedObject : public ObjectBase<TypedObject<T>, T>
{
public:
    explicit TypedObject(const Context<TypedObject<T>, T>& ctx)
        : ObjectBase<TypedObject<T>, T>(ctx)
    {}
};

// Binding for a boxed primitive object.
template<typename T>
class BoxedObject : public ObjectBase<BoxedObject<T>, jobject>
{
public:
    explicit BoxedObject(const Context<BoxedObject<T>, jobject>& ctx)
        : ObjectBase<BoxedObject<T>, jobject>(ctx)
    {}
};

// Define bindings for built-in types.
using String = TypedObject<jstring>;
using Class = TypedObject<jclass>;
using Throwable = TypedObject<jthrowable>;

using Boolean = BoxedObject<jboolean>;
using Byte = BoxedObject<jbyte>;
using Character = BoxedObject<jchar>;
using Short = BoxedObject<jshort>;
using Integer = BoxedObject<jint>;
using Long = BoxedObject<jlong>;
using Float = BoxedObject<jfloat>;
using Double = BoxedObject<jdouble>;

using BooleanArray = TypedObject<jbooleanArray>;
using ByteArray = TypedObject<jbyteArray>;
using CharArray = TypedObject<jcharArray>;
using ShortArray = TypedObject<jshortArray>;
using IntArray = TypedObject<jintArray>;
using LongArray = TypedObject<jlongArray>;
using FloatArray = TypedObject<jfloatArray>;
using DoubleArray = TypedObject<jdoubleArray>;
using ObjectArray = TypedObject<jobjectArray>;


namespace detail {

// See explanation in LocalRef.
template<class Cls> struct GenericObject { using Type = Object; };
template<> struct GenericObject<Object>
{
    struct Type {
        using Ref = jni::Ref<Type, jobject>;
        using Context = jni::Context<Type, jobject>;
    };
};
template<class Cls> struct GenericLocalRef
{
    template<class C> struct Type : jni::Object {};
};
template<> struct GenericLocalRef<Object>
{
    template<class C> using Type = jni::LocalRef<C>;
};

} // namespace

template<class Cls>
class LocalRef : public Cls::Context
{
    template<class C> friend class LocalRef;

    using Ctx = typename Cls::Context;
    using Ref = typename Cls::Ref;
    using JNIType = typename Ref::JNIType;

    // In order to be able to convert LocalRef<Object> to LocalRef<Cls>, we
    // need constructors and copy assignment operators that take in a
    // LocalRef<Object> argument. However, if Cls *is* Object, we would have
    // duplicated constructors and operators with LocalRef<Object> arguments. To
    // avoid this conflict, we use GenericObject, which is defined as Object for
    // LocalRef<non-Object> and defined as a dummy class for LocalRef<Object>.
    using GenericObject = typename detail::GenericObject<Cls>::Type;

    // Similarly, GenericLocalRef is useed to convert LocalRef<Cls> to,
    // LocalRef<Object>. It's defined as LocalRef<C> for Cls == Object,
    // and defined as a dummy template class for Cls != Object.
    template<class C> using GenericLocalRef
            = typename detail::GenericLocalRef<Cls>::template Type<C>;

    static JNIType NewLocalRef(JNIEnv* env, JNIType obj)
    {
        return JNIType(obj ? env->NewLocalRef(obj) : nullptr);
    }

    LocalRef(JNIEnv* env, JNIType instance) : Ctx(env, instance) {}

    LocalRef& swap(LocalRef& other)
    {
        auto instance = other.mInstance;
        other.mInstance = Ctx::mInstance;
        Ctx::mInstance = instance;
        return *this;
    }

public:
    // Construct a LocalRef from a raw JNI local reference. Unlike Ref::From,
    // LocalRef::Adopt returns a LocalRef that will delete the local reference
    // when going out of scope.
    static LocalRef Adopt(JNIType instance)
    {
        return LocalRef(Ref::FindEnv(), instance);
    }

    static LocalRef Adopt(JNIEnv* env, JNIType instance)
    {
        return LocalRef(env, instance);
    }

    // Copy constructor.
    LocalRef(const LocalRef<Cls>& ref)
        : Ctx(ref.mEnv, NewLocalRef(ref.mEnv, ref.mInstance))
    {}

    // Move constructor.
    LocalRef(LocalRef<Cls>&& ref)
        : Ctx(ref.mEnv, ref.mInstance)
    {
        ref.mInstance = nullptr;
    }

    explicit LocalRef(JNIEnv* env = Ref::FindEnv())
        : Ctx(env, nullptr)
    {}

    // Construct a LocalRef from any Ref,
    // which means creating a new local reference.
    MOZ_IMPLICIT LocalRef(const Ref& ref)
        : Ctx(Ref::FindEnv(), nullptr)
    {
        Ctx::mInstance = NewLocalRef(Ctx::mEnv, ref.Get());
    }

    LocalRef(JNIEnv* env, const Ref& ref)
        : Ctx(env, NewLocalRef(env, ref.Get()))
    {}

    // Move a LocalRef<Object> into a LocalRef<Cls> without
    // creating/deleting local references.
    MOZ_IMPLICIT LocalRef(LocalRef<GenericObject>&& ref)
        : Ctx(ref.mEnv, JNIType(ref.mInstance))
    {
        ref.mInstance = nullptr;
    }

    template<class C>
    MOZ_IMPLICIT LocalRef(GenericLocalRef<C>&& ref)
        : Ctx(ref.mEnv, ref.mInstance)
    {
        ref.mInstance = nullptr;
    }

    // Implicitly converts nullptr to LocalRef.
    MOZ_IMPLICIT LocalRef(decltype(nullptr))
        : Ctx(Ref::FindEnv(), nullptr)
    {}

    ~LocalRef()
    {
        if (Ctx::mInstance) {
            Ctx::mEnv->DeleteLocalRef(Ctx::mInstance);
            Ctx::mInstance = nullptr;
        }
    }

    // Get the raw JNI reference that can be used as a return value.
    // Returns the same JNI type (jobject, jstring, etc.) as the underlying Ref.
    typename Ref::JNIType Forget()
    {
        const auto obj = Ctx::Get();
        Ctx::mInstance = nullptr;
        return obj;
    }

    LocalRef<Cls>& operator=(LocalRef<Cls> ref) &
    {
        return swap(ref);
    }

    LocalRef<Cls>& operator=(const Ref& ref) &
    {
        LocalRef<Cls> newRef(Ctx::mEnv, ref);
        return swap(newRef);
    }

    LocalRef<Cls>& operator=(LocalRef<GenericObject>&& ref) &
    {
        LocalRef<Cls> newRef(mozilla::Move(ref));
        return swap(newRef);
    }

    template<class C>
    LocalRef<Cls>& operator=(GenericLocalRef<C>&& ref) &
    {
        LocalRef<Cls> newRef(mozilla::Move(ref));
        return swap(newRef);
    }

    LocalRef<Cls>& operator=(decltype(nullptr)) &
    {
        LocalRef<Cls> newRef(Ctx::mEnv, nullptr);
        return swap(newRef);
    }
};


template<class Cls>
class GlobalRef : public Cls::Ref
{
    using Ref = typename Cls::Ref;
    using JNIType = typename Ref::JNIType;

    static JNIType NewGlobalRef(JNIEnv* env, JNIType instance)
    {
        return JNIType(instance ? env->NewGlobalRef(instance) : nullptr);
    }

    GlobalRef& swap(GlobalRef& other)
    {
        auto instance = other.mInstance;
        other.mInstance = Ref::mInstance;
        Ref::mInstance = instance;
        return *this;
    }

public:
    GlobalRef()
        : Ref(nullptr)
    {}

    // Copy constructor
    GlobalRef(const GlobalRef& ref)
        : Ref(NewGlobalRef(GetEnvForThread(), ref.mInstance))
    {}

    // Move constructor
    GlobalRef(GlobalRef&& ref)
        : Ref(ref.mInstance)
    {
        ref.mInstance = nullptr;
    }

    MOZ_IMPLICIT GlobalRef(const Ref& ref)
        : Ref(NewGlobalRef(GetEnvForThread(), ref.Get()))
    {}

    GlobalRef(JNIEnv* env, const Ref& ref)
        : Ref(NewGlobalRef(env, ref.Get()))
    {}

    MOZ_IMPLICIT GlobalRef(const LocalRef<Cls>& ref)
        : Ref(NewGlobalRef(ref.Env(), ref.Get()))
    {}

    // Implicitly converts nullptr to GlobalRef.
    MOZ_IMPLICIT GlobalRef(decltype(nullptr))
        : Ref(nullptr)
    {}

    ~GlobalRef()
    {
        if (Ref::mInstance) {
            Clear(GetEnvForThread());
        }
    }

    // Get the raw JNI reference that can be used as a return value.
    // Returns the same JNI type (jobject, jstring, etc.) as the underlying Ref.
    typename Ref::JNIType Forget()
    {
        const auto obj = Ref::Get();
        Ref::mInstance = nullptr;
        return obj;
    }

    void Clear(JNIEnv* env)
    {
        if (Ref::mInstance) {
            env->DeleteGlobalRef(Ref::mInstance);
            Ref::mInstance = nullptr;
        }
    }

    GlobalRef<Cls>& operator=(GlobalRef<Cls> ref) &
    {
        return swap(ref);
    }

    GlobalRef<Cls>& operator=(const Ref& ref) &
    {
        GlobalRef<Cls> newRef(ref);
        return swap(newRef);
    }

    GlobalRef<Cls>& operator=(const LocalRef<Cls>& ref) &
    {
        GlobalRef<Cls> newRef(ref);
        return swap(newRef);
    }

    GlobalRef<Cls>& operator=(decltype(nullptr)) &
    {
        GlobalRef<Cls> newRef(nullptr);
        return swap(newRef);
    }
};


template<class Cls>
class DependentRef : public Cls::Ref
{
    using Ref = typename Cls::Ref;

public:
    DependentRef(typename Ref::JNIType instance)
        : Ref(instance)
    {}

    DependentRef(const DependentRef& ref)
        : Ref(ref.Get())
    {}
};


class StringParam;

template<>
class TypedObject<jstring> : public ObjectBase<TypedObject<jstring>, jstring>
{
    using Base = ObjectBase<TypedObject<jstring>, jstring>;

public:
    using Param = const StringParam&;

    explicit TypedObject(const Context& ctx) : Base(ctx) {}

    size_t Length() const
    {
        const size_t ret = Base::Env()->GetStringLength(Base::Instance());
        MOZ_CATCH_JNI_EXCEPTION(Base::Env());
        return ret;
    }

    nsString ToString() const
    {
        const jchar* const str = Base::Env()->GetStringChars(
                Base::Instance(), nullptr);
        const jsize len = Base::Env()->GetStringLength(Base::Instance());

        nsString result(reinterpret_cast<const char16_t*>(str), len);
        Base::Env()->ReleaseStringChars(Base::Instance(), str);
        return result;
    }

    nsCString ToCString() const
    {
        return NS_ConvertUTF16toUTF8(ToString());
    }

    // Convert jstring to a nsString.
    operator nsString() const
    {
        return ToString();
    }

    // Convert jstring to a nsCString.
    operator nsCString() const
    {
        return ToCString();
    }
};

// Define a custom parameter type for String,
// which accepts both String::Ref and nsAString/nsACString
class StringParam : public String::Ref
{
    using Ref = String::Ref;

private:
    // Not null if we should delete ref on destruction.
    JNIEnv* const mEnv;

    static jstring GetString(JNIEnv* env, const nsAString& str)
    {
        const jstring result = env->NewString(
                reinterpret_cast<const jchar*>(str.BeginReading()),
                str.Length());
        MOZ_CATCH_JNI_EXCEPTION(env);
        return result;
    }

public:
    MOZ_IMPLICIT StringParam(decltype(nullptr))
        : Ref(nullptr)
        , mEnv(nullptr)
    {}

    MOZ_IMPLICIT StringParam(const Ref& ref)
        : Ref(ref.Get())
        , mEnv(nullptr)
    {}

    MOZ_IMPLICIT StringParam(const nsAString& str, JNIEnv* env = Ref::FindEnv())
        : Ref(GetString(env, str))
        , mEnv(env)
    {}

    MOZ_IMPLICIT StringParam(const nsLiteralString& str, JNIEnv* env = Ref::FindEnv())
        : Ref(GetString(env, str))
        , mEnv(env)
    {}

    MOZ_IMPLICIT StringParam(const char16_t* str, JNIEnv* env = Ref::FindEnv())
        : Ref(GetString(env, nsDependentString(str)))
        , mEnv(env)
    {}

    MOZ_IMPLICIT StringParam(const nsACString& str, JNIEnv* env = Ref::FindEnv())
        : Ref(GetString(env, NS_ConvertUTF8toUTF16(str)))
        , mEnv(env)
    {}

    MOZ_IMPLICIT StringParam(const nsLiteralCString& str, JNIEnv* env = Ref::FindEnv())
        : Ref(GetString(env, NS_ConvertUTF8toUTF16(str)))
        , mEnv(env)
    {}

    MOZ_IMPLICIT StringParam(const char* str, JNIEnv* env = Ref::FindEnv())
        : Ref(GetString(env, NS_ConvertUTF8toUTF16(str)))
        , mEnv(env)
    {}

    StringParam(StringParam&& other)
        : Ref(other.Get())
        , mEnv(other.mEnv)
    {
        other.mInstance = nullptr;
    }

    ~StringParam()
    {
        if (mEnv && Get()) {
            mEnv->DeleteLocalRef(Get());
        }
    }

    operator String::LocalRef() const
    {
        // We can't return our existing ref because the returned
        // LocalRef could be freed first, so we need a new local ref.
        return String::LocalRef(mEnv ? mEnv : Ref::FindEnv(), *this);
    }
};


namespace detail {
    template<typename T> struct TypeAdapter;
}

// Ref specialization for arrays.
template<typename JNIType, class ElementType>
class ArrayRefBase : public ObjectBase<TypedObject<JNIType>, JNIType>
{
    using Base = ObjectBase<TypedObject<JNIType>, JNIType>;

public:
    explicit ArrayRefBase(const Context<TypedObject<JNIType>, JNIType>& ctx)
        : Base(ctx)
    {}

    static typename Base::LocalRef New(const ElementType* data, size_t length) {
        using JNIElemType = typename detail::TypeAdapter<ElementType>::JNIType;
        static_assert(sizeof(ElementType) == sizeof(JNIElemType),
                      "Size of native type must match size of JNI type");
        JNIEnv* const jenv = mozilla::jni::GetEnvForThread();
        auto result =
            (jenv->*detail::TypeAdapter<ElementType>::NewArray)(length);
        MOZ_CATCH_JNI_EXCEPTION(jenv);
        (jenv->*detail::TypeAdapter<ElementType>::SetArray)(
                result, jsize(0), length,
                reinterpret_cast<const JNIElemType*>(data));
        MOZ_CATCH_JNI_EXCEPTION(jenv);
        return Base::LocalRef::Adopt(jenv, result);
    }

    size_t Length() const
    {
        const size_t ret = Base::Env()->GetArrayLength(Base::Instance());
        MOZ_CATCH_JNI_EXCEPTION(Base::Env());
        return ret;
    }

    ElementType GetElement(size_t index) const
    {
        using JNIElemType = typename detail::TypeAdapter<ElementType>::JNIType;
        static_assert(sizeof(ElementType) == sizeof(JNIElemType),
                      "Size of native type must match size of JNI type");

        ElementType ret;
        (Base::Env()->*detail::TypeAdapter<ElementType>::GetArray)(
                Base::Instance(), jsize(index), 1,
                reinterpret_cast<JNIElemType*>(&ret));
        MOZ_CATCH_JNI_EXCEPTION(Base::Env());
        return ret;
    }

    nsTArray<ElementType> GetElements() const
    {
        using JNIElemType = typename detail::TypeAdapter<ElementType>::JNIType;
        static_assert(sizeof(ElementType) == sizeof(JNIElemType),
                      "Size of native type must match size of JNI type");

        const size_t len = size_t(Base::Env()->GetArrayLength(Base::Instance()));

        nsTArray<ElementType> array(len);
        array.SetLength(len);
        CopyTo(array.Elements(), len);
        return array;
    }

    // returns number of elements copied
    size_t CopyTo(ElementType* buffer, size_t size) const
    {
        using JNIElemType = typename detail::TypeAdapter<ElementType>::JNIType;
        static_assert(sizeof(ElementType) == sizeof(JNIElemType),
                      "Size of native type must match size of JNI type");

        const size_t len = size_t(Base::Env()->GetArrayLength(Base::Instance()));
        const size_t amountToCopy = (len > size ? size : len);
        (Base::Env()->*detail::TypeAdapter<ElementType>::GetArray)(
                Base::Instance(), 0, jsize(amountToCopy),
                reinterpret_cast<JNIElemType*>(buffer));
        return amountToCopy;
    }

    ElementType operator[](size_t index) const
    {
        return GetElement(index);
    }

    operator nsTArray<ElementType>() const
    {
        return GetElements();
    }
};

#define DEFINE_PRIMITIVE_ARRAY_REF(JNIType, ElementType) \
    template<> \
    class TypedObject<JNIType> : public ArrayRefBase<JNIType, ElementType> \
    { \
    public: \
        explicit TypedObject(const Context& ctx) \
            : ArrayRefBase<JNIType, ElementType>(ctx) \
        {} \
    }

DEFINE_PRIMITIVE_ARRAY_REF(jbooleanArray, bool);
DEFINE_PRIMITIVE_ARRAY_REF(jbyteArray,    int8_t);
DEFINE_PRIMITIVE_ARRAY_REF(jcharArray,    char16_t);
DEFINE_PRIMITIVE_ARRAY_REF(jshortArray,   int16_t);
DEFINE_PRIMITIVE_ARRAY_REF(jintArray,     int32_t);
DEFINE_PRIMITIVE_ARRAY_REF(jlongArray,    int64_t);
DEFINE_PRIMITIVE_ARRAY_REF(jfloatArray,   float);
DEFINE_PRIMITIVE_ARRAY_REF(jdoubleArray,  double);

#undef DEFINE_PRIMITIVE_ARRAY_REF


class ByteBuffer : public ObjectBase<ByteBuffer, jobject>
{
public:
    explicit ByteBuffer(const Context& ctx)
        : ObjectBase<ByteBuffer, jobject>(ctx)
    {}

    static LocalRef New(void* data, size_t capacity)
    {
        JNIEnv* const env = GetEnvForThread();
        const auto ret = LocalRef::Adopt(
                env, env->NewDirectByteBuffer(data, jlong(capacity)));
        MOZ_CATCH_JNI_EXCEPTION(env);
        return ret;
    }

    void* Address()
    {
        void* const ret = Env()->GetDirectBufferAddress(Instance());
        MOZ_CATCH_JNI_EXCEPTION(Env());
        return ret;
    }

    size_t Capacity()
    {
        const size_t ret = size_t(Env()->GetDirectBufferCapacity(Instance()));
        MOZ_CATCH_JNI_EXCEPTION(Env());
        return ret;
    }
};


template<>
class TypedObject<jobjectArray>
    : public ObjectBase<TypedObject<jobjectArray>, jobjectArray>
{
    using Base = ObjectBase<TypedObject<jobjectArray>, jobjectArray>;

public:
    template<class Cls = Object>
    static Base::LocalRef New(size_t length,
                              typename Cls::Param initialElement = nullptr) {
        JNIEnv* const env = GetEnvForThread();
        jobjectArray array = env->NewObjectArray(
                jsize(length),
                typename Cls::Context(env, nullptr).ClassRef(),
                initialElement.Get());
        MOZ_CATCH_JNI_EXCEPTION(env);
        return Base::LocalRef::Adopt(env, array);
    }

    explicit TypedObject(const Context& ctx) : Base(ctx) {}

    size_t Length() const
    {
        const size_t ret = Base::Env()->GetArrayLength(Base::Instance());
        MOZ_CATCH_JNI_EXCEPTION(Base::Env());
        return ret;
    }

    Object::LocalRef GetElement(size_t index) const
    {
        auto ret = Object::LocalRef::Adopt(
                Base::Env(), Base::Env()->GetObjectArrayElement(
                    Base::Instance(), jsize(index)));
        MOZ_CATCH_JNI_EXCEPTION(Base::Env());
        return ret;
    }

    nsTArray<Object::LocalRef> GetElements() const
    {
        const jsize len = size_t(Base::Env()->GetArrayLength(Base::Instance()));

        nsTArray<Object::LocalRef> array((size_t(len)));
        for (jsize i = 0; i < len; i++) {
            array.AppendElement(Object::LocalRef::Adopt(
                    Base::Env(), Base::Env()->GetObjectArrayElement(
                        Base::Instance(), i)));
            MOZ_CATCH_JNI_EXCEPTION(Base::Env());
        }
        return array;
    }

    Object::LocalRef operator[](size_t index) const
    {
        return GetElement(index);
    }

    operator nsTArray<Object::LocalRef>() const
    {
        return GetElements();
    }

    void SetElement(size_t index, Object::Param element) const
    {
        Base::Env()->SetObjectArrayElement(
                Base::Instance(), jsize(index), element.Get());
        MOZ_CATCH_JNI_EXCEPTION(Base::Env());
    }
};


// Support conversion from LocalRef<T>* to LocalRef<Object>*:
//   LocalRef<Foo> foo;
//   Foo::GetFoo(&foo); // error because parameter type is LocalRef<Object>*.
//   Foo::GetFoo(ReturnTo(&foo)); // OK because ReturnTo converts the argument.
template<class Cls>
class ReturnToLocal
{
private:
    LocalRef<Cls>* const localRef;
    LocalRef<Object> objRef;

public:
    explicit ReturnToLocal(LocalRef<Cls>* ref) : localRef(ref) {}
    operator LocalRef<Object>*() { return &objRef; }

    ~ReturnToLocal()
    {
        if (objRef) {
            *localRef = mozilla::Move(objRef);
        }
    }
};

template<class Cls>
ReturnToLocal<Cls> ReturnTo(LocalRef<Cls>* ref)
{
    return ReturnToLocal<Cls>(ref);
}


// Support conversion from GlobalRef<T>* to LocalRef<Object/T>*:
//   GlobalRef<Foo> foo;
//   Foo::GetFoo(&foo); // error because parameter type is LocalRef<Foo>*.
//   Foo::GetFoo(ReturnTo(&foo)); // OK because ReturnTo converts the argument.
template<class Cls>
class ReturnToGlobal
{
private:
    GlobalRef<Cls>* const globalRef;
    LocalRef<Object> objRef;
    LocalRef<Cls> clsRef;

public:
    explicit ReturnToGlobal(GlobalRef<Cls>* ref) : globalRef(ref) {}
    operator LocalRef<Object>*() { return &objRef; }
    operator LocalRef<Cls>*() { return &clsRef; }

    ~ReturnToGlobal()
    {
        if (objRef) {
            *globalRef = (clsRef = mozilla::Move(objRef));
        } else if (clsRef) {
            *globalRef = clsRef;
        }
    }
};

template<class Cls>
ReturnToGlobal<Cls> ReturnTo(GlobalRef<Cls>* ref)
{
    return ReturnToGlobal<Cls>(ref);
}

} // namespace jni
} // namespace mozilla

#endif // mozilla_jni_Refs_h__

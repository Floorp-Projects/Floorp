/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/*
 * Proxy JNI implementation.
 */

#include "nscore.h"
#include "jvmmgr.h"
#include "nsISecureEnv.h"
#include "nsIJVMPlugin.h"
#include "nsHashtable.h"
#include "nsVoidArray.h"
#include "plstr.h"
#include "ProxyClassLoader.h"

#include "ProxyJNI.h"
#include "nsDataHashtable.h"

#ifdef DEBUG
#include "nsDebug.h"
#endif

// Method Signature Processing.

static jni_type get_jni_type(char sig)
{
    switch (sig) {
    case 'L':
    case '[':
        return jobject_type;
    case 'Z':
        return jboolean_type;
    case 'B':
        return jbyte_type;
    case 'C':
        return jchar_type;
    case 'S':
        return jshort_type;
    case 'I':
        return jint_type;
    case 'J':
        return jlong_type;
    case 'F':
        return jfloat_type;
    case 'D':
        return jdouble_type;
    case 'V':
        return jvoid_type;
    }
    return jvoid_type;
}

static PRBool get_method_type(const char* sig, PRUint32& arg_count, jni_type*& arg_types, jni_type& return_type)
{
    arg_count = 0;
    if (sig[0] == '(') {
        nsVoidArray vec;
        ++sig;
        while (*sig != ')' && *sig) {
            char arg_sig = *sig;
            jni_type arg_type = get_jni_type(arg_sig);
            if (arg_type == jobject_type) {
                // could be an array or an object.
                while (*sig == '[') ++sig;
                if (*sig == 'L') {
                    // skip over "className;"
                    ++sig;
                    while (*sig != ';') ++sig;
                }
            }
            // skip over scalar or ';'.
            ++sig;
            vec.AppendElement((void *) arg_type);
        }
        arg_count = vec.Count();
        arg_types = new jni_type[arg_count];
        for (int index = arg_count - 1; index >= 0; --index)
            arg_types[index] = jni_type((jint)NS_PTR_TO_INT32(vec.ElementAt(index)));
        if (*sig == ')') {
            char return_sig = *++sig;
            return_type = get_jni_type(return_sig);
        }
    }
    return PR_FALSE;
}

struct JNIMember {
    char* mName;
    char* mSignature;
    
    JNIMember(const char* name, const char* sig);
    ~JNIMember();
};

JNIMember::JNIMember(const char* name, const char* sig)
    : mName(NULL), mSignature(NULL)
{
    mName = PL_strdup(name);
    mSignature = PL_strdup(sig);
}

JNIMember::~JNIMember()
{
    PL_strfree(mName);
    PL_strfree(mSignature);
}

struct JNIField : JNIMember {
    jfieldID mFieldID;
    jni_type mFieldType;
    
    JNIField(const char* name, const char* sig, jfieldID fieldID);
};

JNIField::JNIField(const char* name, const char* sig, jfieldID fieldID)
    : JNIMember(name, sig), mFieldID(fieldID), mFieldType(get_jni_type(*sig))
{
}

struct JNIMethod : JNIMember {
    jmethodID mMethodID;
    PRUint32 mArgCount;
    jni_type* mArgTypes;
    jni_type mReturnType;
    
    JNIMethod(const char* name, const char* sig, jmethodID methodID);
    ~JNIMethod();
    
    jvalue* marshallArgs(va_list args);
};

JNIMethod::JNIMethod(const char* name, const char* sig, jmethodID methodID)
    : JNIMember(name, sig), mMethodID(methodID), mArgCount(0), mArgTypes(NULL), mReturnType(jvoid_type)
{
    get_method_type(sig, mArgCount, mArgTypes, mReturnType);
}

JNIMethod::~JNIMethod()
{
    if (mArgTypes != NULL)
        delete[] mArgTypes;
}

/**
 * Copies an argument list into a jvalue array.
 */
jvalue* JNIMethod::marshallArgs(va_list args)
{
    jvalue* jargs = NULL;
    PRUint32 argCount = mArgCount;
    if (argCount > 0) {
        jni_type* argTypes = mArgTypes;
        jargs = new jvalue[argCount];
        if (jargs != NULL) {
            for (PRUint32 i = 0; i < argCount; i++) {
                switch (argTypes[i]) {
                case jobject_type:
                    jargs[i].l = NS_STATIC_CAST(jobject, va_arg(args, void*));
                    break;
                case jboolean_type:
                    jargs[i].z = NS_STATIC_CAST(jboolean, va_arg(args, jint));
                    break;
                case jbyte_type:
                    jargs[i].b = NS_STATIC_CAST(jbyte, va_arg(args, jint));
                    break;
                case jchar_type:
                    jargs[i].c = NS_STATIC_CAST(jchar, va_arg(args, jint));
                    break;
                case jshort_type:
                    jargs[i].s = NS_STATIC_CAST(jshort, va_arg(args, jint));
                    break;
                case jint_type:
                    jargs[i].i = va_arg(args, jint);
                    break;
                case jlong_type:
                    jargs[i].j = va_arg(args, jlong);
                    break;
                case jfloat_type:
                    jargs[i].f = NS_STATIC_CAST(jfloat, va_arg(args, jdouble));
                    break;
                case jdouble_type:
                    jargs[i].d = va_arg(args, jdouble);
                    break;
                }
            }
        }
    }
    return jargs;
}

struct JavaClassMember {
    jclass clazz;
    void*  memberID;

    JavaClassMember(jclass cl, void* mID)
        { clazz = cl; memberID = mID;}
};

class JavaClassMemberKey : public PLDHashEntryHdr
{
public:
  typedef const JavaClassMember& KeyType;
  typedef const JavaClassMember* KeyTypePointer;
  
  JavaClassMemberKey(KeyTypePointer aKey) : mValue(*aKey) { }
  JavaClassMemberKey(const JavaClassMemberKey& toCopy) : mValue(toCopy.mValue) { }
  ~JavaClassMemberKey() { }

  KeyType GetKey() const { return mValue; }
  PRBool KeyEquals(KeyTypePointer aKey) const { return aKey->clazz == mValue.clazz && aKey->memberID == mValue.memberID; }

  static KeyTypePointer KeyToPointer(KeyType aKey) { return &aKey; }
  static PLDHashNumber HashKey(KeyTypePointer aKey) 
  { 
    PRUint32 v1 = NS_PTR_TO_INT32(aKey->clazz);
    PRUint32 v2 = NS_PTR_TO_INT32(aKey->memberID);
    return v1 ^ v2;
  }
  enum { ALLOW_MEMMOVE = PR_TRUE };

private:
  const JavaClassMember mValue;
};

/**
 * Marshalls a va_list into a jvalue array, and destructor automatically
 * deletes when the args go out of scope.
 */
class MarshalledArgs {
public:
    MarshalledArgs(JNIMethod* forMethod, va_list args) : mArgs(forMethod->marshallArgs(args)) {}
    ~MarshalledArgs() { if (mArgs != NULL) delete[] mArgs; }

    operator jvalue* () { return mArgs; }
    
private:
    jvalue* mArgs;
};

static jvalue kErrorValue;

class ProxyJNIEnv : public JNIEnv {
private:
    static JNINativeInterface_ theFuncs;
    static nsDataHashtable<JavaClassMemberKey, void*>* theIDTable;
    nsISecureEnv* mSecureEnv;
    nsISecurityContext* mContext;
    jbool mInProxyFindClass;

    static ProxyJNIEnv& GetProxyEnv(JNIEnv* env) { return *(ProxyJNIEnv*)env; }

    nsISecurityContext* getContext() { 
        if (!mContext) {
            return  JVM_GetJSSecurityContext();
        } else {
            mContext->AddRef();
            return mContext;
        }
    }

    static jint JNICALL GetVersion(JNIEnv* env)
    {
        jint version = 0;
        nsISecureEnv* secureEnv = GetSecureEnv(env);
        nsresult result;
        result = secureEnv->GetVersion(&version);
        return version;
    }

    static jclass JNICALL DefineClass(JNIEnv *env, const char *name, jobject loader, const jbyte *buf, jsize len)
    {
        jclass outClass = NULL;
        nsISecureEnv* secureEnv = GetSecureEnv(env);
        nsresult result;
        result = secureEnv->DefineClass(name, loader, buf, len, &outClass);
        return outClass;
    }

    static jclass JNICALL FindClass(JNIEnv *env, const char *name)
    {
        jclass outClass = NULL;
        ProxyJNIEnv& proxyEnv = GetProxyEnv(env);
        nsISecureEnv* secureEnv = proxyEnv.mSecureEnv;
        nsresult result;
        result = secureEnv->FindClass(name, &outClass);
        if ((NS_FAILED(result) || !outClass) && !proxyEnv.mInProxyFindClass) {
            proxyEnv.mInProxyFindClass = JNI_TRUE;
            outClass = ProxyFindClass(env, name);
            proxyEnv.mInProxyFindClass = JNI_FALSE;
        }
        return outClass;
    }

#ifdef JDK1_2
    static jmethodID JNICALL FromReflectedMethod(JNIEnv *env, jobject method)
    {
        return NULL;
    }
    
    static jfieldID JNICALL FromReflectedField(JNIEnv *env, jobject field)
    {
        return NULL;
    }

    static jobject JNICALL ToReflectedMethod(JNIEnv *env, jclass cls, jmethodID methodID, jboolean isStatic)
    {
        return NULL;
    }
#endif

    static jclass JNICALL GetSuperclass(JNIEnv *env, jclass sub)
    {
        nsISecureEnv* secureEnv = GetSecureEnv(env);
        jclass outSuper = NULL;
        nsresult result;
        result = secureEnv->GetSuperclass(sub, &outSuper);
        return outSuper;
    }

    static jboolean JNICALL IsAssignableFrom(JNIEnv *env, jclass sub, jclass sup)
    {
        nsISecureEnv* secureEnv = GetSecureEnv(env);
        jboolean outIsAssignable = PR_FALSE;
        nsresult result;
        result = secureEnv->IsAssignableFrom(sub, sup, &outIsAssignable);
        return outIsAssignable;
    }
    
#ifdef JDK1_2
    static jobject JNICALL ToReflectedField(JNIEnv *env, jclass cls, jfieldID fieldID, jboolean isStatic)
    {
        return NULL;
    }
#endif
    
    static jint JNICALL Throw(JNIEnv *env, jthrowable obj)
    {
        nsISecureEnv* secureEnv = GetSecureEnv(env);
        jint outStatus = PR_FALSE;
        nsresult result;
        result = secureEnv->Throw(obj, &outStatus);
        return outStatus;
    }
    
    static jint JNICALL ThrowNew(JNIEnv *env, jclass clazz, const char *msg)
    {
        nsISecureEnv* secureEnv = GetSecureEnv(env);
        jint outStatus = PR_FALSE;
        nsresult result;
        result = secureEnv->ThrowNew(clazz, msg, &outStatus);
        return outStatus;
    }
    
    static jthrowable JNICALL ExceptionOccurred(JNIEnv *env)
    {
        nsISecureEnv* secureEnv = GetSecureEnv(env);
        jthrowable outThrowable = NULL;
        nsresult result;
        result = secureEnv->ExceptionOccurred(&outThrowable);
        return outThrowable;
    }

    static void JNICALL ExceptionDescribe(JNIEnv *env)
    {
        nsISecureEnv* secureEnv = GetSecureEnv(env);
        nsresult result;
        result = secureEnv->ExceptionDescribe();
    }
    
    static void JNICALL ExceptionClear(JNIEnv *env)
    {
        nsISecureEnv* secureEnv = GetSecureEnv(env);
        nsresult result;
        result = secureEnv->ExceptionClear();
    }
    
    static void JNICALL FatalError(JNIEnv *env, const char *msg)
    {
        nsISecureEnv* secureEnv = GetSecureEnv(env);
        nsresult result;
        result = secureEnv->FatalError(msg);
    }

#ifdef JDK1_2
    static jint JNICALL PushLocalFrame(JNIEnv *env, jint capacity)
    {
        return 0;
    }
    
    static jobject JNICALL PopLocalFrame(JNIEnv *env, jobject result)
    {
        return NULL;
    }
#endif

    static jobject JNICALL NewGlobalRef(JNIEnv *env, jobject lobj)
    {
        nsISecureEnv* secureEnv = GetSecureEnv(env);
        jobject outGlobalRef = NULL;
        nsresult result;
        result = secureEnv->NewGlobalRef(lobj, &outGlobalRef);
        return outGlobalRef;
    }
    
    static void JNICALL DeleteGlobalRef(JNIEnv *env, jobject gref)
    {
        nsISecureEnv* secureEnv = GetSecureEnv(env);
        nsresult result;
        result = secureEnv->DeleteGlobalRef(gref);
    }
    
    static void JNICALL DeleteLocalRef(JNIEnv *env, jobject obj)
    {
        nsISecureEnv* secureEnv = GetSecureEnv(env);
        nsresult result;
        result = secureEnv->DeleteLocalRef(obj);
    }
    
    static jboolean JNICALL IsSameObject(JNIEnv *env, jobject obj1, jobject obj2)
    {
        nsISecureEnv* secureEnv = GetSecureEnv(env);
        jboolean outIsSameObject = PR_FALSE;
        nsresult result;
        result = secureEnv->IsSameObject(obj1, obj2, &outIsSameObject);
        return outIsSameObject;
    }

#ifdef JDK1_2
    static jobject JNICALL NewLocalRef(JNIEnv *env, jobject ref)
    {
        return NULL;
    }
    
    static jint JNICALL EnsureLocalCapacity(JNIEnv *env, jint capacity)
    {
        return -1;
    }
#endif

    static jobject JNICALL AllocObject(JNIEnv *env, jclass clazz)
    {
        nsISecureEnv* secureEnv = GetSecureEnv(env);
        jobject outObject = NULL;
        nsresult result;
        result = secureEnv->AllocObject(clazz, &outObject);
        return outObject;
    }
    
    static jobject JNICALL NewObject(JNIEnv *env, jclass clazz, jmethodID methodID, ...)
    {
        va_list args; va_start(args, methodID);
        jobject outObject = NewObjectV(env, clazz, methodID, args);
        va_end(args);
        return outObject;
    }
    
    static jobject JNICALL NewObjectV(JNIEnv *env, jclass clazz, jmethodID methodID, va_list args)
    {
        jobject outObject = NULL;
        
        // convert the va_list into an array of jvalues.
        JNIMethod* method = (JNIMethod*)methodID;
        MarshalledArgs jargs(method, args);
        ProxyJNIEnv& proxyEnv = GetProxyEnv(env);
        nsISecureEnv* secureEnv = GetSecureEnv(env);
        nsISecurityContext* securityContext = proxyEnv.getContext();
        nsresult result;
        result = secureEnv->NewObject(clazz, method->mMethodID, jargs, &outObject, securityContext);
        NS_IF_RELEASE(securityContext);
        
        return outObject;
    }

    static jobject JNICALL NewObjectA(JNIEnv *env, jclass clazz, jmethodID methodID, jvalue *args)
    {
        jobject outObject = NULL;
        ProxyJNIEnv& proxyEnv = GetProxyEnv(env);
        nsISecureEnv* secureEnv = GetSecureEnv(env);
        JNIMethod* method = (JNIMethod*)methodID;
        nsISecurityContext* securityContext = proxyEnv.getContext();
        nsresult result;
        result = secureEnv->NewObject(clazz, method->mMethodID, args, &outObject, securityContext);
        NS_IF_RELEASE(securityContext);
        return outObject;
    }

    static jclass JNICALL GetObjectClass(JNIEnv *env, jobject obj)
    {
        jclass outClass = NULL;
        nsISecureEnv* secureEnv = GetSecureEnv(env);
        nsresult result;
        result = secureEnv->GetObjectClass(obj, &outClass);
        return outClass;
    }
    
    static jboolean JNICALL IsInstanceOf(JNIEnv *env, jobject obj, jclass clazz)
    {
        nsISecureEnv* secureEnv = GetSecureEnv(env);
        jboolean outIsInstanceOf = PR_FALSE;
        nsresult result;
        result = secureEnv->IsInstanceOf(obj, clazz, &outIsInstanceOf);
        return outIsInstanceOf;
    }

    static jmethodID JNICALL GetMethodID(JNIEnv *env, jclass clazz, const char *name, const char *sig)
    {
        nsISecureEnv* secureEnv = GetSecureEnv(env);
        jmethodID outMethodID = NULL;
        nsresult result = secureEnv->GetMethodID(clazz, name, sig, &outMethodID);
        if (result == NS_OK && outMethodID != NULL) {
            JavaClassMember key(clazz, outMethodID);
            JNIMethod* method;
            PRBool bFound = theIDTable && theIDTable->Get(key, (void **)&method);
            if (!bFound) {
                method = new JNIMethod(name, sig, outMethodID);
                if (theIDTable)
                    theIDTable->Put(key, method);
            }
            outMethodID = jmethodID(method);
        }
        return outMethodID;
    }
    
    /**
     * Bottleneck methods called by method families below.
     */
    
    // Virtual Invokers.
    
    static jvalue InvokeMethod(JNIEnv *env, jobject obj, JNIMethod* method, jvalue* args)
    {
        jvalue outValue;
        ProxyJNIEnv& proxyEnv = GetProxyEnv(env);
        nsISecureEnv* secureEnv = GetSecureEnv(env);
        nsISecurityContext* securityContext = proxyEnv.getContext();
        nsresult rv = secureEnv->CallMethod(method->mReturnType, obj, method->mMethodID, args, &outValue, securityContext);
        NS_IF_RELEASE(securityContext);
        return NS_SUCCEEDED(rv) ? outValue : kErrorValue;
    }
    
    static jvalue InvokeMethod(JNIEnv *env, jobject obj, JNIMethod* method, va_list args)
    {
        // convert the va_list into an array of jvalues.
        MarshalledArgs jargs(method, args);
        return InvokeMethod(env, obj, method, jargs);
    }
    
    static void InvokeVoidMethod(JNIEnv *env, jobject obj, JNIMethod* method, jvalue* args)
    {
        jvalue unusedValue;
        ProxyJNIEnv& proxyEnv = GetProxyEnv(env);
        nsISecureEnv* secureEnv = GetSecureEnv(env);
        nsISecurityContext* securityContext = proxyEnv.getContext();
        nsresult result;
        result = secureEnv->CallMethod(jvoid_type, obj, method->mMethodID, args, &unusedValue, securityContext);
        NS_IF_RELEASE(securityContext);
    }

    static void InvokeVoidMethod(JNIEnv *env, jobject obj, JNIMethod* method, va_list args)
    {
        // convert the va_list into an array of jvalues.
        MarshalledArgs jargs(method, args);
        InvokeVoidMethod(env, obj, method, jargs);
    }

#define IMPLEMENT_METHOD_FAMILY(methodName, returnType, jvalueField) \
    static returnType JNICALL methodName(JNIEnv *env, jobject obj, jmethodID methodID, ...)                 \
    {                                                                                                       \
        va_list args; va_start(args, methodID);                                                             \
        returnType result = InvokeMethod(env, obj, (JNIMethod*)methodID, args).jvalueField;                 \
        va_end(args);                                                                                       \
        return result;                                                                                      \
    }                                                                                                       \
                                                                                                            \
    static returnType JNICALL methodName##V(JNIEnv *env, jobject obj, jmethodID methodID, va_list args)     \
    {                                                                                                       \
        return InvokeMethod(env, obj, (JNIMethod*)methodID, args).jvalueField;                              \
    }                                                                                                       \
                                                                                                            \
    static returnType JNICALL methodName##A(JNIEnv *env, jobject obj, jmethodID methodID, jvalue * args)    \
    {                                                                                                       \
        return InvokeMethod(env, obj, (JNIMethod*)methodID, args).jvalueField;                              \
    }                                                                                                       \

    // Pattern on which IMPLEMENT_METHOD_FAMILY is based.
    /*
    static jobject JNICALL CallObjectMethod(JNIEnv *env, jobject obj, jmethodID methodID, ...)
    {
        va_list args; va_start(args, methodID);
        jobject outObject = InvokeMethod(env, obj, (JNIMethod*)methodID, args).l;
        va_end(args);
        return outObject;
    }
    
    static jobject JNICALL CallObjectMethodV(JNIEnv *env, jobject obj, jmethodID methodID, va_list args)
    {
        return InvokeMethod(env, obj, (JNIMethod*)methodID, args).l;
    }
    
    static jobject JNICALL CallObjectMethodA(JNIEnv *env, jobject obj, jmethodID methodID, jvalue * args)
    {
        return InvokeMethod(env, obj, (JNIMethod*)methodID, args).l;
    }
    */
    
    IMPLEMENT_METHOD_FAMILY(CallObjectMethod, jobject, l)
    IMPLEMENT_METHOD_FAMILY(CallBooleanMethod, jboolean, z)
    IMPLEMENT_METHOD_FAMILY(CallByteMethod, jbyte, b)
    IMPLEMENT_METHOD_FAMILY(CallCharMethod, jchar, c)
    IMPLEMENT_METHOD_FAMILY(CallShortMethod, jshort, s)
    IMPLEMENT_METHOD_FAMILY(CallIntMethod, jint, i)
    IMPLEMENT_METHOD_FAMILY(CallLongMethod, jlong, j)
    IMPLEMENT_METHOD_FAMILY(CallFloatMethod, jfloat, f)
    IMPLEMENT_METHOD_FAMILY(CallDoubleMethod, jdouble, d)

#undef IMPLEMENT_METHOD_FAMILY

    static void JNICALL CallVoidMethod(JNIEnv *env, jobject obj, jmethodID methodID, ...)
    {
        va_list args; va_start(args, methodID);
        InvokeVoidMethod(env, obj, (JNIMethod*)methodID, args);
        va_end(args);
    }
    
    static void JNICALL CallVoidMethodV(JNIEnv *env, jobject obj, jmethodID methodID, va_list args)
    {
        InvokeVoidMethod(env, obj, (JNIMethod*)methodID, args);
    }
    
    static void JNICALL CallVoidMethodA(JNIEnv *env, jobject obj, jmethodID methodID, jvalue * args)
    {
        InvokeVoidMethod(env, obj, (JNIMethod*)methodID, args);
    }

    // Non-virtual Invokers.

    static jvalue InvokeNonVirtualMethod(JNIEnv *env, jobject obj, jclass clazz, JNIMethod* method, jvalue* args)
    {
        jvalue outValue;
        ProxyJNIEnv& proxyEnv = GetProxyEnv(env);
        nsISecureEnv* secureEnv = GetSecureEnv(env);
        nsISecurityContext* securityContext = proxyEnv.getContext();
        nsresult rv = secureEnv->CallNonvirtualMethod(method->mReturnType, obj, clazz, method->mMethodID, args, &outValue, securityContext);
        NS_IF_RELEASE(securityContext);
        return NS_SUCCEEDED(rv) ? outValue : kErrorValue;
    }
    
    static jvalue InvokeNonVirtualMethod(JNIEnv *env, jobject obj, jclass clazz, JNIMethod* method, va_list args)
    {
        // convert the va_list into an array of jvalues.
        MarshalledArgs jargs(method, args);
        return InvokeNonVirtualMethod(env, obj, clazz, method, jargs);
    }
    
    static void InvokeNonVirtualVoidMethod(JNIEnv *env, jobject obj, jclass clazz, JNIMethod* method, jvalue* args)
    {
        jvalue unusedValue;
        ProxyJNIEnv& proxyEnv = GetProxyEnv(env);
        nsISecureEnv* secureEnv = GetSecureEnv(env);
        nsISecurityContext* securityContext = proxyEnv.getContext();
        nsresult result;
        result = secureEnv->CallNonvirtualMethod(jvoid_type, obj, clazz, method->mMethodID, args, &unusedValue, securityContext);
        NS_IF_RELEASE(securityContext);
    }

    static void InvokeNonVirtualVoidMethod(JNIEnv *env, jobject obj, jclass clazz, JNIMethod* method, va_list args)
    {
        // convert the va_list into an array of jvalues.
        MarshalledArgs jargs(method, args);
        InvokeNonVirtualVoidMethod(env, obj, clazz, method, jargs);
    }

#define IMPLEMENT_METHOD_FAMILY(methodName, returnType, jvalueField) \
    static returnType JNICALL methodName(JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, ...)               \
    {                                                                                                                   \
        va_list args; va_start(args, methodID);                                                                         \
        returnType result = InvokeNonVirtualMethod(env, obj, clazz, (JNIMethod*)methodID, args).jvalueField;            \
        va_end(args);                                                                                                   \
        return result;                                                                                                  \
    }                                                                                                                   \
                                                                                                                        \
    static returnType JNICALL methodName##V(JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, va_list args)   \
    {                                                                                                                   \
        return InvokeNonVirtualMethod(env, obj, clazz, (JNIMethod*)methodID, args).jvalueField;                         \
    }                                                                                                                   \
                                                                                                                        \
    static returnType JNICALL methodName##A(JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, jvalue * args)  \
    {                                                                                                                   \
        return InvokeNonVirtualMethod(env, obj, clazz, (JNIMethod*)methodID, args).jvalueField;                         \
    }                                                                                                                   \

    IMPLEMENT_METHOD_FAMILY(CallNonvirtualObjectMethod, jobject, l)
    IMPLEMENT_METHOD_FAMILY(CallNonvirtualBooleanMethod, jboolean, z)
    IMPLEMENT_METHOD_FAMILY(CallNonvirtualByteMethod, jbyte, b)
    IMPLEMENT_METHOD_FAMILY(CallNonvirtualCharMethod, jchar, c)
    IMPLEMENT_METHOD_FAMILY(CallNonvirtualShortMethod, jshort, s)
    IMPLEMENT_METHOD_FAMILY(CallNonvirtualIntMethod, jint, i)
    IMPLEMENT_METHOD_FAMILY(CallNonvirtualLongMethod, jlong, j)
    IMPLEMENT_METHOD_FAMILY(CallNonvirtualFloatMethod, jfloat, f)
    IMPLEMENT_METHOD_FAMILY(CallNonvirtualDoubleMethod, jdouble, d)

#undef IMPLEMENT_METHOD_FAMILY

    static void JNICALL CallNonvirtualVoidMethod(JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, ...)
    {
        va_list args; va_start(args, methodID);
        InvokeNonVirtualVoidMethod(env, obj, clazz, (JNIMethod*)methodID, args);
        va_end(args);
    }
    
    static void JNICALL CallNonvirtualVoidMethodV(JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, va_list args)
    {
        InvokeNonVirtualVoidMethod(env, obj, clazz, (JNIMethod*)methodID, args);
    }
    
    static void JNICALL CallNonvirtualVoidMethodA(JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, jvalue * args)
    {
        InvokeNonVirtualVoidMethod(env, obj, clazz, (JNIMethod*)methodID, args);
    }

    // Instance Fields

    static jfieldID JNICALL GetFieldID(JNIEnv *env, jclass clazz, const char *name, const char *sig)
    {
        nsISecureEnv* secureEnv = GetSecureEnv(env);
        jfieldID outFieldID = NULL;
        nsresult result = secureEnv->GetFieldID(clazz, name, sig, &outFieldID);
        if (result == NS_OK && outFieldID != NULL) {
            JavaClassMember key(clazz, outFieldID);
            JNIField* field;
            PRBool bFound = theIDTable && theIDTable->Get(key, (void **)&field);
            if (!bFound) {
                field = new JNIField(name, sig, outFieldID);
                if (theIDTable)
                    theIDTable->Put(key, field);
            }
            outFieldID = jfieldID(field);
        }
        return outFieldID;
    }

    static jvalue GetField(JNIEnv* env, jobject obj, JNIField* field)
    {
        jvalue outValue;
        ProxyJNIEnv& proxyEnv = GetProxyEnv(env);
        nsISecureEnv* secureEnv = GetSecureEnv(env);
        nsISecurityContext* securityContext = proxyEnv.getContext();
        nsresult rv = secureEnv->GetField(field->mFieldType, obj, field->mFieldID, &outValue, securityContext);
        NS_IF_RELEASE(securityContext);
        return NS_SUCCEEDED(rv) ? outValue : kErrorValue;
    }

#define IMPLEMENT_GET_FIELD(methodName, returnType, jvalueField)                            \
    static returnType JNICALL methodName(JNIEnv *env, jobject obj, jfieldID fieldID)        \
    {                                                                                       \
        return GetField(env, obj, (JNIField*)fieldID).jvalueField;                          \
    }                                                                                       \

    IMPLEMENT_GET_FIELD(GetObjectField, jobject, l)
    IMPLEMENT_GET_FIELD(GetBooleanField, jboolean, z)
    IMPLEMENT_GET_FIELD(GetByteField, jbyte, b)
    IMPLEMENT_GET_FIELD(GetCharField, jchar, c)
    IMPLEMENT_GET_FIELD(GetShortField, jshort, s)
    IMPLEMENT_GET_FIELD(GetIntField, jint, i)
    IMPLEMENT_GET_FIELD(GetLongField, jlong, j)
    IMPLEMENT_GET_FIELD(GetFloatField, jfloat, f)
    IMPLEMENT_GET_FIELD(GetDoubleField, jdouble, d)

#undef IMPLEMENT_GET_FIELD

    static void SetField(JNIEnv* env, jobject obj, JNIField* field, jvalue value)
    {
        ProxyJNIEnv& proxyEnv = GetProxyEnv(env);
        nsISecureEnv* secureEnv = GetSecureEnv(env);
        nsISecurityContext* securityContext = proxyEnv.getContext();
        nsresult result;
        result = secureEnv->SetField(field->mFieldType, obj, field->mFieldID, value, securityContext);
        NS_IF_RELEASE(securityContext);
    }

#define IMPLEMENT_SET_FIELD(methodName, fieldType, jvalueField)                                     \
    static void JNICALL methodName(JNIEnv *env, jobject obj, jfieldID fieldID, fieldType value)     \
    {                                                                                               \
        jvalue fieldValue;                                                                          \
        fieldValue.jvalueField = value;                                                             \
        SetField(env, obj, (JNIField*)fieldID, fieldValue);                                         \
    }                                                                                               \

    IMPLEMENT_SET_FIELD(SetObjectField, jobject, l)
    IMPLEMENT_SET_FIELD(SetBooleanField, jboolean, z)
    IMPLEMENT_SET_FIELD(SetByteField, jbyte, b)
    IMPLEMENT_SET_FIELD(SetCharField, jchar, c)
    IMPLEMENT_SET_FIELD(SetShortField, jshort, s)
    IMPLEMENT_SET_FIELD(SetIntField, jint, i)
    IMPLEMENT_SET_FIELD(SetLongField, jlong, j)
    IMPLEMENT_SET_FIELD(SetFloatField, jfloat, f)
    IMPLEMENT_SET_FIELD(SetDoubleField, jdouble, d)

#undef IMPLEMENT_SET_FIELD

    // Static Methods

    static jmethodID JNICALL GetStaticMethodID(JNIEnv *env, jclass clazz, const char *name, const char *sig)
    {
        nsISecureEnv* secureEnv = GetSecureEnv(env);
        jmethodID outMethodID = NULL;
        nsresult result = secureEnv->GetStaticMethodID(clazz, name, sig, &outMethodID);
        if (result == NS_OK && outMethodID != NULL) {
            JavaClassMember key(clazz, outMethodID);
            JNIMethod* method;
            PRBool bFound = theIDTable && theIDTable->Get(key, (void **)&method);
            if (!bFound) {
                method = new JNIMethod(name, sig, outMethodID);
                if (theIDTable)
                    theIDTable->Put(key, method);
            }
            outMethodID = jmethodID(method);
        }
        return outMethodID;
    }

    // jobject (JNICALL *CallStaticObjectMethod) (JNIEnv *env, jclass clazz, jmethodID methodID, ...);
    
    // Static Invokers.
    
    static jvalue InvokeStaticMethod(JNIEnv *env, jclass clazz, JNIMethod* method, jvalue* args)
    {
        jvalue outValue;
        ProxyJNIEnv& proxyEnv = GetProxyEnv(env);
        nsISecureEnv* secureEnv = GetSecureEnv(env);
        nsISecurityContext* securityContext = proxyEnv.getContext();
        nsresult rv = secureEnv->CallStaticMethod(method->mReturnType, clazz, method->mMethodID, args, &outValue, securityContext);
        NS_IF_RELEASE(securityContext);
        return NS_SUCCEEDED(rv) ? outValue : kErrorValue;
    }
    
    static jvalue InvokeStaticMethod(JNIEnv *env, jclass clazz, JNIMethod* method, va_list args)
    {
        // convert the va_list into an array of jvalues.
        MarshalledArgs jargs(method, args);
        return InvokeStaticMethod(env, clazz, method, jargs);
    }
    
    static void InvokeStaticVoidMethod(JNIEnv *env, jclass clazz, JNIMethod* method, jvalue* args)
    {
        jvalue unusedValue;
        ProxyJNIEnv& proxyEnv = GetProxyEnv(env);
        nsISecureEnv* secureEnv = GetSecureEnv(env);
        nsISecurityContext* securityContext = proxyEnv.getContext();
        nsresult result;
        result = secureEnv->CallStaticMethod(jvoid_type, clazz, method->mMethodID, args, &unusedValue, securityContext);
        NS_IF_RELEASE(securityContext);
    }

    static void InvokeStaticVoidMethod(JNIEnv *env, jclass clazz, JNIMethod* method, va_list args)
    {
        // convert the va_list into an array of jvalues.
        MarshalledArgs jargs(method, args);
        InvokeStaticVoidMethod(env, clazz, method, jargs);
    }

#define IMPLEMENT_METHOD_FAMILY(methodName, returnType, jvalueField) \
    static returnType JNICALL methodName(JNIEnv *env, jclass clazz, jmethodID methodID, ...)                \
    {                                                                                                       \
        va_list args; va_start(args, methodID);                                                             \
        returnType result = InvokeStaticMethod(env, clazz, (JNIMethod*)methodID, args).jvalueField;         \
        va_end(args);                                                                                       \
        return result;                                                                                      \
    }                                                                                                       \
                                                                                                            \
    static returnType JNICALL methodName##V(JNIEnv *env, jclass clazz, jmethodID methodID, va_list args)    \
    {                                                                                                       \
        return InvokeStaticMethod(env, clazz, (JNIMethod*)methodID, args).jvalueField;                      \
    }                                                                                                       \
                                                                                                            \
    static returnType JNICALL methodName##A(JNIEnv *env, jclass clazz, jmethodID methodID, jvalue * args)   \
    {                                                                                                       \
        return InvokeStaticMethod(env, clazz, (JNIMethod*)methodID, args).jvalueField;                      \
    }                                                                                                       \

    IMPLEMENT_METHOD_FAMILY(CallStaticObjectMethod, jobject, l)
    IMPLEMENT_METHOD_FAMILY(CallStaticBooleanMethod, jboolean, z)
    IMPLEMENT_METHOD_FAMILY(CallStaticByteMethod, jbyte, b)
    IMPLEMENT_METHOD_FAMILY(CallStaticCharMethod, jchar, c)
    IMPLEMENT_METHOD_FAMILY(CallStaticShortMethod, jshort, s)
    IMPLEMENT_METHOD_FAMILY(CallStaticIntMethod, jint, i)
    IMPLEMENT_METHOD_FAMILY(CallStaticLongMethod, jlong, j)
    IMPLEMENT_METHOD_FAMILY(CallStaticFloatMethod, jfloat, f)
    IMPLEMENT_METHOD_FAMILY(CallStaticDoubleMethod, jdouble, d)

#undef IMPLEMENT_METHOD_FAMILY

    static void JNICALL CallStaticVoidMethod(JNIEnv *env, jclass clazz, jmethodID methodID, ...)
    {
        va_list args; va_start(args, methodID);
        InvokeStaticVoidMethod(env, clazz, (JNIMethod*)methodID, args);
        va_end(args);
    }
    
    static void JNICALL CallStaticVoidMethodV(JNIEnv *env, jclass clazz, jmethodID methodID, va_list args)
    {
        InvokeStaticVoidMethod(env, clazz, (JNIMethod*)methodID, args);
    }
    
    static void JNICALL CallStaticVoidMethodA(JNIEnv *env, jclass clazz, jmethodID methodID, jvalue * args)
    {
        InvokeStaticVoidMethod(env, clazz, (JNIMethod*)methodID, args);
    }

    // Static Fields

    static jfieldID JNICALL GetStaticFieldID(JNIEnv *env, jclass clazz, const char *name, const char *sig)
    {
        nsISecureEnv* secureEnv = GetSecureEnv(env);
        jfieldID outFieldID = NULL;
        nsresult result = secureEnv->GetStaticFieldID(clazz, name, sig, &outFieldID);
        if (result == NS_OK && outFieldID != NULL) {
            JavaClassMember key(clazz, outFieldID);
            JNIField* field;
            PRBool bFound = theIDTable && theIDTable->Get(key, (void **)&field);
            if (!bFound) {
                field = new JNIField(name, sig, outFieldID);
                if (theIDTable)
                    theIDTable->Put(key, field);
            }
            outFieldID = jfieldID(field);
        }
        return outFieldID;
    }

    static jvalue GetStaticField(JNIEnv* env, jclass clazz, JNIField* field)
    {
        jvalue outValue;
        ProxyJNIEnv& proxyEnv = GetProxyEnv(env);
        nsISecureEnv* secureEnv = GetSecureEnv(env);
        nsISecurityContext* securityContext = proxyEnv.getContext();
        nsresult rv = secureEnv->GetStaticField(field->mFieldType, clazz, field->mFieldID, &outValue, securityContext);
        NS_IF_RELEASE(securityContext);
        return NS_SUCCEEDED(rv) ? outValue : kErrorValue;
    }

#define IMPLEMENT_GET_FIELD(methodName, returnType, jvalueField)                            \
    static returnType JNICALL methodName(JNIEnv *env, jclass clazz, jfieldID fieldID)       \
    {                                                                                       \
        return GetStaticField(env, clazz, (JNIField*)fieldID).jvalueField;                  \
    }                                                                                       \

    IMPLEMENT_GET_FIELD(GetStaticObjectField, jobject, l)
    IMPLEMENT_GET_FIELD(GetStaticBooleanField, jboolean, z)
    IMPLEMENT_GET_FIELD(GetStaticByteField, jbyte, b)
    IMPLEMENT_GET_FIELD(GetStaticCharField, jchar, c)
    IMPLEMENT_GET_FIELD(GetStaticShortField, jshort, s)
    IMPLEMENT_GET_FIELD(GetStaticIntField, jint, i)
    IMPLEMENT_GET_FIELD(GetStaticLongField, jlong, j)
    IMPLEMENT_GET_FIELD(GetStaticFloatField, jfloat, f)
    IMPLEMENT_GET_FIELD(GetStaticDoubleField, jdouble, d)

#undef IMPLEMENT_GET_FIELD

    static void SetStaticField(JNIEnv* env, jclass clazz, JNIField* field, jvalue value)
    {
        ProxyJNIEnv& proxyEnv = GetProxyEnv(env);
        nsISecureEnv* secureEnv = GetSecureEnv(env);
        nsISecurityContext* securityContext = proxyEnv.getContext();
        nsresult result;
        result = secureEnv->SetStaticField(field->mFieldType, clazz, field->mFieldID, value, securityContext);
        NS_IF_RELEASE(securityContext);
    }

#define IMPLEMENT_SET_FIELD(methodName, fieldType, jvalueField)                                     \
    static void JNICALL methodName(JNIEnv *env, jclass clazz, jfieldID fieldID, fieldType value)    \
    {                                                                                               \
        jvalue fieldValue;                                                                          \
        fieldValue.jvalueField = value;                                                             \
        SetStaticField(env, clazz, (JNIField*)fieldID, fieldValue);                                 \
    }                                                                                               \

    IMPLEMENT_SET_FIELD(SetStaticObjectField, jobject, l)
    IMPLEMENT_SET_FIELD(SetStaticBooleanField, jboolean, z)
    IMPLEMENT_SET_FIELD(SetStaticByteField, jbyte, b)
    IMPLEMENT_SET_FIELD(SetStaticCharField, jchar, c)
    IMPLEMENT_SET_FIELD(SetStaticShortField, jshort, s)
    IMPLEMENT_SET_FIELD(SetStaticIntField, jint, i)
    IMPLEMENT_SET_FIELD(SetStaticLongField, jlong, j)
    IMPLEMENT_SET_FIELD(SetStaticFloatField, jfloat, f)
    IMPLEMENT_SET_FIELD(SetStaticDoubleField, jdouble, d)

#undef IMPLEMENT_SET_FIELD

    static jstring JNICALL NewString(JNIEnv *env, const jchar *unicode, jsize len)
    {
        jstring outString;
        nsISecureEnv* secureEnv = GetSecureEnv(env);
        nsresult result;
        result = secureEnv->NewString(unicode, len, &outString);
        return outString;
    }
    
    static jsize JNICALL GetStringLength(JNIEnv *env, jstring str)
    {
        jsize outLength;
        nsISecureEnv* secureEnv = GetSecureEnv(env);
        nsresult result;
        result = secureEnv->GetStringLength(str, &outLength);
        return outLength;
    }
    
    static const jchar* JNICALL GetStringChars(JNIEnv *env, jstring str, jboolean *isCopy)
    {
        const jchar* outChars = NULL;
        nsISecureEnv* secureEnv = GetSecureEnv(env);
        nsresult result;
        result = secureEnv->GetStringChars(str, isCopy, &outChars);
        return outChars;
    }
    
    static void JNICALL ReleaseStringChars(JNIEnv *env, jstring str, const jchar *chars)
    {
        nsISecureEnv* secureEnv = GetSecureEnv(env);
        nsresult result;
        result = secureEnv->ReleaseStringChars(str, chars);
    }

    static jstring JNICALL NewStringUTF(JNIEnv *env, const char *utf)
    {
        jstring outString;
        nsISecureEnv* secureEnv = GetSecureEnv(env);
        nsresult result;
        result = secureEnv->NewStringUTF(utf, &outString);
        return outString;
    }
    
    static jsize JNICALL GetStringUTFLength(JNIEnv *env, jstring str)
    {
        jsize outLength;
        nsISecureEnv* secureEnv = GetSecureEnv(env);
        nsresult result;
        result = secureEnv->GetStringUTFLength(str, &outLength);
        return outLength;
    }
    
    static const char* JNICALL GetStringUTFChars(JNIEnv *env, jstring str, jboolean *isCopy)
    {
        const char* outChars = NULL;
        nsISecureEnv* secureEnv = GetSecureEnv(env);
        nsresult result;
        result = secureEnv->GetStringUTFChars(str, isCopy, &outChars);
        return outChars;
    }
    
    static void JNICALL ReleaseStringUTFChars(JNIEnv *env, jstring str, const char* chars)
    {
        nsISecureEnv* secureEnv = GetSecureEnv(env);
        nsresult result;
        result = secureEnv->ReleaseStringUTFChars(str, chars);
    }

    static jsize JNICALL GetArrayLength(JNIEnv *env, jarray array)
    {
        jsize outLength;
        nsISecureEnv* secureEnv = GetSecureEnv(env);
        nsresult result;
        result = secureEnv->GetArrayLength(array, &outLength);
        return outLength;
    }

    static jobjectArray JNICALL NewObjectArray(JNIEnv *env, jsize len, jclass clazz, jobject initVal)
    {
        jobjectArray outArray = NULL;
        nsISecureEnv* secureEnv = GetSecureEnv(env);
        nsresult result;
        result = secureEnv->NewObjectArray(len, clazz, initVal, &outArray);
        return outArray;
    }

    static jobject JNICALL GetObjectArrayElement(JNIEnv *env, jobjectArray array, jsize index)
    {
        jobject outObject = NULL;
        nsISecureEnv* secureEnv = GetSecureEnv(env);
        nsresult result;
        result = secureEnv->GetObjectArrayElement(array, index, &outObject);
        return outObject;
    }

    static void JNICALL SetObjectArrayElement(JNIEnv *env, jobjectArray array, jsize index, jobject val)
    {
        nsISecureEnv* secureEnv = GetSecureEnv(env);
        nsresult result;
        result = secureEnv->SetObjectArrayElement(array, index, val);
    }

#define IMPLEMENT_NEW_ARRAY(methodName, type)                                                       \
    static type##Array JNICALL methodName(JNIEnv *env, jsize len)                                   \
    {                                                                                               \
        type##Array outArray = NULL;                                                                \
        nsISecureEnv* secureEnv = GetSecureEnv(env);                                                \
        nsresult result; \
        result = secureEnv->NewArray(type##_type, len, (jarray*)&outArray);             \
        return outArray;                                                                            \
    }                                                                                               \

    IMPLEMENT_NEW_ARRAY(NewBooleanArray, jboolean)
    IMPLEMENT_NEW_ARRAY(NewByteArray, jbyte)
    IMPLEMENT_NEW_ARRAY(NewCharArray, jchar)
    IMPLEMENT_NEW_ARRAY(NewShortArray, jshort)
    IMPLEMENT_NEW_ARRAY(NewIntArray, jint)
    IMPLEMENT_NEW_ARRAY(NewLongArray, jlong)
    IMPLEMENT_NEW_ARRAY(NewFloatArray, jfloat)
    IMPLEMENT_NEW_ARRAY(NewDoubleArray, jdouble)

#undef IMPLEMENT_NEW_ARRAY

#define IMPLEMENT_GET_ARRAY_ELEMENTS(methodName, type)                                                      \
    static type* JNICALL methodName(JNIEnv *env, type##Array array, jboolean *isCopy)                       \
    {                                                                                                       \
        type* outElements = NULL;                                                                           \
        nsISecureEnv* secureEnv = GetSecureEnv(env);                                                        \
        nsresult result;    \
        result = secureEnv->GetArrayElements(type##_type, array, isCopy, &outElements);         \
        return outElements;                                                                                 \
    }                                                                                                       \

    IMPLEMENT_GET_ARRAY_ELEMENTS(GetBooleanArrayElements, jboolean)
    IMPLEMENT_GET_ARRAY_ELEMENTS(GetByteArrayElements, jbyte)
    IMPLEMENT_GET_ARRAY_ELEMENTS(GetCharArrayElements, jchar)
    IMPLEMENT_GET_ARRAY_ELEMENTS(GetShortArrayElements, jshort)
    IMPLEMENT_GET_ARRAY_ELEMENTS(GetIntArrayElements, jint)
    IMPLEMENT_GET_ARRAY_ELEMENTS(GetLongArrayElements, jlong)
    IMPLEMENT_GET_ARRAY_ELEMENTS(GetFloatArrayElements, jfloat)
    IMPLEMENT_GET_ARRAY_ELEMENTS(GetDoubleArrayElements, jdouble)

#undef IMPLEMENT_GET_ARRAY_ELEMENTS

#define IMPLEMENT_RELEASE_ARRAY_ELEMENTS(methodName, type)                                                  \
    static void JNICALL methodName(JNIEnv *env, type##Array array, type* elems, jint mode)                  \
    {                                                                                                       \
        nsISecureEnv* secureEnv = GetSecureEnv(env);                                                        \
        nsresult result;    \
        result = secureEnv->ReleaseArrayElements(type##_type, array, elems, mode);                  \
    }                                                                                                       \

    IMPLEMENT_RELEASE_ARRAY_ELEMENTS(ReleaseBooleanArrayElements, jboolean)
    IMPLEMENT_RELEASE_ARRAY_ELEMENTS(ReleaseByteArrayElements, jbyte)
    IMPLEMENT_RELEASE_ARRAY_ELEMENTS(ReleaseCharArrayElements, jchar)
    IMPLEMENT_RELEASE_ARRAY_ELEMENTS(ReleaseShortArrayElements, jshort)
    IMPLEMENT_RELEASE_ARRAY_ELEMENTS(ReleaseIntArrayElements, jint)
    IMPLEMENT_RELEASE_ARRAY_ELEMENTS(ReleaseLongArrayElements, jlong)
    IMPLEMENT_RELEASE_ARRAY_ELEMENTS(ReleaseFloatArrayElements, jfloat)
    IMPLEMENT_RELEASE_ARRAY_ELEMENTS(ReleaseDoubleArrayElements, jdouble)

#undef IMPLEMENT_RELEASE_ARRAY_ELEMENTS

#define IMPLEMENT_GET_ARRAY_REGION(methodName, type)                                                        \
    static void JNICALL methodName(JNIEnv *env, type##Array array, jsize start, jsize len, type* buf)       \
    {                                                                                                       \
        nsISecureEnv* secureEnv = GetSecureEnv(env);                                                        \
        nsresult result;    \
        result = secureEnv->GetArrayRegion(type##_type, array, start, len, buf);                    \
    }                                                                                                       \

    IMPLEMENT_GET_ARRAY_REGION(GetBooleanArrayRegion, jboolean)
    IMPLEMENT_GET_ARRAY_REGION(GetByteArrayRegion, jbyte)
    IMPLEMENT_GET_ARRAY_REGION(GetCharArrayRegion, jchar)
    IMPLEMENT_GET_ARRAY_REGION(GetShortArrayRegion, jshort)
    IMPLEMENT_GET_ARRAY_REGION(GetIntArrayRegion, jint)
    IMPLEMENT_GET_ARRAY_REGION(GetLongArrayRegion, jlong)
    IMPLEMENT_GET_ARRAY_REGION(GetFloatArrayRegion, jfloat)
    IMPLEMENT_GET_ARRAY_REGION(GetDoubleArrayRegion, jdouble)

#undef IMPLEMENT_GET_ARRAY_REGION

#define IMPLEMENT_SET_ARRAY_REGION(methodName, type)                                                        \
    static void JNICALL methodName(JNIEnv *env, type##Array array, jsize start, jsize len, type* buf)       \
    {                                                                                                       \
        nsISecureEnv* secureEnv = GetSecureEnv(env);                                                        \
        nsresult result;    \
        result = secureEnv->SetArrayRegion(type##_type, array, start, len, buf);                    \
    }                                                                                                       \

    IMPLEMENT_SET_ARRAY_REGION(SetBooleanArrayRegion, jboolean)
    IMPLEMENT_SET_ARRAY_REGION(SetByteArrayRegion, jbyte)
    IMPLEMENT_SET_ARRAY_REGION(SetCharArrayRegion, jchar)
    IMPLEMENT_SET_ARRAY_REGION(SetShortArrayRegion, jshort)
    IMPLEMENT_SET_ARRAY_REGION(SetIntArrayRegion, jint)
    IMPLEMENT_SET_ARRAY_REGION(SetLongArrayRegion, jlong)
    IMPLEMENT_SET_ARRAY_REGION(SetFloatArrayRegion, jfloat)
    IMPLEMENT_SET_ARRAY_REGION(SetDoubleArrayRegion, jdouble)

#undef IMPLEMENT_SET_ARRAY_REGION

    static jint JNICALL RegisterNatives(JNIEnv *env, jclass clazz, const JNINativeMethod *methods, jint nMethods)
    {
        jint outStatus = -1;
        nsISecureEnv* secureEnv = GetSecureEnv(env);
        nsresult result;
        result = secureEnv->RegisterNatives(clazz, methods, nMethods, &outStatus);
        return outStatus;
    }
    
    static jint JNICALL UnregisterNatives(JNIEnv *env, jclass clazz)
    {
        jint outStatus = -1;
        nsISecureEnv* secureEnv = GetSecureEnv(env);
        nsresult result;
        result = secureEnv->UnregisterNatives(clazz, &outStatus);
        return outStatus;
    }

    static jint JNICALL MonitorEnter(JNIEnv *env, jobject obj)
    {
        jint outStatus = -1;
        nsISecureEnv* secureEnv = GetSecureEnv(env);
        nsresult result;
        result = secureEnv->MonitorEnter(obj, &outStatus);
        return outStatus;
    }
    
    static jint JNICALL JNICALL MonitorExit(JNIEnv *env, jobject obj)
    {
        jint outStatus = -1;
        nsISecureEnv* secureEnv = GetSecureEnv(env);
        nsresult result;
        result = secureEnv->MonitorExit(obj, &outStatus);
        return outStatus;
    }

    static jint JNICALL GetJavaVM(JNIEnv *env, JavaVM **vm)
    {
        jint outStatus = -1;
        nsISecureEnv* secureEnv = GetSecureEnv(env);
        nsresult result;
        result = secureEnv->GetJavaVM(vm, &outStatus);
        return outStatus;
    }

public:
    ProxyJNIEnv(nsIJVMPlugin* jvmPlugin, nsISecureEnv* secureEnv);
    ~ProxyJNIEnv();
    
    nsISecureEnv* getSecureEnv() { return mSecureEnv; }
    void SetSecurityContext(nsISecurityContext *context) { 
        if (mContext)
            mContext->Release();
        mContext = context;
        mContext->AddRef();
    }
    
    nsresult GetSecurityContext(nsISecurityContext **context)
    {
        if (!context)
            return NS_ERROR_FAILURE;

        *context = getContext();
        return NS_OK;
    }
};

JNINativeInterface_ ProxyJNIEnv::theFuncs = {
    NULL,   // void *reserved0;
    NULL,   // void *reserved1;
    NULL,   // void *reserved2;

    NULL,   // void *reserved3;

    // jint (JNICALL *GetVersion)(JNIEnv *env); 
    &GetVersion,

    // jclass (JNICALL *DefineClass) (JNIEnv *env, const char *name, jobject loader, const jbyte *buf, jsize len);
    &DefineClass,
    
    // jclass (JNICALL *FindClass) (JNIEnv *env, const char *name);
    &FindClass,

#ifdef JDK1_2
    // jmethodID (JNICALL *FromReflectedMethod) (JNIEnv *env, jobject method);
    &FromReflectedMethod,
    
    // jfieldID (JNICALL *FromReflectedField) (JNIEnv *env, jobject field);
    &FromReflectedField,

    // jobject (JNICALL *ToReflectedMethod) (JNIEnv *env, jclass cls, jmethodID methodID, jboolean isStatic);
    &ToReflectedMethod,
#else
    NULL,   // void *reserved4;
    NULL,   // void *reserved5;
    NULL,   // void *reserved6;
#endif

    // jclass (JNICALL *GetSuperclass) (JNIEnv *env, jclass sub);
    &GetSuperclass,
    
    // jboolean (JNICALL *IsAssignableFrom) (JNIEnv *env, jclass sub, jclass sup);
    &IsAssignableFrom,
    
#ifdef JDK1_2
    // jobject (JNICALL *ToReflectedField) (JNIEnv *env, jclass cls, jfieldID fieldID, jboolean isStatic);
    &ToReflectedField,
#else    
    NULL, // void *reserved7;
#endif

    // jint (JNICALL *Throw) (JNIEnv *env, jthrowable obj);
    &Throw,
    
    // jint (JNICALL *ThrowNew) (JNIEnv *env, jclass clazz, const char *msg);
    &ThrowNew,
    
    // jthrowable (JNICALL *ExceptionOccurred) (JNIEnv *env);
    &ExceptionOccurred,
    
    // void (JNICALL *ExceptionDescribe) (JNIEnv *env);
    &ExceptionDescribe,
    
    // void (JNICALL *ExceptionClear) (JNIEnv *env);
    &ExceptionClear,
    
    // void (JNICALL *FatalError) (JNIEnv *env, const char *msg);
    &FatalError,
    
#ifdef JDK1_2
    // jint (JNICALL *PushLocalFrame) (JNIEnv *env, jint capacity);
    &PushLocalFrame,
    
    // jobject (JNICALL *PopLocalFrame) (JNIEnv *env, jobject result);
    &PopLocalFrame,
#else
    NULL, // void *reserved8;
    NULL, // void *reserved9;
#endif

    // jobject (JNICALL *NewGlobalRef) (JNIEnv *env, jobject lobj);
    &NewGlobalRef,
    
    // void (JNICALL *DeleteGlobalRef) (JNIEnv *env, jobject gref);
    &DeleteGlobalRef,
    
    // void (JNICALL *DeleteLocalRef) (JNIEnv *env, jobject obj);
    &DeleteLocalRef,
    
    // jboolean (JNICALL *IsSameObject) (JNIEnv *env, jobject obj1, jobject obj2);
    &IsSameObject,
    
#ifdef JDK1_2
    // jobject (JNICALL *NewLocalRef) (JNIEnv *env, jobject ref);
    &NewLocalRef,
    
    // jint (JNICALL *EnsureLocalCapacity) (JNIEnv *env, jint capacity);
    &EnsureLocalCapacity,
#else
    NULL, // void *reserved10;
    NULL, // void *reserved11;
#endif

    // jobject (JNICALL *AllocObject) (JNIEnv *env, jclass clazz);
    &AllocObject,
    
#define REFERENCE_METHOD_FAMILY(methodName) &methodName, &methodName##V, &methodName##A,

    // jobject (JNICALL *NewObject) (JNIEnv *env, jclass clazz, jmethodID methodID, ...);
    // jobject (JNICALL *NewObjectV) (JNIEnv *env, jclass clazz, jmethodID methodID, va_list args);
    // jobject (JNICALL *NewObjectA) (JNIEnv *env, jclass clazz, jmethodID methodID, jvalue *args);
    REFERENCE_METHOD_FAMILY(NewObject)
    
    // jclass (JNICALL *GetObjectClass) (JNIEnv *env, jobject obj);
    &GetObjectClass,
    
    // jboolean (JNICALL *IsInstanceOf) (JNIEnv *env, jobject obj, jclass clazz);
    &IsInstanceOf,

    // jmethodID (JNICALL *GetMethodID)(JNIEnv *env, jclass clazz, const char *name, const char *sig);
    &GetMethodID,

    // jobject (JNICALL *CallObjectMethod) (JNIEnv *env, jobject obj, jmethodID methodID, ...);
    // jobject (JNICALL *CallObjectMethodV) (JNIEnv *env, jobject obj, jmethodID methodID, va_list args);
    // jobject (JNICALL *CallObjectMethodA) (JNIEnv *env, jobject obj, jmethodID methodID, jvalue * args);
    REFERENCE_METHOD_FAMILY(CallObjectMethod)
    
    // jboolean (JNICALL *CallBooleanMethod) (JNIEnv *env, jobject obj, jmethodID methodID, ...);
    // jboolean (JNICALL *CallBooleanMethodV) (JNIEnv *env, jobject obj, jmethodID methodID, va_list args);
    // jboolean (JNICALL *CallBooleanMethodA) (JNIEnv *env, jobject obj, jmethodID methodID, jvalue * args);
    REFERENCE_METHOD_FAMILY(CallBooleanMethod)

    // jbyte (JNICALL *CallByteMethod) (JNIEnv *env, jobject obj, jmethodID methodID, ...);
    // jbyte (JNICALL *CallByteMethodV) (JNIEnv *env, jobject obj, jmethodID methodID, va_list args);
    // jbyte (JNICALL *CallByteMethodA) (JNIEnv *env, jobject obj, jmethodID methodID, jvalue *args);
    REFERENCE_METHOD_FAMILY(CallByteMethod)

    // jchar (JNICALL *CallCharMethod) (JNIEnv *env, jobject obj, jmethodID methodID, ...);
    // jchar (JNICALL *CallCharMethodV) (JNIEnv *env, jobject obj, jmethodID methodID, va_list args);
    // jchar (JNICALL *CallCharMethodA) (JNIEnv *env, jobject obj, jmethodID methodID, jvalue *args);
    REFERENCE_METHOD_FAMILY(CallCharMethod)

    // jshort (JNICALL *CallShortMethod) (JNIEnv *env, jobject obj, jmethodID methodID, ...);
    // jshort (JNICALL *CallShortMethodV) (JNIEnv *env, jobject obj, jmethodID methodID, va_list args);
    // jshort (JNICALL *CallShortMethodA) (JNIEnv *env, jobject obj, jmethodID methodID, jvalue *args);
    REFERENCE_METHOD_FAMILY(CallShortMethod)

    // jint (JNICALL *CallIntMethod) (JNIEnv *env, jobject obj, jmethodID methodID, ...);
    // jint (JNICALL *CallIntMethodV) (JNIEnv *env, jobject obj, jmethodID methodID, va_list args);
    // jint (JNICALL *CallIntMethodA) (JNIEnv *env, jobject obj, jmethodID methodID, jvalue *args);
    REFERENCE_METHOD_FAMILY(CallIntMethod)

    // jlong (JNICALL *CallLongMethod) (JNIEnv *env, jobject obj, jmethodID methodID, ...);
    // jlong (JNICALL *CallLongMethodV) (JNIEnv *env, jobject obj, jmethodID methodID, va_list args);
    // jlong (JNICALL *CallLongMethodA) (JNIEnv *env, jobject obj, jmethodID methodID, jvalue *args);
    REFERENCE_METHOD_FAMILY(CallLongMethod)

    // jfloat (JNICALL *CallFloatMethod) (JNIEnv *env, jobject obj, jmethodID methodID, ...);
    // jfloat (JNICALL *CallFloatMethodV) (JNIEnv *env, jobject obj, jmethodID methodID, va_list args);
    // jfloat (JNICALL *CallFloatMethodA) (JNIEnv *env, jobject obj, jmethodID methodID, jvalue *args);
    REFERENCE_METHOD_FAMILY(CallFloatMethod)

    // jdouble (JNICALL *CallDoubleMethod) (JNIEnv *env, jobject obj, jmethodID methodID, ...);
    // jdouble (JNICALL *CallDoubleMethodV) (JNIEnv *env, jobject obj, jmethodID methodID, va_list args);
    // jdouble (JNICALL *CallDoubleMethodA) (JNIEnv *env, jobject obj, jmethodID methodID, jvalue *args);
    REFERENCE_METHOD_FAMILY(CallDoubleMethod)

    // void (JNICALL *CallVoidMethod) (JNIEnv *env, jobject obj, jmethodID methodID, ...);
    // void (JNICALL *CallVoidMethodV) (JNIEnv *env, jobject obj, jmethodID methodID, va_list args);
    // void (JNICALL *CallVoidMethodA) (JNIEnv *env, jobject obj, jmethodID methodID, jvalue * args);
    REFERENCE_METHOD_FAMILY(CallVoidMethod)

    // jobject (JNICALL *CallNonvirtualObjectMethod) (JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, ...);
    // jobject (JNICALL *CallNonvirtualObjectMethodV) (JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, va_list args);
    // jobject (JNICALL *CallNonvirtualObjectMethodA) (JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, jvalue * args);
    REFERENCE_METHOD_FAMILY(CallNonvirtualObjectMethod)

    // jboolean (JNICALL *CallNonvirtualBooleanMethod) (JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, ...);
    // jboolean (JNICALL *CallNonvirtualBooleanMethodV) (JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, va_list args);
    // jboolean (JNICALL *CallNonvirtualBooleanMethodA) (JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, jvalue * args);
    REFERENCE_METHOD_FAMILY(CallNonvirtualBooleanMethod)

    // jbyte (JNICALL *CallNonvirtualByteMethod) (JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, ...);
    // jbyte (JNICALL *CallNonvirtualByteMethodV) (JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, va_list args);
    // jbyte (JNICALL *CallNonvirtualByteMethodA) (JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, jvalue *args);
    REFERENCE_METHOD_FAMILY(CallNonvirtualByteMethod)

    // jchar (JNICALL *CallNonvirtualCharMethod) (JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, ...);
    // jchar (JNICALL *CallNonvirtualCharMethodV) (JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, va_list args);
    // jchar (JNICALL *CallNonvirtualCharMethodA) (JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, jvalue *args);
    REFERENCE_METHOD_FAMILY(CallNonvirtualCharMethod)

    // jshort (JNICALL *CallNonvirtualShortMethod) (JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, ...);
    // jshort (JNICALL *CallNonvirtualShortMethodV) (JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, va_list args);
    // jshort (JNICALL *CallNonvirtualShortMethodA) (JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, jvalue *args);
    REFERENCE_METHOD_FAMILY(CallNonvirtualShortMethod)

    // jint (JNICALL *CallNonvirtualIntMethod) (JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, ...);
    // jint (JNICALL *CallNonvirtualIntMethodV) (JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, va_list args);
    // jint (JNICALL *CallNonvirtualIntMethodA) (JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, jvalue *args);
    REFERENCE_METHOD_FAMILY(CallNonvirtualIntMethod)

    // jlong (JNICALL *CallNonvirtualLongMethod) (JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, ...);
    // jlong (JNICALL *CallNonvirtualLongMethodV) (JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, va_list args);
    // jlong (JNICALL *CallNonvirtualLongMethodA) (JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, jvalue *args);
    REFERENCE_METHOD_FAMILY(CallNonvirtualLongMethod)

    // jfloat (JNICALL *CallNonvirtualFloatMethod) (JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, ...);
    // jfloat (JNICALL *CallNonvirtualFloatMethodV) (JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, va_list args);
    // jfloat (JNICALL *CallNonvirtualFloatMethodA) (JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, jvalue *args);
    REFERENCE_METHOD_FAMILY(CallNonvirtualFloatMethod)

    // jdouble (JNICALL *CallNonvirtualDoubleMethod) (JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, ...);
    // jdouble (JNICALL *CallNonvirtualDoubleMethodV) (JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, va_list args);
    // jdouble (JNICALL *CallNonvirtualDoubleMethodA) (JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, jvalue *args);
    REFERENCE_METHOD_FAMILY(CallNonvirtualDoubleMethod)

    // void (JNICALL *CallNonvirtualVoidMethod) (JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, ...);
    // void (JNICALL *CallNonvirtualVoidMethodV) (JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, va_list args);
    // void (JNICALL *CallNonvirtualVoidMethodA) (JNIEnv *env, jobject obj, jclass clazz, jmethodID methodID, jvalue * args);
    REFERENCE_METHOD_FAMILY(CallNonvirtualVoidMethod)

    // jfieldID (JNICALL *GetFieldID) (JNIEnv *env, jclass clazz, const char *name, const char *sig);
    &GetFieldID,

    // jobject (JNICALL *GetObjectField) (JNIEnv *env, jobject obj, jfieldID fieldID);
    // jboolean (JNICALL *GetBooleanField) (JNIEnv *env, jobject obj, jfieldID fieldID);
    // jbyte (JNICALL *GetByteField) (JNIEnv *env, jobject obj, jfieldID fieldID);
    // jchar (JNICALL *GetCharField) (JNIEnv *env, jobject obj, jfieldID fieldID);
    // jshort (JNICALL *GetShortField) (JNIEnv *env, jobject obj, jfieldID fieldID);
    // jint (JNICALL *GetIntField) (JNIEnv *env, jobject obj, jfieldID fieldID);
    // jlong (JNICALL *GetLongField) (JNIEnv *env, jobject obj, jfieldID fieldID);
    // jfloat (JNICALL *GetFloatField) (JNIEnv *env, jobject obj, jfieldID fieldID);
    // jdouble (JNICALL *GetDoubleField) (JNIEnv *env, jobject obj, jfieldID fieldID);
    &GetObjectField, &GetBooleanField, &GetByteField, &GetCharField, &GetShortField, &GetIntField, &GetLongField,
    &GetFloatField, &GetDoubleField,

    // void (JNICALL *SetObjectField) (JNIEnv *env, jobject obj, jfieldID fieldID, jobject val);
    // void (JNICALL *SetBooleanField) (JNIEnv *env, jobject obj, jfieldID fieldID, jboolean val);
    // void (JNICALL *SetByteField) (JNIEnv *env, jobject obj, jfieldID fieldID, jbyte val);
    // void (JNICALL *SetCharField) (JNIEnv *env, jobject obj, jfieldID fieldID, jchar val);
    // void (JNICALL *SetShortField) (JNIEnv *env, jobject obj, jfieldID fieldID, jshort val);
    // void (JNICALL *SetIntField) (JNIEnv *env, jobject obj, jfieldID fieldID, jint val);
    // void (JNICALL *SetLongField) (JNIEnv *env, jobject obj, jfieldID fieldID, jlong val);
    // void (JNICALL *SetFloatField) (JNIEnv *env, jobject obj, jfieldID fieldID, jfloat val);
    // void (JNICALL *SetDoubleField) (JNIEnv *env, jobject obj, jfieldID fieldID, jdouble val);
    &SetObjectField, &SetBooleanField, &SetByteField, &SetCharField, &SetShortField, &SetIntField, &SetLongField,
    &SetFloatField, &SetDoubleField,

    // jmethodID (JNICALL *GetStaticMethodID) (JNIEnv *env, jclass clazz, const char *name, const char *sig);
    &GetStaticMethodID,

    // jobject (JNICALL *CallStaticObjectMethod) (JNIEnv *env, jclass clazz, jmethodID methodID, ...);
    // jobject (JNICALL *CallStaticObjectMethodV) (JNIEnv *env, jclass clazz, jmethodID methodID, va_list args);
    // jobject (JNICALL *CallStaticObjectMethodA) (JNIEnv *env, jclass clazz, jmethodID methodID, jvalue *args);
    REFERENCE_METHOD_FAMILY(CallStaticObjectMethod)

    // jboolean (JNICALL *CallStaticBooleanMethod) (JNIEnv *env, jclass clazz, jmethodID methodID, ...);
    // jboolean (JNICALL *CallStaticBooleanMethodV) (JNIEnv *env, jclass clazz, jmethodID methodID, va_list args);
    // jboolean (JNICALL *CallStaticBooleanMethodA) (JNIEnv *env, jclass clazz, jmethodID methodID, jvalue *args);
    REFERENCE_METHOD_FAMILY(CallStaticBooleanMethod)

    // jbyte (JNICALL *CallStaticByteMethod) (JNIEnv *env, jclass clazz, jmethodID methodID, ...);
    // jbyte (JNICALL *CallStaticByteMethodV) (JNIEnv *env, jclass clazz, jmethodID methodID, va_list args);
    // jbyte (JNICALL *CallStaticByteMethodA) (JNIEnv *env, jclass clazz, jmethodID methodID, jvalue *args);
    REFERENCE_METHOD_FAMILY(CallStaticByteMethod)

    // jchar (JNICALL *CallStaticCharMethod) (JNIEnv *env, jclass clazz, jmethodID methodID, ...);
    // jchar (JNICALL *CallStaticCharMethodV) (JNIEnv *env, jclass clazz, jmethodID methodID, va_list args);
    // jchar (JNICALL *CallStaticCharMethodA) (JNIEnv *env, jclass clazz, jmethodID methodID, jvalue *args);
    REFERENCE_METHOD_FAMILY(CallStaticCharMethod)

    // jshort (JNICALL *CallStaticShortMethod) (JNIEnv *env, jclass clazz, jmethodID methodID, ...);
    // jshort (JNICALL *CallStaticShortMethodV) (JNIEnv *env, jclass clazz, jmethodID methodID, va_list args);
    // jshort (JNICALL *CallStaticShortMethodA) (JNIEnv *env, jclass clazz, jmethodID methodID, jvalue *args);
    REFERENCE_METHOD_FAMILY(CallStaticShortMethod)

    // jint (JNICALL *CallStaticIntMethod) (JNIEnv *env, jclass clazz, jmethodID methodID, ...);
    // jint (JNICALL *CallStaticIntMethodV) (JNIEnv *env, jclass clazz, jmethodID methodID, va_list args);
    // jint (JNICALL *CallStaticIntMethodA) (JNIEnv *env, jclass clazz, jmethodID methodID, jvalue *args);
    REFERENCE_METHOD_FAMILY(CallStaticIntMethod)

    // jlong (JNICALL *CallStaticLongMethod) (JNIEnv *env, jclass clazz, jmethodID methodID, ...);
    // jlong (JNICALL *CallStaticLongMethodV) (JNIEnv *env, jclass clazz, jmethodID methodID, va_list args);
    // jlong (JNICALL *CallStaticLongMethodA) (JNIEnv *env, jclass clazz, jmethodID methodID, jvalue *args);
    REFERENCE_METHOD_FAMILY(CallStaticLongMethod)

    // jfloat (JNICALL *CallStaticFloatMethod) (JNIEnv *env, jclass clazz, jmethodID methodID, ...);
    // jfloat (JNICALL *CallStaticFloatMethodV) (JNIEnv *env, jclass clazz, jmethodID methodID, va_list args);
    // jfloat (JNICALL *CallStaticFloatMethodA) (JNIEnv *env, jclass clazz, jmethodID methodID, jvalue *args);
    REFERENCE_METHOD_FAMILY(CallStaticFloatMethod)

    // jdouble (JNICALL *CallStaticDoubleMethod) (JNIEnv *env, jclass clazz, jmethodID methodID, ...);
    // jdouble (JNICALL *CallStaticDoubleMethodV) (JNIEnv *env, jclass clazz, jmethodID methodID, va_list args);
    // jdouble (JNICALL *CallStaticDoubleMethodA) (JNIEnv *env, jclass clazz, jmethodID methodID, jvalue *args);
    REFERENCE_METHOD_FAMILY(CallStaticDoubleMethod)

    // void (JNICALL *CallStaticVoidMethod) (JNIEnv *env, jclass cls, jmethodID methodID, ...);
    // void (JNICALL *CallStaticVoidMethodV) (JNIEnv *env, jclass cls, jmethodID methodID, va_list args);
    // void (JNICALL *CallStaticVoidMethodA) (JNIEnv *env, jclass cls, jmethodID methodID, jvalue * args);
    REFERENCE_METHOD_FAMILY(CallStaticVoidMethod)

    // jfieldID (JNICALL *GetStaticFieldID) (JNIEnv *env, jclass clazz, const char *name, const char *sig);
    &GetStaticFieldID,
    
    // jobject (JNICALL *GetStaticObjectField) (JNIEnv *env, jclass clazz, jfieldID fieldID);
    // jboolean (JNICALL *GetStaticBooleanField) (JNIEnv *env, jclass clazz, jfieldID fieldID);
    // jbyte (JNICALL *GetStaticByteField) (JNIEnv *env, jclass clazz, jfieldID fieldID);
    // jchar (JNICALL *GetStaticCharField) (JNIEnv *env, jclass clazz, jfieldID fieldID);
    // jshort (JNICALL *GetStaticShortField) (JNIEnv *env, jclass clazz, jfieldID fieldID);
    // jint (JNICALL *GetStaticIntField) (JNIEnv *env, jclass clazz, jfieldID fieldID);
    // jlong (JNICALL *GetStaticLongField) (JNIEnv *env, jclass clazz, jfieldID fieldID);
    // jfloat (JNICALL *GetStaticFloatField) (JNIEnv *env, jclass clazz, jfieldID fieldID);
    // jdouble (JNICALL *GetStaticDoubleField) (JNIEnv *env, jclass clazz, jfieldID fieldID);
    &GetStaticObjectField, &GetStaticBooleanField, &GetStaticByteField, &GetStaticCharField, &GetStaticShortField,
    &GetStaticIntField, &GetStaticLongField, &GetStaticFloatField, &GetStaticDoubleField,

    // void (JNICALL *SetStaticObjectField) (JNIEnv *env, jclass clazz, jfieldID fieldID, jobject value);
    // void (JNICALL *SetStaticBooleanField) (JNIEnv *env, jclass clazz, jfieldID fieldID, jboolean value);
    // void (JNICALL *SetStaticByteField) (JNIEnv *env, jclass clazz, jfieldID fieldID, jbyte value);
    // void (JNICALL *SetStaticCharField) (JNIEnv *env, jclass clazz, jfieldID fieldID, jchar value);
    // void (JNICALL *SetStaticShortField) (JNIEnv *env, jclass clazz, jfieldID fieldID, jshort value);
    // void (JNICALL *SetStaticIntField) (JNIEnv *env, jclass clazz, jfieldID fieldID, jint value);
    // void (JNICALL *SetStaticLongField) (JNIEnv *env, jclass clazz, jfieldID fieldID, jlong value);
    // void (JNICALL *SetStaticFloatField) (JNIEnv *env, jclass clazz, jfieldID fieldID, jfloat value);
    // void (JNICALL *SetStaticDoubleField) (JNIEnv *env, jclass clazz, jfieldID fieldID, jdouble value);
    &SetStaticObjectField, &SetStaticBooleanField, &SetStaticByteField, &SetStaticCharField, &SetStaticShortField,
    &SetStaticIntField, &SetStaticLongField, &SetStaticFloatField, &SetStaticDoubleField,

    // jstring (JNICALL *NewString) (JNIEnv *env, const jchar *unicode, jsize len);
    // jsize (JNICALL *GetStringLength) (JNIEnv *env, jstring str);
    // const jchar *(JNICALL *GetStringChars) (JNIEnv *env, jstring str, jboolean *isCopy);
    // void (JNICALL *ReleaseStringChars) (JNIEnv *env, jstring str, const jchar *chars);
    &NewString, &GetStringLength, &GetStringChars, &ReleaseStringChars,

    // jstring (JNICALL *NewStringUTF) (JNIEnv *env, const char *utf);
    // jsize (JNICALL *GetStringUTFLength) (JNIEnv *env, jstring str);
    // const char* (JNICALL *GetStringUTFChars) (JNIEnv *env, jstring str, jboolean *isCopy);
    // void (JNICALL *ReleaseStringUTFChars) (JNIEnv *env, jstring str, const char* chars);
    &NewStringUTF, &GetStringUTFLength, &GetStringUTFChars, &ReleaseStringUTFChars,

    // jsize (JNICALL *GetArrayLength) (JNIEnv *env, jarray array);
    &GetArrayLength,

    // jobjectArray (JNICALL *NewObjectArray) (JNIEnv *env, jsize len, jclass clazz, jobject init);
    // jobject (JNICALL *GetObjectArrayElement) (JNIEnv *env, jobjectArray array, jsize index);
    // void (JNICALL *SetObjectArrayElement) (JNIEnv *env, jobjectArray array, jsize index, jobject val);
    &NewObjectArray, &GetObjectArrayElement, &SetObjectArrayElement,

    // jbooleanArray (JNICALL *NewBooleanArray) (JNIEnv *env, jsize len);
    // jbyteArray (JNICALL *NewByteArray) (JNIEnv *env, jsize len);
    // jcharArray (JNICALL *NewCharArray) (JNIEnv *env, jsize len);
    // jshortArray (JNICALL *NewShortArray) (JNIEnv *env, jsize len);
    // jintArray (JNICALL *NewIntArray) (JNIEnv *env, jsize len);
    // jlongArray (JNICALL *NewLongArray) (JNIEnv *env, jsize len);
    // jfloatArray (JNICALL *NewFloatArray) (JNIEnv *env, jsize len);
    // jdoubleArray (JNICALL *NewDoubleArray) (JNIEnv *env, jsize len);
    &NewBooleanArray, &NewByteArray, &NewCharArray, &NewShortArray, 
    &NewIntArray, &NewLongArray, &NewFloatArray, &NewDoubleArray,

    // jboolean * (JNICALL *GetBooleanArrayElements) (JNIEnv *env, jbooleanArray array, jboolean *isCopy);
    // jbyte * (JNICALL *GetByteArrayElements) (JNIEnv *env, jbyteArray array, jboolean *isCopy);
    // jchar * (JNICALL *GetCharArrayElements) (JNIEnv *env, jcharArray array, jboolean *isCopy);
    // jshort * (JNICALL *GetShortArrayElements) (JNIEnv *env, jshortArray array, jboolean *isCopy);
    // jint * (JNICALL *GetIntArrayElements) (JNIEnv *env, jintArray array, jboolean *isCopy);
    // jlong * (JNICALL *GetLongArrayElements) (JNIEnv *env, jlongArray array, jboolean *isCopy);
    // jfloat * (JNICALL *GetFloatArrayElements) (JNIEnv *env, jfloatArray array, jboolean *isCopy);
    // jdouble * (JNICALL *GetDoubleArrayElements) (JNIEnv *env, jdoubleArray array, jboolean *isCopy);
    &GetBooleanArrayElements, &GetByteArrayElements, &GetCharArrayElements, &GetShortArrayElements, 
    &GetIntArrayElements, &GetLongArrayElements, &GetFloatArrayElements, &GetDoubleArrayElements, 

    // void (JNICALL *ReleaseBooleanArrayElements)(JNIEnv *env, jbooleanArray array, jboolean *elems, jint mode);
    // void (JNICALL *ReleaseByteArrayElements) (JNIEnv *env, jbyteArray array, jbyte *elems, jint mode);
    // void (JNICALL *ReleaseCharArrayElements) (JNIEnv *env, jcharArray array, jchar *elems, jint mode);
    // void (JNICALL *ReleaseShortArrayElements) (JNIEnv *env, jshortArray array, jshort *elems, jint mode);
    // void (JNICALL *ReleaseIntArrayElements) (JNIEnv *env, jintArray array, jint *elems, jint mode);
    // void (JNICALL *ReleaseLongArrayElements) (JNIEnv *env, jlongArray array, jlong *elems, jint mode);
    // void (JNICALL *ReleaseFloatArrayElements) (JNIEnv *env, jfloatArray array, jfloat *elems, jint mode);
    // void (JNICALL *ReleaseDoubleArrayElements) (JNIEnv *env, jdoubleArray array, jdouble *elems, jint mode);
    &ReleaseBooleanArrayElements, &ReleaseByteArrayElements, &ReleaseCharArrayElements, &ReleaseShortArrayElements, 
    &ReleaseIntArrayElements, &ReleaseLongArrayElements, &ReleaseFloatArrayElements, &ReleaseDoubleArrayElements, 

    // void (JNICALL *GetBooleanArrayRegion) (JNIEnv *env, jbooleanArray array, jsize start, jsize l, jboolean *buf);
    // void (JNICALL *GetByteArrayRegion) (JNIEnv *env, jbyteArray array, jsize start, jsize len, jbyte *buf);
    // void (JNICALL *GetCharArrayRegion) (JNIEnv *env, jcharArray array, jsize start, jsize len, jchar *buf);
    // void (JNICALL *GetShortArrayRegion) (JNIEnv *env, jshortArray array, jsize start, jsize len, jshort *buf);
    // void (JNICALL *GetIntArrayRegion) (JNIEnv *env, jintArray array, jsize start, jsize len, jint *buf);
    // void (JNICALL *GetLongArrayRegion) (JNIEnv *env, jlongArray array, jsize start, jsize len, jlong *buf);
    // void (JNICALL *GetFloatArrayRegion) (JNIEnv *env, jfloatArray array, jsize start, jsize len, jfloat *buf);
    // void (JNICALL *GetDoubleArrayRegion) (JNIEnv *env, jdoubleArray array, jsize start, jsize len, jdouble *buf);
    &GetBooleanArrayRegion, &GetByteArrayRegion, &GetCharArrayRegion, &GetShortArrayRegion, 
    &GetIntArrayRegion, &GetLongArrayRegion, &GetFloatArrayRegion, &GetDoubleArrayRegion, 

    // void (JNICALL *SetBooleanArrayRegion) (JNIEnv *env, jbooleanArray array, jsize start, jsize l, jboolean *buf);
    // void (JNICALL *SetByteArrayRegion) (JNIEnv *env, jbyteArray array, jsize start, jsize len, jbyte *buf);
    // void (JNICALL *SetCharArrayRegion) (JNIEnv *env, jcharArray array, jsize start, jsize len, jchar *buf);
    // void (JNICALL *SetShortArrayRegion) (JNIEnv *env, jshortArray array, jsize start, jsize len, jshort *buf);
    // void (JNICALL *SetIntArrayRegion) (JNIEnv *env, jintArray array, jsize start, jsize len, jint *buf);
    // void (JNICALL *SetLongArrayRegion) (JNIEnv *env, jlongArray array, jsize start, jsize len, jlong *buf);
    // void (JNICALL *SetFloatArrayRegion) (JNIEnv *env, jfloatArray array, jsize start, jsize len, jfloat *buf);
    // void (JNICALL *SetDoubleArrayRegion) (JNIEnv *env, jdoubleArray array, jsize start, jsize len, jdouble *buf);
    &SetBooleanArrayRegion, &SetByteArrayRegion, &SetCharArrayRegion, &SetShortArrayRegion, 
    &SetIntArrayRegion, &SetLongArrayRegion, &SetFloatArrayRegion, &SetDoubleArrayRegion, 

    // jint (JNICALL *RegisterNatives) (JNIEnv *env, jclass clazz, const JNINativeMethod *methods, jint nMethods);
    // jint (JNICALL *UnregisterNatives) (JNIEnv *env, jclass clazz);
    &RegisterNatives, &UnregisterNatives,

    // jint (JNICALL *MonitorEnter) (JNIEnv *env, jobject obj);
    // jint (JNICALL *MonitorExit) (JNIEnv *env, jobject obj);
    &MonitorEnter, &MonitorExit,

    // jint (JNICALL *GetJavaVM) (JNIEnv *env, JavaVM **vm);
    &GetJavaVM
};

nsDataHashtable<JavaClassMemberKey, void*>* ProxyJNIEnv::theIDTable = NULL;

ProxyJNIEnv::ProxyJNIEnv(nsIJVMPlugin* jvmPlugin, nsISecureEnv* secureEnv)
    :   mSecureEnv(secureEnv), mContext(NULL), mInProxyFindClass(JNI_FALSE)
{
    this->functions = &theFuncs;
    if (theIDTable == NULL) {
        theIDTable = new nsDataHashtable<JavaClassMemberKey, void*>;
        if (theIDTable) {
            if (!theIDTable->Init(16)) {
                delete theIDTable;
                theIDTable = NULL;
                NS_WARNING("theIDTable initialization FAILED");
            }
        }
        else {
            NS_WARNING("theIDTable allocation FAILED");
        }
    }
    
    // Ask the JVM for a new nsISecureEnv, if none provided.
    if (secureEnv == NULL) {
        nsresult rv = jvmPlugin->CreateSecureEnv(this, &mSecureEnv);
#ifdef DEBUG
        if (NS_FAILED(rv))
            NS_WARNING("CreateSecureEnv FAILED");
        else
            NS_WARNING("CreateSecureEnv OK");
#endif
    }
}

ProxyJNIEnv::~ProxyJNIEnv()
{
    this->functions = NULL;
    
    if (mSecureEnv != NULL)
        mSecureEnv->Release();
}

JNIEnv* CreateProxyJNI(nsIJVMPlugin* jvmPlugin, nsISecureEnv* inSecureEnv)
{
    ProxyJNIEnv* proxyEnv = new ProxyJNIEnv(jvmPlugin, inSecureEnv);
    if (proxyEnv->getSecureEnv() == NULL) {
        delete proxyEnv;
        return NULL;
    }
    return proxyEnv;
}

void DeleteProxyJNI(JNIEnv* env)
{
    ProxyJNIEnv* proxyEnv = (ProxyJNIEnv*)env;
    if (proxyEnv != NULL)
        delete proxyEnv;
}

nsISecureEnv* GetSecureEnv(JNIEnv* env)
{
    ProxyJNIEnv* proxyEnv = (ProxyJNIEnv*)env;
    return proxyEnv->getSecureEnv();
}

PR_IMPLEMENT(void) SetSecurityContext(JNIEnv* env, nsISecurityContext *context) {
    ProxyJNIEnv* proxyEnv = (ProxyJNIEnv*)env;
    proxyEnv->SetSecurityContext(context);
}

PR_IMPLEMENT(nsresult) GetSecurityContext(JNIEnv* env, nsISecurityContext **context) {
    ProxyJNIEnv* proxyEnv = (ProxyJNIEnv*)env;
    return proxyEnv->GetSecurityContext(context);
}

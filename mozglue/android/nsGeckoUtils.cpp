/* -*- Mode: c++; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <jni.h>

#ifdef MOZ_MEMORY
// Wrap malloc and free to use jemalloc
#define malloc __wrap_malloc
#define free __wrap_free
#endif

#include <stdlib.h>
#include <fcntl.h>
#include "APKOpen.h"
#include "Zip.h"
#include "mozilla/RefPtr.h"

extern "C"
__attribute__ ((visibility("default")))
void JNICALL
Java_org_mozilla_gecko_mozglue_GeckoLoader_putenv(JNIEnv *jenv, jclass, jstring map)
{
    const char* str;
    // XXX: java doesn't give us true UTF8, we should figure out something
    // better to do here
    str = jenv->GetStringUTFChars(map, NULL);
    if (str == NULL)
        return;
    putenv(strdup(str));
    jenv->ReleaseStringUTFChars(map, str);
}

extern "C"
__attribute__ ((visibility("default")))
jobject JNICALL
Java_org_mozilla_gecko_mozglue_DirectBufferAllocator_nativeAllocateDirectBuffer(JNIEnv *jenv, jclass, jlong size)
{
    jobject buffer = NULL;
    void* mem = malloc(size);
    if (mem) {
        buffer = jenv->NewDirectByteBuffer(mem, size);
        if (!buffer)
            free(mem);
    }
    return buffer;
}

extern "C"
__attribute__ ((visibility("default")))
void JNICALL
Java_org_mozilla_gecko_mozglue_DirectBufferAllocator_nativeFreeDirectBuffer(JNIEnv *jenv, jclass, jobject buf)
{
    free(jenv->GetDirectBufferAddress(buf));
}

extern "C"
__attribute__ ((visibility("default")))
jlong JNICALL
Java_org_mozilla_gecko_mozglue_NativeZip_getZip(JNIEnv *jenv, jclass, jstring path)
{
    const char* str;
    str = jenv->GetStringUTFChars(path, NULL);
    if (!str || !*str) {
        if (str)
            jenv->ReleaseStringUTFChars(path, str);
        JNI_Throw(jenv, "java/lang/IllegalArgumentException", "Invalid path");
        return 0;
    }
    mozilla::RefPtr<Zip> zip = ZipCollection::GetZip(str);
    jenv->ReleaseStringUTFChars(path, str);
    if (!zip) {
        JNI_Throw(jenv, "java/lang/IllegalArgumentException", "Invalid path or invalid zip");
        return 0;
    }
    zip->AddRef();
    return (jlong) zip.get();
}

extern "C"
__attribute__ ((visibility("default")))
jlong JNICALL
Java_org_mozilla_gecko_mozglue_NativeZip_getZipFromByteBuffer(JNIEnv *jenv, jclass, jobject buffer)
{
    void *buf = jenv->GetDirectBufferAddress(buffer);
    size_t size = jenv->GetDirectBufferCapacity(buffer);
    mozilla::RefPtr<Zip> zip = Zip::Create(buf, size);
    if (!zip) {
        JNI_Throw(jenv, "java/lang/IllegalArgumentException", "Invalid zip");
        return 0;
    }
    zip->AddRef();
    return (jlong) zip.get();
}

 extern "C"
__attribute__ ((visibility("default")))
void JNICALL
Java_org_mozilla_gecko_mozglue_NativeZip__1release(JNIEnv *jenv, jclass, jlong obj)
{
    Zip *zip = (Zip *)obj;
    zip->Release();
}

extern "C"
__attribute__ ((visibility("default")))
jobject JNICALL
Java_org_mozilla_gecko_mozglue_NativeZip__1getInputStream(JNIEnv *jenv, jobject jzip, jlong obj, jstring path)
{
    Zip *zip = (Zip *)obj;
    const char* str;
    str = jenv->GetStringUTFChars(path, NULL);

    Zip::Stream stream;
    bool res = zip->GetStream(str, &stream);
    jenv->ReleaseStringUTFChars(path, str);
    if (!res) {
        return NULL;
    }
    jobject buf = jenv->NewDirectByteBuffer(const_cast<void *>(stream.GetBuffer()), stream.GetSize());
    if (!buf) {
        JNI_Throw(jenv, "java/lang/RuntimeException", "Failed to create ByteBuffer");
        return NULL;
    }
    jclass nativeZip = jenv->GetObjectClass(jzip);
    jmethodID method = jenv->GetMethodID(nativeZip, "createInputStream", "(Ljava/nio/ByteBuffer;I)Ljava/io/InputStream;");
    // Since this function is only expected to be called from Java, it is safe
    // to skip exception checking for the method call below, as long as no
    // other Native -> Java call doesn't happen before returning to Java.
    return jenv->CallObjectMethod(jzip, method, buf, (jint) stream.GetType());
}

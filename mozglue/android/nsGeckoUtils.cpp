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

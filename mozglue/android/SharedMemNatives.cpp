/* -*- Mode: c++; c-basic-offset: 2; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <errno.h>
#include <jni.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>

#include "mozilla/Sprintf.h"

extern "C" {

JNIEXPORT
void JNICALL
Java_org_mozilla_gecko_media_SampleBuffer_nativeReadFromDirectBuffer(
    JNIEnv* jenv, jclass, jobject src, jlong dest, jint offset, jint size) {
  uint8_t* from = static_cast<uint8_t*>(jenv->GetDirectBufferAddress(src));
  if (from == nullptr) {
    jenv->ThrowNew(jenv->FindClass("java/lang/NullPointerException"),
                   "Null direct buffer");
    return;
  }

  void* to = reinterpret_cast<void*>(dest);
  if (to == nullptr) {
    jenv->ThrowNew(jenv->FindClass("java/lang/NullPointerException"),
                   "Null shared memory buffer");
    return;
  }

  memcpy(to, from + offset, size);
}

JNIEXPORT
void JNICALL
Java_org_mozilla_gecko_media_SampleBuffer_nativeWriteToDirectBuffer(
    JNIEnv* jenv, jclass, jlong src, jobject dest, jint offset, jint size) {
  uint8_t* from = reinterpret_cast<uint8_t*>(src);
  if (from == nullptr) {
    jenv->ThrowNew(jenv->FindClass("java/lang/NullPointerException"),
                   "Null shared memory buffer");
    return;
  }

  void* to = jenv->GetDirectBufferAddress(dest);
  if (to == nullptr) {
    jenv->ThrowNew(jenv->FindClass("java/lang/NullPointerException"),
                   "Null direct buffer");
    return;
  }

  memcpy(to, from + offset, size);
}

JNIEXPORT
jlong JNICALL Java_org_mozilla_gecko_mozglue_SharedMemory_map(JNIEnv* env,
                                                              jobject jobj,
                                                              jint fd,
                                                              jint length) {
  void* address = mmap(NULL, length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (address == MAP_FAILED) {
    char msg[128];
    SprintfLiteral(msg, "mmap failed. errno=%d", errno);
    env->ThrowNew(env->FindClass("java/lang/NullPointerException"), msg);
    return 0;
  }
  return jlong(address);
}

JNIEXPORT
void JNICALL Java_org_mozilla_gecko_mozglue_SharedMemory_unmap(JNIEnv* env,
                                                               jobject jobj,
                                                               jlong address,
                                                               jint size) {
  munmap((void*)address, (size_t)size);
}
}

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "NativeCrypto.h"

#include <jni.h>

#include <errno.h>
#include <stdlib.h>
#include <inttypes.h>

#include "mozilla/SHA1.h"
#include "pbkdf2_sha256.h"

/**
 * Helper function to invoke native PBKDF2 function with JNI
 * arguments.
 */
extern "C" JNIEXPORT jbyteArray JNICALL Java_org_mozilla_gecko_background_nativecode_NativeCrypto_pbkdf2SHA256
    (JNIEnv *env, jclass jc, jbyteArray jpassword, jbyteArray jsalt, jint c, jint dkLen) {
  if (dkLen < 0) {
    env->ThrowNew(env->FindClass("java/lang/IllegalArgumentException"),
                  "dkLen should not be less than 0");
    return NULL;
  }

  jbyte *password = env->GetByteArrayElements(jpassword, NULL);
  size_t passwordLen = env->GetArrayLength(jpassword);

  jbyte *salt = env->GetByteArrayElements(jsalt, NULL);
  size_t saltLen = env->GetArrayLength(jsalt);

  uint8_t hashResult[dkLen];
  PBKDF2_SHA256((uint8_t *) password, passwordLen, (uint8_t *) salt, saltLen,
      (uint64_t) c, hashResult, (size_t) dkLen);

  env->ReleaseByteArrayElements(jpassword, password, JNI_ABORT);
  env->ReleaseByteArrayElements(jsalt, salt, JNI_ABORT);

  jbyteArray out = env->NewByteArray(dkLen);
  if (out == NULL) {
    return NULL;
  }
  env->SetByteArrayRegion(out, 0, dkLen, (jbyte *) hashResult);

  return out;
}

using namespace mozilla;

/**
 * Helper function to invoke native SHA-1 function with JNI arguments.
 */
extern "C" JNIEXPORT jbyteArray JNICALL Java_org_mozilla_gecko_background_nativecode_NativeCrypto_sha1
    (JNIEnv *env, jclass jc, jbyteArray jstr) {
  jbyte *str = env->GetByteArrayElements(jstr, NULL);
  size_t strLen = env->GetArrayLength(jstr);

  SHA1Sum sha1;
  SHA1Sum::Hash hashResult;
  sha1.update((void *) str, (uint32_t) strLen);
  sha1.finish(hashResult);

  env->ReleaseByteArrayElements(jstr, str, JNI_ABORT);

  jbyteArray out = env->NewByteArray(SHA1Sum::HashSize);
  if (out == NULL) {
    return NULL;
  }
  env->SetByteArrayRegion(out, 0, SHA1Sum::HashSize, (jbyte *) hashResult);

  return out;
}

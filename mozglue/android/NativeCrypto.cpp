/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "NativeCrypto.h"
#include "APKOpen.h"

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
extern "C" JNIEXPORT jbyteArray MOZ_JNICALL Java_org_mozilla_gecko_background_nativecode_NativeCrypto_pbkdf2SHA256
    (JNIEnv *env, jclass jc, jbyteArray jpassword, jbyteArray jsalt, jint c, jint dkLen) {
  if (dkLen < 0) {
    env->ThrowNew(env->FindClass("java/lang/IllegalArgumentException"),
                  "dkLen should not be less than 0");
    return nullptr;
  }

  jbyte *password = env->GetByteArrayElements(jpassword, nullptr);
  size_t passwordLen = env->GetArrayLength(jpassword);

  jbyte *salt = env->GetByteArrayElements(jsalt, nullptr);
  size_t saltLen = env->GetArrayLength(jsalt);

  uint8_t hashResult[dkLen];
  PBKDF2_SHA256((uint8_t *) password, passwordLen, (uint8_t *) salt, saltLen,
      (uint64_t) c, hashResult, (size_t) dkLen);

  env->ReleaseByteArrayElements(jpassword, password, JNI_ABORT);
  env->ReleaseByteArrayElements(jsalt, salt, JNI_ABORT);

  jbyteArray out = env->NewByteArray(dkLen);
  if (out == nullptr) {
    return nullptr;
  }
  env->SetByteArrayRegion(out, 0, dkLen, (jbyte *) hashResult);

  return out;
}

using namespace mozilla;

/**
 * Helper function to invoke native SHA-1 function with JNI arguments.
 */
extern "C" JNIEXPORT jbyteArray MOZ_JNICALL Java_org_mozilla_gecko_background_nativecode_NativeCrypto_sha1
    (JNIEnv *env, jclass jc, jbyteArray jstr) {
  jbyte *str = env->GetByteArrayElements(jstr, nullptr);
  size_t strLen = env->GetArrayLength(jstr);

  SHA1Sum sha1;
  SHA1Sum::Hash hashResult;
  sha1.update((void *) str, (uint32_t) strLen);
  sha1.finish(hashResult);

  env->ReleaseByteArrayElements(jstr, str, JNI_ABORT);

  jbyteArray out = env->NewByteArray(SHA1Sum::kHashSize);
  if (out == nullptr) {
    return nullptr;
  }
  env->SetByteArrayRegion(out, 0, SHA1Sum::kHashSize, (jbyte *) hashResult);

  return out;
}

/**
 * Helper function to invoke native SHA-256 init with JNI arguments.
 */
extern "C" JNIEXPORT jbyteArray MOZ_JNICALL Java_org_mozilla_gecko_background_nativecode_NativeCrypto_sha256init
    (JNIEnv *env, jclass jc) {
  jbyteArray out = env->NewByteArray(sizeof(SHA256_CTX));
  if (nullptr == out) {
    return nullptr;
  }

  SHA256_CTX *shaContext = (SHA256_CTX*)env->GetByteArrayElements(out, nullptr);
  SHA256_Init(shaContext);

  env->ReleaseByteArrayElements(out, (jbyte*)shaContext, 0);

  return out;
}

/**
 * Helper function to invoke native SHA-256 update with JNI arguments.
 */
extern "C" JNIEXPORT void MOZ_JNICALL Java_org_mozilla_gecko_background_nativecode_NativeCrypto_sha256update
    (JNIEnv *env, jclass jc, jbyteArray jctx, jbyteArray jstr, jint len) {
  jbyte *str = env->GetByteArrayElements(jstr, nullptr);

  SHA256_CTX *shaContext = (SHA256_CTX*)env->GetByteArrayElements(jctx, nullptr);

  SHA256_Update(shaContext, (void*)str, (size_t) len);

  env->ReleaseByteArrayElements(jstr, str, JNI_ABORT);
  env->ReleaseByteArrayElements(jctx, (jbyte*)shaContext, 0);

  return;
}

/**
 * Helper function to invoke native SHA-256 finalize with JNI arguments.
 */
extern "C" JNIEXPORT jbyteArray MOZ_JNICALL Java_org_mozilla_gecko_background_nativecode_NativeCrypto_sha256finalize
    (JNIEnv *env, jclass jc, jbyteArray jctx) {
  SHA256_CTX *shaContext = (SHA256_CTX*)env->GetByteArrayElements(jctx, nullptr);

  unsigned char* digest = new unsigned char[32];
  SHA256_Final(digest, shaContext);

  env->ReleaseByteArrayElements(jctx, (jbyte*)shaContext, JNI_ABORT);

  jbyteArray out = env->NewByteArray(32);
  if (nullptr != out) {
    env->SetByteArrayRegion(out, 0, 32, (jbyte*)digest);
  }

  delete[] digest;

  return out;
}

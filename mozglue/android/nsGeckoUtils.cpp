/* -*- Mode: c++; c-basic-offset: 2; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <jni.h>

#include <stdlib.h>
#include <fcntl.h>
#include "APKOpen.h"
#include "Zip.h"
#include "mozilla/RefPtr.h"

#ifdef MOZ_CRASHREPORTER
#  include "minidump-analyzer.h"
#endif

extern "C" __attribute__((visibility("default"))) void MOZ_JNICALL
Java_org_mozilla_gecko_mozglue_GeckoLoader_putenv(JNIEnv* jenv, jclass,
                                                  jstring map) {
  const char* str;
  // XXX: java doesn't give us true UTF8, we should figure out something
  // better to do here
  str = jenv->GetStringUTFChars(map, nullptr);
  if (str == nullptr) return;
  putenv(strdup(str));
  jenv->ReleaseStringUTFChars(map, str);
}

#ifdef MOZ_CRASHREPORTER

extern "C" __attribute__((visibility("default"))) jboolean MOZ_JNICALL
Java_org_mozilla_gecko_mozglue_MinidumpAnalyzer_GenerateStacks(
    JNIEnv* jenv, jclass, jstring minidumpPath, jboolean fullStacks) {
  const char* str;
  str = jenv->GetStringUTFChars(minidumpPath, nullptr);

  bool res = CrashReporter::GenerateStacks(str, fullStacks);

  jenv->ReleaseStringUTFChars(minidumpPath, str);
  return res;
}

#endif  // MOZ_CRASHREPORTER

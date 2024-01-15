/* -*- Mode: c++; c-basic-offset: 2; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <jni.h>

#include <stdlib.h>
#include <string.h>
#include "APKOpen.h"

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

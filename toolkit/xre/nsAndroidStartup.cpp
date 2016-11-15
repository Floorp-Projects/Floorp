/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <android/log.h>

#include <jni.h>

#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include "mozilla/jni/Utils.h"
#include "nsTArray.h"
#include "nsString.h"
#include "nsIFile.h"
#include "nsAppRunner.h"
#include "APKOpen.h"
#include "nsExceptionHandler.h"

#define LOG(args...) __android_log_print(ANDROID_LOG_INFO, MOZ_APP_NAME, args)

extern "C" NS_EXPORT void
GeckoStart(JNIEnv* env, char** argv, int argc, const nsXREAppData* appData)
{
    mozilla::jni::SetGeckoThreadEnv(env);

#ifdef MOZ_CRASHREPORTER
    const struct mapping_info *info = getLibraryMapping();
    while (info->name) {
      CrashReporter::AddLibraryMapping(info->name, info->base,
                                       info->len, info->offset);
      info++;
    }
#endif

    if (!argv) {
        LOG("Failed to get arguments for GeckoStart\n");
        return;
    }

    int result = XRE_main(argc, argv, appData, 0);

    if (result)
        LOG("XRE_main returned %d", result);
}

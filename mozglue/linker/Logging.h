/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef Logging_h
#define Logging_h

#ifdef ANDROID
#include <android/log.h>
#define log(...) __android_log_print(ANDROID_LOG_ERROR, "GeckoLinker", __VA_ARGS__)
#else
#include <cstdio>
#define log(format, ...) fprintf(stderr, format "\n", ##__VA_ARGS__)
#endif

#ifdef MOZ_DEBUG_LINKER
#define debug log
#else
#define debug(...)
#endif

#endif /* Logging_h */

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

/* Expand to 1 or m depending on whether there is one argument or more
 * given. */
#define MOZ_ONE_OR_MORE_ARGS_IMPL2(_1, _2, _3, _4, _5, _6, _7, _8, _9, N, ...) \
  N
#define MOZ_ONE_OR_MORE_ARGS_IMPL(args) MOZ_ONE_OR_MORE_ARGS_IMPL2 args
#define MOZ_ONE_OR_MORE_ARGS(...) \
  MOZ_ONE_OR_MORE_ARGS_IMPL((__VA_ARGS__, m, m, m, m, m, m, m, m, 1, 0))

#define MOZ_MACRO_GLUE(a, b) a b
#define MOZ_CONCAT2(a, b) a ## b
#define MOZ_CONCAT1(a, b) MOZ_CONCAT2(a, b)
#define MOZ_CONCAT(a, b) MOZ_CONCAT1(a, b)

/* Some magic to choose between log1 and logm depending on the number of
 * arguments */
#define MOZ_CHOOSE_LOG(...) \
  MOZ_MACRO_GLUE(MOZ_CONCAT(log, MOZ_ONE_OR_MORE_ARGS(__VA_ARGS__)), \
                 (__VA_ARGS__))

#define log1(format) fprintf(stderr, format "\n")
#define logm(format, ...) fprintf(stderr, format "\n", __VA_ARGS__)
#define log(...) MOZ_CHOOSE_LOG(__VA_ARGS__)

#endif

#ifdef MOZ_DEBUG_LINKER
#define debug log
#else
#define debug(...)
#endif

#ifdef HAVE_64BIT_OS
#  define PRIxAddr "lx"
#  define PRIxSize "lx"
#  define PRIdSize "ld"
#else
#  define PRIxAddr "x"
#  define PRIxSize "x"
#  define PRIdSize "d"
#endif

#endif /* Logging_h */

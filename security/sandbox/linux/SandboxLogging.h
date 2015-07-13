/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_SandboxLogging_h
#define mozilla_SandboxLogging_h

#if defined(ANDROID)
#include <android/log.h>
#else
#include <stdio.h>
#endif

#if defined(ANDROID)
#define SANDBOX_LOG_ERROR(args...) __android_log_print(ANDROID_LOG_ERROR, "Sandbox", ## args)
#else
#define SANDBOX_LOG_ERROR(fmt, args...) fprintf(stderr, "Sandbox: " fmt "\n", ## args)
#endif

#endif // mozilla_SandboxLogging_h

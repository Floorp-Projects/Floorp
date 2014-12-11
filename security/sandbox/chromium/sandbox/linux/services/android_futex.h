// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_LINUX_SERVICES_ANDROID_FUTEX_H_
#define SANDBOX_LINUX_SERVICES_ANDROID_FUTEX_H_

#if !defined(FUTEX_PRIVATE_FLAG)
#define FUTEX_PRIVATE_FLAG 128
#endif

#if !defined(FUTEX_CLOCK_REALTIME)
#define FUTEX_CLOCK_REALTIME 256
#endif

#if !defined(FUTEX_CMP_REQUEUE_PI)
#define FUTEX_CMP_REQUEUE_PI 12
#endif

#if !defined(FUTEX_CMP_REQUEUE_PI_PRIVATE)
#define FUTEX_CMP_REQUEUE_PI_PRIVATE (FUTEX_CMP_REQUEUE_PI | FUTEX_PRIVATE_FLAG)
#endif

#if !defined(FUTEX_CMD_MASK)
#define FUTEX_CMD_MASK ~(FUTEX_PRIVATE_FLAG | FUTEX_CLOCK_REALTIME)
#endif

#endif  // SANDBOX_LINUX_SERVICES_ANDROID_FUTEX_H_

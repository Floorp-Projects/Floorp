/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef AndroidUiThread_h__
#define AndroidUiThread_h__

#include <mozilla/RefPtr.h>
#include <nsThread.h>

class MessageLoop;

namespace mozilla {

void CreateAndroidUiThread();
void DestroyAndroidUiThread();
int64_t RunAndroidUiTasks();

MessageLoop* GetAndroidUiThreadMessageLoop();
RefPtr<nsThread> GetAndroidUiThread();

} // namespace mozilla

#endif // AndroidUiThread_h__

/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MacLaunchHelper_h_
#define MacLaunchHelper_h_

#include <stdint.h>

#include <unistd.h>

extern "C" {
  void LaunchChildMac(int aArgc, char** aArgv, uint32_t aRestartType = 0,
                      pid_t *pid = 0);
  bool LaunchElevatedUpdate(int argc, char** argv, uint32_t aRestartType = 0,
                            pid_t* pid = 0);
}

#endif

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <assert.h>
#include <stdio.h>

#include <string>

#include "android/log.h"

#include "progressui.h"

#define LOG(args...)  __android_log_print(ANDROID_LOG_INFO, "GeckoUpdater" , ## args)

using namespace std;

int InitProgressUI(int *argc, char ***argv)
{
  return 0;
}

int ShowProgressUI()
{
  LOG("Starting to apply update ...\n");
  return 0;
}

void QuitProgressUI()
{
  LOG("Finished applying update\n");
}

void UpdateProgressUI(float progress)
{
  assert(0.0f <= progress && progress <= 100.0f);

  static const size_t kProgressBarLength = 50;
  static size_t sLastNumBars;
  size_t numBars = size_t(float(kProgressBarLength) * progress / 100.0f);
  if (numBars == sLastNumBars) {
    return;
  }
  sLastNumBars = numBars;

  size_t numSpaces = kProgressBarLength - numBars;
  string bars(numBars, '=');
  string spaces(numSpaces, ' ');
  LOG("Progress [ %s%s ]\n", bars.c_str(), spaces.c_str());
}

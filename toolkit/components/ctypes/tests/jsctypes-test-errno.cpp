/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdio.h>
#include <errno.h>
#if defined(XP_WIN)
#  include <windows.h>
#endif  // defined(XP_WIN)

#include "jsctypes-test-errno.h"

#define FAIL                                                    \
  {                                                             \
    fprintf(stderr, "Assertion failed at line %i\n", __LINE__); \
    (*(int*)nullptr)++;                                         \
  }

void set_errno(int status) { errno = status; }
int get_errno() { return errno; }

#if defined(XP_WIN)
void set_last_error(int status) { SetLastError((int)status); }
int get_last_error() { return (int)GetLastError(); }
#endif  // defined(XP_WIN)

/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef toolkit_breakpad_mac_utils_h__
#define toolkit_breakpad_mac_utils_h__

#include "nsStringGlue.h"

// Given an Objective-C NSException object, put exception info into a string.
void GetObjCExceptionInfo(void* inException, nsACString& outString);

#endif /* toolkit_breakpad_mac_utils_h__ */

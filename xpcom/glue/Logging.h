/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_logging_h
#define mozilla_logging_h

#include "prlog.h"

// This file is a placeholder for a replacement to the NSPR logging framework
// that is defined in prlog.h. Currently it is just a pass through, but as
// work progresses more functionality will be swapped out in favor of
// mozilla logging implementations.

#define PR_LOG_INFO PR_LOG_DEBUG
#define PR_LOG_VERBOSE (PR_LOG_DEBUG + 1)

#define MOZ_LOG PR_LOG

// Tests if a module has enabled the given log level.
// NB: _module can be null.
#define MOZ_LOG_TEST(_module, _level) \
  ((_module) && (_module)->level >= (_level))

#endif // mozilla_logging_h


/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:set ts=4 sw=4 sts=4 ci et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_LateWriteChecks_h
#define mozilla_LateWriteChecks_h

// This file, along with LateWriteChecks.cpp, serves to check for and report
// late writes. The idea is discover writes to the file system that happens
// during shutdown such that these maybe be moved forward and the process may be
// killed without waiting for static destructors.

namespace mozilla {

/** Different shutdown check modes */
enum ShutdownChecksMode {
  SCM_CRASH,      /** Crash on shutdown check failure */
  SCM_RECORD,     /** Record shutdown check violations */
  SCM_NOTHING     /** Don't attempt any shutdown checks */
};

/**
 * Current shutdown check mode.
 * This variable is defined and initialized in nsAppRunner.cpp
 */
extern ShutdownChecksMode gShutdownChecks;

/**
 * Allocate structures and acquire information from XPCOM necessary to do late
 * write checks. This function must be invoked before BeginLateWriteChecks()
 * and before XPCOM has stopped working.
 */
void InitLateWriteChecks();

/**
 * Begin recording all writes as late-writes. This function should be called
 * when all legitimate writes have occurred. This function does not rely on
 * XPCOM as it is designed to be invoked during XPCOM shutdown.
 *
 * For late-write checks to work you must initialize one or more backends that
 * reports IO through the IOInterposer API. PoisonIOInterposer would probably
 * be the backend of choice in this case.
 *
 * Note: BeginLateWriteChecks() must have been invoked before this function.
 */
void BeginLateWriteChecks();

/**
 * Stop recording all writes as late-writes, call this function when you want
 * late-write checks to stop. I.e. exception handling, or the special case on
 * Mac described in bug 826029.
 */
void StopLateWriteChecks();

} // mozilla

#endif // mozilla_LateWriteChecks_h

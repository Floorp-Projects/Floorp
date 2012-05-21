/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 ci et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_AvailableMemoryTracker_h
#define mozilla_AvailableMemoryTracker_h

namespace mozilla {
namespace AvailableMemoryTracker {

// The AvailableMemoryTracker is implemented only on Windows.  But to make
// callers' lives easier, we stub out empty calls for all its public functions.
// So you can always initialize the AvailableMemoryTracker; it just might not
// do anything.
//
// Init() must be called before any other threads have started, because it
// modifies the in-memory implementations of some DLL functions in
// non-thread-safe ways.
//
// The hooks don't do anything until Activate() is called.  It's an error to
// call Activate() without first calling Init().

#if defined(XP_WIN)
void Init();
void Activate();
#else
void Init() {}
void Activate() {}
#endif

} // namespace AvailableMemoryTracker
} // namespace mozilla

#endif // ifndef mozilla_AvailableMemoryTracker_h

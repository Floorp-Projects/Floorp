/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_AvailableMemoryTracker_h
#define mozilla_AvailableMemoryTracker_h

namespace mozilla {
namespace AvailableMemoryTracker {

// The AvailableMemoryTracker launches a memory pressure watcher on all
// platforms to react to low-memory situations and on Windows it implements
// the full functionality used to monitor how much memory is available.
//
// Init() must be called before any other threads have started, because it
// modifies the in-memory implementations of some DLL functions in
// non-thread-safe ways.
//
// The hooks don't do anything until Activate() is called.  It's an error to
// call Activate() without first calling Init().

void Init();
void Activate();

} // namespace AvailableMemoryTracker
} // namespace mozilla

#endif // ifndef mozilla_AvailableMemoryTracker_h

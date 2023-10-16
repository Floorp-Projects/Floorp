/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_TaskCategory_h
#define mozilla_TaskCategory_h

namespace mozilla {

// The different kinds of tasks we can dispatch to a SystemGroup, TabGroup, or
// DocGroup.
enum class TaskCategory {
  // User input (clicks, keypresses, etc.)
  UI,

  // Data from the network
  Network,

  // setTimeout, setInterval
  Timer,

  // Runnables posted from a worker to the main thread
  Worker,

  // requestIdleCallback
  IdleCallback,

  // Vsync notifications
  RefreshDriver,

  // GC/CC-related tasks
  GarbageCollection,

  // Most DOM events (postMessage, media, plugins)
  Other,

  // Runnables related to Performance Counting
  Performance,

  Count
};

}  // namespace mozilla

#endif  // mozilla_TaskCategory_h

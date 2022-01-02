/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsMemoryPressure_h__
#define nsMemoryPressure_h__

#include "nscore.h"

/*
 * These pre-defined strings are the topic to pass to the observer
 * service to declare the memory-pressure or lift the memory-pressure.
 *
 * 1. Notify kTopicMemoryPressure with kSubTopicLowMemoryNew
 * New memory pressure deteced
 * On a new memory pressure, we stop everything to start cleaning
 * aggresively the memory used, in order to free as much memory as
 * possible.
 *
 * 2. Notify kTopicMemoryPressure with kSubTopicLowMemoryOngoing
 * Repeated memory pressure.
 * A repeated memory pressure implies to clean softly recent allocations.
 * It is supposed to happen after a new memory pressure which already
 * cleaned aggressivley.  So there is no need to damage the reactivity of
 * Gecko by stopping the world again.
 *
 * In case of conflict with an new memory pressue, the new memory pressure
 * takes precedence over an ongoing memory pressure.  The reason being
 * that if no events are processed between 2 notifications (new followed
 * by ongoing, or ongoing followed by a new) we want to be as aggresive as
 * possible on the clean-up of the memory.  After all, we are trying to
 * keep Gecko alive as long as possible.
 *
 * 3. Notify kTopicMemoryPressureStop with nullptr
 * Memory pressure stopped.
 * We're no longer under acute memory pressure, so we might want to have a
 * chance of (cautiously) re-enabling some things we previously turned off.
 * As above, an already enqueued new memory pressure event takes precedence.
 * The priority ordering between concurrent attempts to queue both stopped
 * and ongoing memory pressure is currently not defined.
 */
extern const char* const kTopicMemoryPressure;
extern const char* const kTopicMemoryPressureStop;
extern const char16_t* const kSubTopicLowMemoryNew;
extern const char16_t* const kSubTopicLowMemoryOngoing;

enum class MemoryPressureState : uint32_t {
  None,  // For internal use.  Don't use this value.
  LowMemory,
  NoPressure,
};

/**
 * This function causes the main thread to fire a memory pressure event
 * before processing the next event, but if there are no events pending in
 * the main thread's event queue, the memory pressure event would not be
 * dispatched until one is enqueued. It is infallible and does not allocate
 * any memory.
 *
 * You may call this function from any thread.
 */
void NS_NotifyOfEventualMemoryPressure(MemoryPressureState aState);

/**
 * This function causes the main thread to fire a memory pressure event
 * before processing the next event. We wake up the main thread by adding a
 * dummy event to its event loop, so, unlike with
 * NS_NotifyOfEventualMemoryPressure, this memory-pressure event is always
 * fired relatively quickly, even if the event loop is otherwise empty.
 *
 * You may call this function from any thread.
 */
nsresult NS_NotifyOfMemoryPressure(MemoryPressureState aState);

#endif  // nsMemoryPressure_h__

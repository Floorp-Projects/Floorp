/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CUBEB_TRACING_H
#define CUBEB_TRACING_H

#include <MicroGeckoProfiler.h>

#define CUBEB_REGISTER_THREAD(name)               \
  do {                                            \
    char stacktop;                                \
    uprofiler_register_thread(name, &stacktop);   \
  } while (0)

#define CUBEB_UNREGISTER_THREAD() uprofiler_unregister_thread()

// Insert a tracing marker, with a particular name.
// Phase can be 'x': instant marker, start time but no duration
//              'b': beginning of a marker with a duration
//              'e': end of a marker with a duration
#define CUBEB_TRACE(name, phase) \
  uprofiler_simple_event_marker(name, phase, 0, NULL, NULL, NULL)

#endif // CUBEB_TRACING_H

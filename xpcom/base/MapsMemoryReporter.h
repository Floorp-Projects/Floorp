/* -*- Mode: C++; tab-width: 50; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 ci et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_MapsMemoryReporter_h_
#define mozilla_MapsMemoryReporter_h_

namespace mozilla {
namespace MapsMemoryReporter {

// This only works on Linux, but to make callers' lives easier, we stub out
// empty functions on other platforms.

#if defined(XP_LINUX)
  void Init();
#else
  void Init() {}
#endif

} // namespace MapsMemoryReporter
} // namespace mozilla

#endif

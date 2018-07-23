/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_recordreplay_Trigger_h
#define mozilla_recordreplay_Trigger_h

namespace mozilla {
namespace recordreplay {

// See RecordReplay.h for a description of the record/replay trigger API.

// Initialize trigger state at the beginning of recording or replaying.
void InitializeTriggers();

} // namespace recordreplay
} // namespace mozilla

#endif // mozilla_recordreplay_Trigger_h

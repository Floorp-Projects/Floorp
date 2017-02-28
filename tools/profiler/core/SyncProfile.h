/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __SYNCPROFILE_H
#define __SYNCPROFILE_H

#include "ThreadInfo.h"

class SyncProfile : public ThreadInfo
{
public:
  SyncProfile(int aThreadId, PseudoStack* aStack);
  ~SyncProfile();

  // SyncProfiles' stacks are deduplicated in the context of the containing
  // profile in which the backtrace is as a marker payload.
  void StreamJSON(SpliceableJSONWriter& aWriter, UniqueStacks& aUniqueStacks);

private:
  friend class ProfilerBacktrace;
};

#endif // __SYNCPROFILE_H


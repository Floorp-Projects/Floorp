/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ProfilerBacktrace.h"

#include "ProfileJSONWriter.h"
#include "SyncProfile.h"

ProfilerBacktrace::ProfilerBacktrace(SyncProfile* aProfile)
  : mProfile(aProfile)
{
  MOZ_COUNT_CTOR(ProfilerBacktrace);
  MOZ_ASSERT(aProfile);
}

ProfilerBacktrace::~ProfilerBacktrace()
{
  MOZ_COUNT_DTOR(ProfilerBacktrace);
  if (mProfile->ShouldDestroy()) {
    delete mProfile;
  }
}

void
ProfilerBacktrace::StreamJSON(SpliceableJSONWriter& aWriter,
                              UniqueStacks& aUniqueStacks)
{
  ::MutexAutoLock lock(mProfile->GetMutex());
  mProfile->StreamJSON(aWriter, aUniqueStacks);
}

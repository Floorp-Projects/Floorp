/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __SYNCPROFILE_H
#define __SYNCPROFILE_H

#include "ProfileEntry.h"

struct LinkedUWTBuffer;

class SyncProfile : public ThreadProfile
{
public:
  SyncProfile(ThreadInfo* aInfo, int aEntrySize);
  ~SyncProfile();

  bool SetUWTBuffer(LinkedUWTBuffer* aBuff);
  LinkedUWTBuffer* GetUWTBuffer() { return mUtb; }

  virtual void EndUnwind();
  virtual SyncProfile* AsSyncProfile() { return this; }

private:
  friend class ProfilerBacktrace;

  enum OwnerState
  {
    REFERENCED,       // ProfilerBacktrace has a pointer to this but doesn't own
    OWNED,            // ProfilerBacktrace is responsible for destroying this
    OWNER_DESTROYING, // ProfilerBacktrace owns this and is destroying
    ORPHANED          // No owner, we must destroy ourselves
  };

  bool ShouldDestroy();

  OwnerState        mOwnerState;
  LinkedUWTBuffer*  mUtb;
};

#endif // __SYNCPROFILE_H


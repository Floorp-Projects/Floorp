/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "JSCustomObjectBuilder.h"
#include "JSObjectBuilder.h"
#include "ProfilerBacktrace.h"
#include "SyncProfile.h"

ProfilerBacktrace::ProfilerBacktrace(SyncProfile* aProfile)
  : mProfile(aProfile)
{
  MOZ_ASSERT(aProfile);
}

ProfilerBacktrace::~ProfilerBacktrace()
{
  if (mProfile->ShouldDestroy()) {
    delete mProfile;
  }
}

template<typename Builder> void
ProfilerBacktrace::BuildJSObject(Builder& aObjBuilder,
                                 typename Builder::ObjectHandle aScope)
{
  mozilla::MutexAutoLock lock(*mProfile->GetMutex());
  mProfile->BuildJSObject(aObjBuilder, aScope);
}

template void
ProfilerBacktrace::BuildJSObject<JSCustomObjectBuilder>(
                                    JSCustomObjectBuilder& aObjBuilder,
                                    JSCustomObjectBuilder::ObjectHandle aScope);
template void
ProfilerBacktrace::BuildJSObject<JSObjectBuilder>(JSObjectBuilder& aObjBuilder,
                                          JSObjectBuilder::ObjectHandle aScope);

/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "mozilla/Types.h"
#include "utils/RefBase.h"
#include "utils/String16.h"
#include "utils/String8.h"

namespace android {
MOZ_EXPORT RefBase::RefBase() : mRefs(0)
{
}

MOZ_EXPORT RefBase::~RefBase()
{
}

MOZ_EXPORT void RefBase::incStrong(const void *id) const
{
}

MOZ_EXPORT void RefBase::decStrong(const void *id) const
{
}

MOZ_EXPORT void RefBase::onFirstRef()
{
}

MOZ_EXPORT void RefBase::onLastStrongRef(const void* id)
{
}

MOZ_EXPORT bool RefBase::onIncStrongAttempted(uint32_t flags, const void* id)
{
  return false;
}

MOZ_EXPORT void RefBase::onLastWeakRef(void const* id)
{
}

MOZ_EXPORT String16::String16(char const*)
{
}

MOZ_EXPORT String16::~String16()
{
}

MOZ_EXPORT String8::String8()
{
}

MOZ_EXPORT String8::~String8()
{
}
}

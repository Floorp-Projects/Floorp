/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 ci et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Private interface for code shared between the platforms of mozPoisonWriteXXX

#ifndef MOZPOISONWRITEBASE_H
#define MOZPOISONWRITEBASE_H

#include <stdio.h>
#include <vector>
#include "nspr.h"
#include "mozilla/NullPtr.h"
#include "mozilla/Util.h"
#include "mozilla/Scoped.h"

namespace mozilla {
bool PoisonWriteEnabled();
bool ValidWriteAssert(bool ok);
bool IsDebugFile(intptr_t aFileID);
intptr_t FileDescriptorToID(int aFd);
}

#endif

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <string.h>
#include "nsTArray.h"
#include "nsXPCOM.h"
#include "nsDebug.h"
#include "mozilla/CheckedInt.h"

nsTArrayHeader nsTArrayHeader::sEmptyHdr = { 0, 0, 0 };

bool
IsTwiceTheRequiredBytesRepresentableAsUint32(size_t aCapacity, size_t aElemSize)
{
  using mozilla::CheckedUint32;
  return ((CheckedUint32(aCapacity) * aElemSize) * 2).isValid();
}

MOZ_NORETURN MOZ_COLD void
InvalidArrayIndex_CRASH(size_t aIndex, size_t aLength)
{
  const size_t CAPACITY = 512;
  // Leak the buffer on the heap to make sure that it lives long enough, as
  // MOZ_CRASH_ANNOTATE expects the pointer passed to it to live to the end of
  // the program.
  auto* buffer = new char[CAPACITY];
  snprintf(buffer, CAPACITY,
           "ElementAt(aIndex = %llu, aLength = %llu)",
           (long long unsigned) aIndex,
           (long long unsigned) aLength);
  MOZ_CRASH_ANNOTATE(buffer);
  MOZ_REALLY_CRASH();
}

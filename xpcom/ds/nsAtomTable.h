/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsAtomTable_h__
#define nsAtomTable_h__

#include "mozilla/MemoryReporting.h"
#include <stddef.h>

void NS_InitAtomTable();
void NS_ShutdownAtomTable();

namespace mozilla {
struct AtomsSizes
{
  size_t mTable;
  size_t mDynamicAtoms;

  AtomsSizes()
   : mTable(0)
   , mDynamicAtoms(0)
  {}
};
} // namespace mozilla

void NS_AddSizeOfAtoms(mozilla::MallocSizeOf aMallocSizeOf,
                       mozilla::AtomsSizes& aSizes);

#endif // nsAtomTable_h__

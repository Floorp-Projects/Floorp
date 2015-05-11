/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/HashFunctions.h"

#include "ProfileJSONWriter.h"

void
SpliceableJSONWriter::Splice(const ChunkedJSONWriteFunc* aFunc)
{
  Separator();
  for (size_t i = 0; i < aFunc->mChunkList.length(); i++) {
    WriteFunc()->Write(aFunc->mChunkList[i].get());
  }
  mNeedComma[mDepth] = true;
}

void
SpliceableJSONWriter::Splice(const char* aStr)
{
  Separator();
  WriteFunc()->Write(aStr);
  mNeedComma[mDepth] = true;
}

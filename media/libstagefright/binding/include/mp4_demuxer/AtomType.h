/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ATOM_TYPE_H_
#define ATOM_TYPE_H_

#include <stdint.h>
#include "mozilla/Endian.h"

using namespace mozilla;

namespace mp4_demuxer {

class AtomType
{
public:
  AtomType() : mType(0) { }
  MOZ_IMPLICIT AtomType(uint32_t aType) : mType(aType) { }
  MOZ_IMPLICIT AtomType(const char* aType) : mType(BigEndian::readUint32(aType)) { }
  bool operator==(const AtomType& aType) const { return mType == aType.mType; }
  bool operator!() const { return !mType; }

private:
  uint32_t mType;
};
}

#endif

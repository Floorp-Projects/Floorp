/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsStaticAtomUtils_h
#define nsStaticAtomUtils_h

#include <stdint.h>
#include "nsAtom.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/Maybe.h"

// This class holds basic operations on arrays of static atoms.
class nsStaticAtomUtils {
 public:
  static mozilla::Maybe<uint32_t> Lookup(nsAtom* aAtom,
                                         const nsStaticAtom* aAtoms,
                                         uint32_t aCount) {
    if (aAtom->IsStatic()) {
      ptrdiff_t index = aAtom->AsStatic() - aAtoms;
      if (index >= 0 && index < static_cast<ptrdiff_t>(aCount)) {
        return mozilla::Some(static_cast<uint32_t>(index));
      }
    }
    return mozilla::Nothing();
  }

  static bool IsMember(nsAtom* aAtom, const nsStaticAtom* aAtoms,
                       uint32_t aCount) {
    return Lookup(aAtom, aAtoms, aCount).isSome();
  }
};

#endif  // nsStaticAtomUtils_h

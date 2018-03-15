/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsGkAtoms_h___
#define nsGkAtoms_h___

#include "nsStaticAtom.h"

class nsGkAtoms
{
public:
  static void AddRefAtoms();

  #define GK_ATOM(_name, _value) NS_STATIC_ATOM_DECL(_name)
  #include "nsGkAtomList.h"
  #undef GK_ATOM
};

#endif /* nsGkAtoms_h___ */

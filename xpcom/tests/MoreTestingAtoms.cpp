/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MoreTestingAtoms.h"
#include "nsStaticAtom.h"
#include "nsMemory.h"

// define storage for all atoms
#define MORE_TESTING_ATOM(_name, _value) nsIAtom* MoreTestingAtoms::_name;
#include "MoreTestingAtomList.h"
#undef MORE_TESTING_ATOM

#define MORE_TESTING_ATOM(name_, value_) NS_STATIC_ATOM_BUFFER(name_##_buffer, value_)
#include "MoreTestingAtomList.h"
#undef MORE_TESTING_ATOM

static const nsStaticAtom MoreTestingAtoms_info[] = {

#define MORE_TESTING_ATOM(name_, value_) NS_STATIC_ATOM(name_##_buffer, &MoreTestingAtoms::name_),
#include "MoreTestingAtomList.h"
#undef MORE_TESTING_ATOM
};

void MoreTestingAtoms::AddRefAtoms()
{
  NS_RegisterStaticAtoms(MoreTestingAtoms_info);
}

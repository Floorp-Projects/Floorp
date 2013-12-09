/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TestingAtoms.h"
#include "nsStaticAtom.h"
#include "nsMemory.h"

// define storage for all atoms
#define TESTING_ATOM(_name, _value) nsIAtom* TestingAtoms::_name;
#include "TestingAtomList.h"
#undef TESTING_ATOM

#define TESTING_ATOM(name_, value_) NS_STATIC_ATOM_BUFFER(name_##_buffer, value_)
#include "TestingAtomList.h"
#undef TESTING_ATOM

static const nsStaticAtom TestingAtoms_info[] = {
#define TESTING_ATOM(name_, value_) NS_STATIC_ATOM(name_##_buffer, &TestingAtoms::name_),
#include "TestingAtomList.h"
#undef TESTING_ATOM
};

void TestingAtoms::AddRefAtoms()
{
  NS_RegisterStaticAtoms(TestingAtoms_info);
}

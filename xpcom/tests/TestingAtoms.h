/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
 
#ifndef TestingAtoms_h_
#define TestingAtoms_h_

#include "nsIAtom.h"

class TestingAtoms {
  public:
    static void AddRefAtoms();
#define TESTING_ATOM(_name, _value) static nsIAtom* _name;
#include "TestingAtomList.h"
#undef TESTING_ATOM
};

#endif /* TestingAtoms_h_ */

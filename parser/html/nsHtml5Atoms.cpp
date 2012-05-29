/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * This class wraps up the creation (and destruction) of the standard
 * set of atoms used by the HTML5 parser; the atoms are created when
 * nsHtml5Module is loaded and they are destroyed when nsHtml5Module is
 * unloaded.
 */

#include "mozilla/Util.h"

#include "nsHtml5Atoms.h"
#include "nsStaticAtom.h"

using namespace mozilla;

// define storage for all atoms
#define HTML5_ATOM(_name, _value) nsIAtom* nsHtml5Atoms::_name;
#include "nsHtml5AtomList.h"
#undef HTML5_ATOM

#define HTML5_ATOM(name_, value_) NS_STATIC_ATOM_BUFFER(name_##_buffer, value_)
#include "nsHtml5AtomList.h"
#undef HTML5_ATOM

static const nsStaticAtom Html5Atoms_info[] = {
#define HTML5_ATOM(name_, value_) NS_STATIC_ATOM(name_##_buffer, &nsHtml5Atoms::name_),
#include "nsHtml5AtomList.h"
#undef HTML5_ATOM
};

void nsHtml5Atoms::AddRefAtoms()
{
  NS_RegisterStaticAtoms(Html5Atoms_info);
}

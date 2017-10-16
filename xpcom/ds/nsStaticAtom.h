/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsStaticAtom_h__
#define nsStaticAtom_h__

#include "nsAtom.h"
#include "nsStringBuffer.h"

#define NS_STATIC_ATOM(buffer_name, atom_ptr) \
  { buffer_name, atom_ptr }

// Note that |str_data| is an 8-bit string, and so |sizeof(str_data)| is equal
// to the number of chars (including the terminating '\0'). The |u""| prefix
// converts |str_data| to a 16-bit string, which is assigned.
#define NS_STATIC_ATOM_BUFFER(buffer_name, str_data) \
  static const char16_t buffer_name[sizeof(str_data)] = u"" str_data; \
  static_assert(sizeof(str_data[0]) == 1, "non-8-bit static atom literal");

/**
 * Holds data used to initialize large number of atoms during startup. Use
 * the above macros to initialize these structs. They should never be accessed
 * directly other than from AtomTable.cpp.
 */
struct nsStaticAtom
{
  const char16_t* const mString;
  nsAtom** const mAtom;
};

// Register an array of static atoms with the atom table
template<uint32_t N>
void
NS_RegisterStaticAtoms(const nsStaticAtom (&aAtoms)[N])
{
  extern void RegisterStaticAtoms(const nsStaticAtom*, uint32_t aAtomCount);
  RegisterStaticAtoms(aAtoms, N);
}

#endif

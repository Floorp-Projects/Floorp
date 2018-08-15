/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsGkAtoms.h"

// Register an array of static atoms with the atom table.
void
NS_RegisterStaticAtoms(const nsStaticAtom* aAtoms, size_t aAtomsLen);

namespace mozilla {
namespace detail {

extern constexpr GkAtoms gGkAtoms = {
  // The initialization of each atom's string.
  #define GK_ATOM(name_, value_, hash_, type_, atom_type_) \
    u"" value_,
  #include "nsGkAtomList.h"
  #undef GK_ATOM
  {
    // The initialization of the atoms themselves.
    //
    // Note that |value_| is an 8-bit string, and so |sizeof(value_)| is equal
    // to the number of chars (including the terminating '\0'). The |u""| prefix
    // converts |value_| to a 16-bit string.
    #define GK_ATOM(name_, value_, hash_, type_, atom_type_)              \
      nsStaticAtom(u"" value_,                                            \
          sizeof(value_) - 1,                                             \
          hash_,                                                          \
          offsetof(GkAtoms,                                               \
                   mAtoms[static_cast<size_t>(GkAtoms::Atoms::name_)]) -  \
          offsetof(GkAtoms, name_##_string)),
    #include "nsGkAtomList.h"
    #undef GK_ATOM
  }
};

} // namespace detail
} // namespace mozilla

const nsStaticAtom* const nsGkAtoms::sAtoms = mozilla::detail::gGkAtoms.mAtoms;

// Definition of the pointer to the static atom.
#define GK_ATOM(name_, value_, hash_, type_, atom_type_)                   \
  type_* nsGkAtoms::name_ = const_cast<type_*>(static_cast<const type_*>(  \
    &mozilla::detail::gGkAtoms.mAtoms[                                     \
      static_cast<size_t>(mozilla::detail::GkAtoms::Atoms::name_)]));
#include "nsGkAtomList.h"
#undef GK_ATOM

void nsGkAtoms::RegisterStaticAtoms()
{
  NS_RegisterStaticAtoms(sAtoms, sAtomsLen);
}


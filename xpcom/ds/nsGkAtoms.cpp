/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsGkAtoms.h"

namespace mozilla {
namespace detail {

// Because this is `constexpr` it ends up in read-only memory where it can be
// shared between processes.
extern constexpr GkAtoms gGkAtoms = {
// The initialization of each atom's string.
//
// Expansion of the example GK_ATOM entries in nsGkAtoms.h:
//
//   u"a",
//   u"bb",
//   u"Ccc",
//
#define GK_ATOM(name_, value_, hash_, is_ascii_lower_, type_, atom_type_) \
  u"" value_,
#include "nsGkAtomList.h"
#undef GK_ATOM
    {
// The initialization of the atoms themselves.
//
// Note that |value_| is an 8-bit string, and so |sizeof(value_)| is equal
// to the number of chars (including the terminating '\0'). The |u""| prefix
// converts |value_| to a 16-bit string.
//
// Expansion of the example GK_ATOM entries in nsGkAtoms.h:
//
//   nsStaticAtom(
//     1,
//     0x01234567,
//     offsetof(GkAtoms, mAtoms[static_cast<size_t>(GkAtoms::Atoms::a)]) -
//       offsetof(GkAtoms, a_string),
//     true),
//
//   nsStaticAtom(
//     2,
//     0x12345678,
//     offsetof(GkAtoms, mAtoms[static_cast<size_t>(GkAtoms::Atoms::bb)]) -
//       offsetof(GkAtoms, bb_string),
//     false),
//
//   nsStaticAtom(
//     3,
//     0x23456789,
//     offsetof(GkAtoms, mAtoms[static_cast<size_t>(GkAtoms::Atoms::Ccc)]) -
//       offsetof(GkAtoms, Ccc_string),
//     false),
//
#define GK_ATOM(name_, value_, hash_, is_ascii_lower_, type_, atom_type_)     \
  nsStaticAtom(                                                               \
      sizeof(value_) - 1, hash_,                                              \
      offsetof(GkAtoms, mAtoms[static_cast<size_t>(GkAtoms::Atoms::name_)]) - \
          offsetof(GkAtoms, name_##_string),                                  \
      is_ascii_lower_),
#include "nsGkAtomList.h"
#undef GK_ATOM
    }};

}  // namespace detail
}  // namespace mozilla

const nsStaticAtom* const nsGkAtoms::sAtoms = mozilla::detail::gGkAtoms.mAtoms;

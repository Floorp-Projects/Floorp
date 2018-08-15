/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsGkAtoms_h___
#define nsGkAtoms_h___

#include "nsAtom.h"
#include "nsStaticAtom.h"

// Trivial subclasses of nsStaticAtom so that function signatures can require
// an atom from a specific atom list.
#define DEFINE_STATIC_ATOM_SUBCLASS(name_)                                    \
  class name_ : public nsStaticAtom                                           \
  {                                                                           \
  public:                                                                     \
    constexpr name_(const char16_t* aStr, uint32_t aLength, uint32_t aOffset) \
      : nsStaticAtom(aStr, aLength, aOffset) {}                               \
  };

DEFINE_STATIC_ATOM_SUBCLASS(nsICSSAnonBoxPseudo)
DEFINE_STATIC_ATOM_SUBCLASS(nsICSSPseudoElement)

#undef DEFINE_STATIC_ATOM_SUBCLASS

namespace mozilla {
namespace detail {

struct GkAtoms
{
  #define GK_ATOM(name_, value_, type_, atom_type_) NS_STATIC_ATOM_DECL_STRING(name_, value_)
  #include "nsGkAtomList.h"
  #undef GK_ATOM

  enum class Atoms {
    #define GK_ATOM(name_, value_, type_, atom_type_) NS_STATIC_ATOM_ENUM(name_)
    #include "nsGkAtomList.h"
    #undef GK_ATOM
    AtomsCount
  };

  const nsStaticAtom mAtoms[static_cast<size_t>(Atoms::AtomsCount)];
};

} // namespace detail
} // namespace mozilla

class nsGkAtoms
{
private:
  static const nsStaticAtom* const sAtoms;
  static constexpr size_t sAtomsLen =
    static_cast<size_t>(mozilla::detail::GkAtoms::Atoms::AtomsCount);

public:
  static void RegisterStaticAtoms();

  static nsStaticAtom* GetAtomByIndex(size_t aIndex)
  {
    MOZ_ASSERT(aIndex < sAtomsLen);
    return const_cast<nsStaticAtom*>(&sAtoms[aIndex]);
  }

  #define GK_ATOM(name_, value_, type_, atom_type_) NS_STATIC_ATOM_DECL_PTR(type_, name_)
  #include "nsGkAtomList.h"
  #undef GK_ATOM
};

#endif /* nsGkAtoms_h___ */

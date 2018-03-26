/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsStaticAtom_h__
#define nsStaticAtom_h__

#include <stdint.h>
#include "nsAtom.h"
#include "mozilla/Maybe.h"

// The following macros are used to define static atoms, typically in
// conjunction with a .h file that defines the names and values of the atoms.
//
// For example, the .h file might be called MyAtomList.h and look like this:
//
//   MY_ATOM(one, "one")
//   MY_ATOM(two, "two")
//   MY_ATOM(three, "three")
//
// The code defining the static atoms should look something like this:
//
//   ====> MyAtoms.h <====
//
//   namespace detail {
//
//   struct MyAtoms
//   {
//     #define MY_ATOM(name_, value_) NS_STATIC_ATOM_DECL_STRING(name_, value_)
//     #include "MyAtomList.h"
//     #undef MY_ATOM
//
//     #define MY_ATOM(name_, value_) NS_STATIC_ATOM_DECL_ATOM(name_)
//     #include "MyAtomList.h"
//     #undef MY_ATOM
//   };
//
//   extern const MyAtoms gMyAtoms;
//
//   } // namespace detail
//
//   class nsMyAtoms
//   {
//   public:
//     #define MY_ATOM(name_, value_) NS_STATIC_ATOM_DECL_PTR(name_)
//     #include "MyAtomList.h"
//     #undef MY_ATOM
//   };
//
//   ====> MyAtoms.cpp <====
//
//   MOZ_PUSH_DISABLE_INTEGRAL_CONSTANT_OVERFLOW_WARNING
//   static constexpr MyAtoms gMyAtoms = {
//     #define MY_ATOM(name_, value_) NS_STATIC_ATOM_INIT_STRING(value_)
//     #include "MyAtomList.h"
//     #undef MY_ATOM
//
//     #define MY_ATOM(name_, value_) NS_STATIC_ATOM_INIT_ATOM(MyAtoms, name_, value_)
//     #include "MyAtomList.h"
//     #undef MY_ATOM
//   };
//   MOZ_POP_DISABLE_INTEGRAL_CONSTANT_OVERFLOW_WARNING
//
//   #define MY_ATOM(name_, value_) NS_STATIC_ATOM_DEFN_PTR(nsMyAtoms, name_)
//   #include "MyAtomList.h"
//   #undef MY_ATOM
//
//   static const nsStaticAtomSetup sMyAtomSetup[] = {
//     #define MY_ATOM(name_, value_) NS_STATIC_ATOM_SETUP(mozilla::detail::gMyAtoms, nsMyAtoms, name_)
//     #include "MyAtomList.h"
//     #undef MY_ATOM
//   };
//
// The macros expand to the following:
//
//   ====> MyAtoms.h <====
//
//   // A `detail` namespace is used because the things within it aren't
//   // directly referenced by external users of these static atoms.
//   namespace detail {
//
//   // This `detail` class contains the atom strings and the atom objects.
//   // Because they are together in a class, the mStringOffset field of the
//   // atoms will be small and can be initialized at compile time.
//   struct MyAtoms
//   {
//     const char16_t one_string[4];
//     const char16_t two_string[4];
//     const char16_t three_string[6];
//
//     const nsStaticAtom one_atom;
//     const nsStaticAtom two_atom;
//     const nsStaticAtom three_atom;
//   };
//
//   extern const MyAtoms gMyAtoms;
//
//   } // namespace detail
//
//   // This class holds the pointers to the individual atoms.
//   class nsMyAtoms
//   {
//   public:
//     static nsStaticAtom* one;
//     static nsStaticAtom* two;
//     static nsStaticAtom* three;
//   };
//
//   ====> MyAtoms.cpp <====
//
//   // Need to suppress some MSVC warning weirdness with WrappingMultiply().
//   MOZ_PUSH_DISABLE_INTEGRAL_CONSTANT_OVERFLOW_WARNING
//   // Because this is `constexpr` it ends up in read-only memory where it can
//   // be shared between processes.
//   extern constexpr MyAtoms gMyAtoms = {
//     u"one",
//     u"two",
//     u"three",
//
//     nsStaticAtom(u"one", 3, offsetof(MyAtoms, one_atom) -
//                             offsetof(MyAtoms, one_string)),
//     nsStaticAtom(u"two", 3, offsetof(MyAtoms, two_atom) -
//                             offsetof(MyAtoms, two_string)),
//     nsStaticAtom(u"three", 3, offsetof(MyAtoms, three_atom) -
//                               offsetof(MyAtoms, three_string)),
//   };
//   MOZ_POP_DISABLE_INTEGRAL_CONSTANT_OVERFLOW_WARNING
//
//   nsStaticAtom* MyAtoms::one;
//   nsStaticAtom* MyAtoms::two;
//   nsStaticAtom* MyAtoms::three;
//
//   static const nsStaticAtomSetup sMyAtomSetup[] = {
//     { &detail::gMyAtoms.one_atom, &nsMyAtoms::one },
//     { &detail::gMyAtoms.two_atom, &nsMyAtoms::two },
//     { &detail::gMyAtoms.three_atom, &nsMyAtoms::three },
//   };
//
// When RegisterStaticAtoms(sMyAtomSetup) is called it iterates over
// sMyAtomSetup[]. E.g. for the first atom it does roughly the following:
// - MyAtoms::one = one_atom
// - inserts one_atom into the atom table

// The declaration of the atom's string, which must be within the same `detail`
// class as NS_STATIC_ATOM_DECL_ATOM.
#define NS_STATIC_ATOM_DECL_STRING(name_, value_) \
  const char16_t name_##_string[sizeof(value_)];

// The declaration of the static atom itself, which must be within the same
// `detail` class as NS_STATIC_ATOM_DECL_STRING.
#define NS_STATIC_ATOM_DECL_ATOM(name_) \
  const nsStaticAtom name_##_atom;

// The declaration of the pointer to the static atom, which must be within
// a class.
#define NS_STATIC_ATOM_DECL_PTR(name_) \
  static nsStaticAtom* name_;

// Like NS_STATIC_ATOM_DECL, but for sub-classes of nsStaticAtom.
#define NS_STATIC_ATOM_SUBCLASS_DECL_PTR(type_, name_) \
  static type_* name_;

// The initialization of the atom's string, which must be within a `constexpr`
// instance of the `detail` class.
#define NS_STATIC_ATOM_INIT_STRING(value_) \
  u"" value_,

// The initialization of the static atom itself, which must be within a
// `constexpr` instance of the `detail` class.
//
// Note that |value_| is an 8-bit string, and so |sizeof(value_)| is equal
// to the number of chars (including the terminating '\0'). The |u""| prefix
// converts |value_| to a 16-bit string.
#define NS_STATIC_ATOM_INIT_ATOM(class_, name_, value_) \
  nsStaticAtom(u"" value_, \
               sizeof(value_) - 1, \
               offsetof(class_, name_##_atom) - \
               offsetof(class_, name_##_string)),

// The definition of the pointer to the static atom. Initially null, it is
// set by RegisterStaticAtoms() to point to a heap-allocated nsStaticAtom.
#define NS_STATIC_ATOM_DEFN_PTR(class_, name_) \
  nsStaticAtom* class_::name_;

// Like NS_STATIC_ATOM_DEFN, but for sub-classes of nsStaticAtom.
#define NS_STATIC_ATOM_SUBCLASS_DEFN_PTR(type_, class_, name_) \
  type_* class_::name_;

// The StaticAtomSetup. Used during start-up, and in some cases afterwards.
#define NS_STATIC_ATOM_SETUP(detailObj_, class_, name_) \
  { &detailObj_.name_##_atom, &class_::name_ },

// Like NS_STATIC_ATOM_SUBCLASS, but for sub-classes of nsStaticAtom.
#define NS_STATIC_ATOM_SUBCLASS_SETUP(detailObj_, class_, name_) \
  { &detailObj_.name_##_atom, \
    reinterpret_cast<nsStaticAtom**>(&class_::name_) },

// Holds data used to initialize large number of atoms during startup. Use
// NS_STATIC_ATOM_SETUP to initialize these structs.
struct nsStaticAtomSetup
{
  const nsStaticAtom* const mAtom;
  nsStaticAtom** const mAtomp;
};

// Register an array of static atoms with the atom table.
template<uint32_t N>
void
NS_RegisterStaticAtoms(const nsStaticAtomSetup (&aSetup)[N])
{
  extern void RegisterStaticAtoms(const nsStaticAtomSetup* aSetup,
                                  uint32_t aCount);
  RegisterStaticAtoms(aSetup, N);
}

// This class holds basic operations on arrays of static atoms.
class nsStaticAtomUtils {
public:
  template<uint32_t N>
  static mozilla::Maybe<uint32_t> Lookup(nsAtom *aAtom,
                                         const nsStaticAtomSetup (&aSetup)[N])
  {
    return Lookup(aAtom, aSetup, N);
  }

  template<uint32_t N>
  static bool IsMember(nsAtom *aAtom, const nsStaticAtomSetup (&aSetup)[N])
  {
    return Lookup(aAtom, aSetup, N).isSome();
  }

private:
  static mozilla::Maybe<uint32_t> Lookup(nsAtom* aAtom,
                                         const nsStaticAtomSetup* aSetup,
                                         uint32_t aCount)
  {
    for (uint32_t i = 0; i < aCount; i++) {
      if (aAtom == *(aSetup[i].mAtomp)) {
        return mozilla::Some(i);
      }
    }
    return mozilla::Nothing();
  }
};

#endif

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsStaticAtom_h__
#define nsStaticAtom_h__

#include <stdint.h>
#include "nsAtom.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/Maybe.h"

// Static atoms are structured carefully to satisfy a lot of constraints.
//
// - We have ~2700 static atoms. They are divided across ~4 classes, with the
//   majority in nsGkAtoms.
//
// - We want them to be constexpr so they end up in .rodata, and thus shared
//   between processes, minimizing memory usage.
//
// - We need them to be in an array, so we can iterate over them (for
//   registration and lookups).
//
// - Each static atom has a string literal associated with it. We can't use a
//   pointer to the string literal because then the atoms won't end up in
//   .rodata. Therefore the string literals and the atoms must be arranged in a
//   way such that a numeric index can be used instead. This numeric index
//   (nsStaticAtom::mStringOffset) must be computable at compile-time to keep
//   the static atom constexpr. It should also not be too large (a uint32_t is
//   reasonable).
//
// - Each static atom stores the hash value of its associated string literal;
//   it's used in various ways. The hash value must be computed at
//   compile-time, to keep the static atom constexpr.
//
// - As well as accessing each static atom via array indexing, we need an
//   individual pointer, e.g. nsGkAtoms::foo. Ideally this would be constexpr
//   so it doesn't take up any space in memory. Unfortunately MSVC's constexpr
//   support is buggy and so this isn't possible yet. See bug 1449787.
//
// - The array of static atoms can't be in a .h file, because it's a huge
//   constexpr expression, which would blow out compile times. But the
//   individual pointers for the static atoms must be in a .h file so they are
//   public.
//
// The macros below are used to define static atoms in a way that satisfies
// these constraints. They are used in conjunction with a .h file that defines
// the names and values of the atoms.
//
// For example, the .h file might be called MyAtomList.h and look like this:
//
//   MY_ATOM(one, "one")
//   MY_ATOM(two, "two")
//   MY_ATOM(three, "three")
//
// The code defining the static atoms should look something like the following.
// ("<<<"/"---"/">>>" markers are used below to indicate what the macros look
// like before and after expansion.)
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
//     <<<
//     #define MY_ATOM(name_, value_) NS_STATIC_ATOM_DECL_STRING(name_, value_)
//     #include "MyAtomList.h"
//     #undef MY_ATOM
//     ---
//     const char16_t one_string[4];
//     const char16_t two_string[4];
//     const char16_t three_string[6];
//     >>>
//
//     enum class Atoms {
//       <<<
//       #define MY_ATOM(name_, value_) NS_STATIC_ATOM_ENUM(name_)
//       #include "MyAtomList.h"
//       #undef MY_ATOM
//       ---
//       one,
//       two,
//       three,
//       >>>
//       AtomsCount
//     };
//
//     const nsStaticAtom mAtoms[static_cast<size_t>(Atoms::AtomsCount)];
//   };
//
//   } // namespace detail
//
//   // This class holds the pointers to the individual atoms.
//   class nsMyAtoms
//   {
//   private:
//     // This is a useful handle to the array of atoms, used below and also
//     // possibly by Rust code.
//     static const nsStaticAtom* const sAtoms;
//
//     // The number of atoms, used below.
//     static constexpr size_t sAtomsLen =
//       static_cast<size_t>(detail::MyAtoms::Atoms::AtomsCount);
//
//   public:
//     // The type is not `nsStaticAtom* const` -- even though these atoms are
//     // immutable -- because they are often passed to functions with
//     // `nsAtom*` parameters, i.e. that can be passed both dynamic and
//     // static.
//     <<<
//     #define MY_ATOM(name_, value_) NS_STATIC_ATOM_DECL_PTR(nsStaticAtom, name_)
//     #include "MyAtomList.h"
//     #undef MY_ATOM
//     ---
//     static nsStaticAtom* one;
//     static nsStaticAtom* two;
//     static nsStaticAtom* three;
//     >>>
//   };
//
//   ====> MyAtoms.cpp <====
//
//   namespace detail {
//
//   // Need to suppress some MSVC warning weirdness with WrappingMultiply().
//   MOZ_PUSH_DISABLE_INTEGRAL_CONSTANT_OVERFLOW_WARNING
//   // Because this is `constexpr` it ends up in read-only memory where it can
//   // be shared between processes.
//   static constexpr MyAtoms gMyAtoms = {
//     <<<
//     #define MY_ATOM(name_, value_) NS_STATIC_ATOM_INIT_STRING(value_)
//     #include "MyAtomList.h"
//     #undef MY_ATOM
//     ---
//     u"one",
//     u"two",
//     u"three",
//     >>>
//     {
//       <<<
//       #define MY_ATOM(name_, value_) NS_STATIC_ATOM_INIT_ATOM(nsStaticAtom, MyAtoms, name_, value_)
//       #include "MyAtomList.h"
//       #undef MY_ATOM
//       ---
//       nsStaticAtom(
//         u"one", 3,
//         offsetof(MyAtoms, mAtoms[static_cast<size_t>(MyAtoms::Atoms::one)]) -
//         offsetof(MyAtoms, one_string)),
//       nsStaticAtom(
//         u"two", 3,
//         offsetof(MyAtoms, mAtoms[static_cast<size_t>(MyAtoms::Atoms::two)]) -
//         offsetof(MyAtoms, two_string)),
//       nsStaticAtom(
//         u"three", 3,
//         offsetof(MyAtoms, mAtoms[static_cast<size_t>(MyAtoms::Atoms::three)]) -
//         offsetof(MyAtoms, three_string)),
//       >>>
//     }
//   };
//   MOZ_POP_DISABLE_INTEGRAL_CONSTANT_OVERFLOW_WARNING
//
//   } // namespace detail
//
//   const nsStaticAtom* const nsMyAtoms::sAtoms =
//     mozilla::detail::gMyAtoms.mAtoms;
//
//   <<<
//   #define MY_ATOM(name_, value_) NS_STATIC_ATOM_DEFN_PTR(nsStaticAtom, detail::MyAtoms, detail::gMyAtoms, nsMyAtoms, name_)
//   #include "MyAtomList.h"
//   #undef MY_ATOM
//   ---
//   nsStaticAtom* nsMyAtoms::one =
//     const_cast<nsStaticAtom*>(&detail::gMyAtoms.mAtoms[
//       static_cast<size_t>(detail::MyAtoms::Atoms::one)]);
//   nsStaticAtom* nsMyAtoms::two =
//     const_cast<nsStaticAtom*>(&detail::gMyAtoms.mAtoms[
//       static_cast<size_t>(detail::MyAtoms::Atoms::two)]);
//   nsStaticAtom* nsMyAtoms::three =
//     const_cast<nsStaticAtom*>(&detail::gMyAtoms.mAtoms[
//       static_cast<size_t>(detail::MyAtoms::Atoms::three)]);
//   >>>
//
// When NS_RegisterStaticAtoms(sAtoms, sAtomsLen) is called it iterates
// over the atoms, inserting them into the atom table.

// The declaration of the atom's string.
#define NS_STATIC_ATOM_DECL_STRING(name_, value_) \
  const char16_t name_##_string[sizeof(value_)];

// The enum value for the atom.
#define NS_STATIC_ATOM_ENUM(name_) \
  name_,

// The declaration of the pointer to the static atom. `type_` must be
// `nsStaticAtom` or a subclass thereof.
// XXX: Eventually this should be combined with NS_STATIC_ATOM_DEFN_PTR and the
// pointer should be made `constexpr`. See bug 1449787.
#define NS_STATIC_ATOM_DECL_PTR(type_, name_) \
  static type_* name_;

// The initialization of the atom's string.
#define NS_STATIC_ATOM_INIT_STRING(value_) \
  u"" value_,

// The initialization of the atom itself. `type_` must be `nsStaticAtom` or a
// subclass thereof.
//
// Note that |value_| is an 8-bit string, and so |sizeof(value_)| is equal
// to the number of chars (including the terminating '\0'). The |u""| prefix
// converts |value_| to a 16-bit string.
#define NS_STATIC_ATOM_INIT_ATOM(type_, detailClass_, name_, value_) \
  type_(u"" value_, \
        sizeof(value_) - 1, \
        offsetof(detailClass_, \
                 mAtoms[static_cast<size_t>(detailClass_::Atoms::name_)]) - \
        offsetof(detailClass_, name_##_string)),

// Definition of the pointer to the static atom. `type_` must be `nsStaticAtom`
// or a subclass thereof.
#define NS_STATIC_ATOM_DEFN_PTR(type_, detailClass_, detailObj_, class_, name_) \
  type_* class_::name_ = const_cast<type_*>( \
    &detailObj_.mAtoms[static_cast<size_t>(detailClass_::Atoms::name_)]);

// Register an array of static atoms with the atom table.
void
NS_RegisterStaticAtoms(const nsStaticAtom* aAtoms, size_t aAtomsLen);

// This class holds basic operations on arrays of static atoms.
class nsStaticAtomUtils {
public:
  static mozilla::Maybe<uint32_t> Lookup(nsAtom* aAtom,
                                         const nsStaticAtom* aAtoms,
                                         uint32_t aCount)
  {
    for (uint32_t i = 0; i < aCount; i++) {
      if (aAtom == &aAtoms[i]) {
        return mozilla::Some(i);
      }
    }
    return mozilla::Nothing();
  }

  static bool IsMember(nsAtom* aAtom, const nsStaticAtom* aAtoms,
                       uint32_t aCount)
  {
    return Lookup(aAtom, aAtoms, aCount).isSome();
  }
};

#endif

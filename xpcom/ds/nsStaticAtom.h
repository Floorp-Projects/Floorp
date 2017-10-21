/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsStaticAtom_h__
#define nsStaticAtom_h__

#include <stdint.h>

class nsAtom;

// The following macros are used to define static atoms, typically in
// conjunction with a .h file that defines the names and values of the atoms.
//
// For example, the .h file might be called MyAtomList.h and look like this:
//
//   MY_ATOM(one, "one")
//   MY_ATOM(two, "two")
//   MY_ATOM(three, "three")
//
// The code defining the static atoms might look like this:
//
//   class MyAtoms {
//   public:
//     #define MY_ATOM(_name, _value) NS_STATIC_ATOM_DECL(_name)
//     #include "MyAtomList.h"
//     #undef MY_ATOM
//   };
//
//   #define MY_ATOM(name_, value_) NS_STATIC_ATOM_DEFN(MyAtoms, name_)
//   #include "MyAtomList.h"
//   #undef MY_ATOM
//
//   #define MY_ATOM(name_, value_) NS_STATIC_ATOM_BUFFER(name_, value_)
//   #include "MyAtomList.h"
//   #undef MY_ATOM
//
//   static const nsStaticAtomSetup sMyAtomSetup[] = {
//     #define MY_ATOM(name_, value_) NS_STATIC_ATOM_SETUP(MyAtoms, name_)
//     #include "MyAtomList.h"
//     #undef MY_ATOM
//   };
//
// The macros expand to the following:
//
//   class MyAtoms
//   {
//   public:
//     static nsAtom* one;
//     static nsAtom* two;
//     static nsAtom* three;
//   };
//
//   nsAtom* MyAtoms::one;
//   nsAtom* MyAtoms::two;
//   nsAtom* MyAtoms::three;
//
//   static const char16_t one_buffer[4] = u"one";     // plus a static_assert
//   static const char16_t two_buffer[4] = u"two";     // plus a static_assert
//   static const char16_t three_buffer[6] = u"three"; // plus a static_assert
//
//   static const nsStaticAtomSetup sMyAtomSetup[] = {
//     { one_buffer, &MyAtoms::one },
//     { two_buffer, &MyAtoms::two },
//     { three_buffer, &MyAtoms::three },
//   };
//
// When RegisterStaticAtoms(sMyAtomSetup) is called it iterates over
// sMyAtomSetup[]. E.g. for the first atom it does roughly the following:
// - MyAtoms::one = new nsAtom(one_buffer)
// - inserts MyAtoms::one into the atom table

// The declaration of the pointer to the static atom, which must be within a
// class.
#define NS_STATIC_ATOM_DECL(name_) \
  static nsAtom* name_;

// Like NS_STATIC_ATOM_DECL, but for sub-classes of nsAtom.
#define NS_STATIC_ATOM_SUBCLASS_DECL(type_, name_) \
  static type_* name_;

// The definition of the pointer to the static atom. Initially null, it is
// set by RegisterStaticAtoms() to point to a heap-allocated nsAtom.
#define NS_STATIC_ATOM_DEFN(class_, name_) \
  nsAtom* class_::name_;

// Like NS_STATIC_ATOM_DEFN, but for sub-classes of nsAtom.
#define NS_STATIC_ATOM_SUBCLASS_DEFN(type_, class_, name_) \
  type_* class_::name_;

// The buffer of 16-bit chars that constitute the static atom.
//
// Note that |value_| is an 8-bit string, and so |sizeof(value_)| is equal
// to the number of chars (including the terminating '\0'). The |u""| prefix
// converts |value_| to a 16-bit string, which is what is assigned.
#define NS_STATIC_ATOM_BUFFER(name_, value_) \
  static const char16_t name_##_buffer[sizeof(value_)] = u"" value_; \
  static_assert(sizeof(value_[0]) == 1, "non-8-bit static atom literal");

// The StaticAtomSetup. Used only during start-up.
#define NS_STATIC_ATOM_SETUP(class_, name_) \
  { name_##_buffer, &class_::name_ },

// Like NS_STATIC_ATOM_SUBCLASS, but for sub-classes of nsAtom.
#define NS_STATIC_ATOM_SUBCLASS_SETUP(class_, name_) \
  { name_##_buffer, reinterpret_cast<nsAtom**>(&class_::name_) },

// Holds data used to initialize large number of atoms during startup. Use
// NS_STATIC_ATOM_SETUP to initialize these structs. They should never be
// accessed directly other than from nsAtomTable.cpp.
struct nsStaticAtomSetup
{
  const char16_t* const mString;
  nsAtom** const mAtom;
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

#endif

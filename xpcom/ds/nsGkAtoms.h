/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsGkAtoms_h___
#define nsGkAtoms_h___

#include "nsAtom.h"
#include "nsStaticAtom.h"

namespace mozilla {
namespace detail {

struct GkAtoms
{
  #define GK_ATOM(name_, value_) NS_STATIC_ATOM_DECL_STRING(name_, value_)
  #include "nsGkAtomList.h"
  #undef GK_ATOM

  #define GK_ATOM(name_, value_) NS_STATIC_ATOM_DECL_ATOM(name_)
  #include "nsGkAtomList.h"
  #undef GK_ATOM
};

extern const GkAtoms gGkAtoms;

} // namespace detail
} // namespace mozilla

class nsGkAtoms
{
public:
  static void RegisterStaticAtoms();

  #define GK_ATOM(name_, value_) NS_STATIC_ATOM_DECL_PTR(name_)
  #include "nsGkAtomList.h"
  #undef GK_ATOM
};

#endif /* nsGkAtoms_h___ */

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsGkAtoms.h"

namespace mozilla {
namespace detail {

MOZ_PUSH_DISABLE_INTEGRAL_CONSTANT_OVERFLOW_WARNING
extern constexpr GkAtoms gGkAtoms = {
  #define GK_ATOM(name_, value_) NS_STATIC_ATOM_INIT_STRING(value_)
  #include "nsGkAtomList.h"
  #undef GK_ATOM

  #define GK_ATOM(name_, value_) \
    NS_STATIC_ATOM_INIT_ATOM(GkAtoms, name_, value_)
  #include "nsGkAtomList.h"
  #undef GK_ATOM
};
MOZ_POP_DISABLE_INTEGRAL_CONSTANT_OVERFLOW_WARNING

} // namespace detail
} // namespace mozilla

#define GK_ATOM(name_, value_) NS_STATIC_ATOM_DEFN_PTR(nsGkAtoms, name_)
#include "nsGkAtomList.h"
#undef GK_ATOM

static const nsStaticAtomSetup sGkAtomSetup[] = {
  #define GK_ATOM(name_, value_) \
    NS_STATIC_ATOM_SETUP(mozilla::detail::gGkAtoms, nsGkAtoms, name_)
  #include "nsGkAtomList.h"
  #undef GK_ATOM
};

void nsGkAtoms::RegisterStaticAtoms()
{
  NS_RegisterStaticAtoms(sGkAtomSetup);
}


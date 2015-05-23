/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/TemplateLib.h"

using mozilla::tl::And;

static_assert(And<>::value == true,
              "And<>::value should be true");
static_assert(And<true>::value == true,
              "And<true>::value should be true");
static_assert(And<false>::value == false,
              "And<false>::value should be false");
static_assert(And<false, true>::value == false,
              "And<false, true>::value should be false");
static_assert(And<false, false>::value == false,
              "And<false, false>::value should be false");
static_assert(And<true, false>::value == false,
              "And<true, false>::value should be false");
static_assert(And<true, true>::value == true,
              "And<true, true>::value should be true");
static_assert(And<true, true, true>::value == true,
              "And<true, true, true>::value should be true");
static_assert(And<true, false, true>::value == false,
              "And<true, false, true>::value should be false");

int
main()
{
  // Nothing to do here.
  return 0;
}

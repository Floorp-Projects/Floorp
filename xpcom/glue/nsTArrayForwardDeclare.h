/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsTArrayForwardDeclare_h__
#define nsTArrayForwardDeclare_h__

//
// This simple header file contains forward declarations for the TArray family
// of classes.
//
// Including this header is preferable to writing
//
//   template<class E> class nsTArray;
//
// yourself, since this header gives us flexibility to e.g. change the default
// template parameters.
//

#include <stddef.h>

template<class E>
class nsTArray;

template<class E>
class FallibleTArray;

template<class E, size_t N>
class nsAutoTArray;

template<class E, size_t N>
class AutoFallibleTArray;

#define InfallibleTArray nsTArray
#define AutoInfallibleTArray nsAutoTArray

#endif

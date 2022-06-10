/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsString.h"

// This file provides concrete instantiations for externed string template
// classes.

// ================
// Template classes
// ================
template class mozilla::detail::nsTStringRepr<char>;
template class mozilla::detail::nsTStringRepr<char16_t>;

template class nsTLiteralString<char>;
template class nsTLiteralString<char16_t>;

template class nsTSubstring<char>;
template class nsTSubstring<char16_t>;

template class nsTDependentSubstring<char>;
template class nsTDependentSubstring<char16_t>;

template class nsTString<char>;
template class nsTString<char16_t>;

template class nsTAutoStringN<char, 64>;
template class nsTAutoStringN<char16_t, 64>;

template class nsTDependentString<char>;
template class nsTDependentString<char16_t>;

template class nsTPromiseFlatString<char>;
template class nsTPromiseFlatString<char16_t>;

template class nsTSubstringSplitter<char>;
template class nsTSubstringSplitter<char16_t>;

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsString.h"

// This file provides concrete instantiations for externed string template
// classes and functions.

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

// Note: nsTString is skipped as it's implicitly instantiated by derived
// classes.

template class nsTAutoStringN<char, 64>;
template class nsTAutoStringN<char16_t, 64>;

template class nsTDependentString<char>;
template class nsTDependentString<char16_t>;

template class nsTPromiseFlatString<char>;
template class nsTPromiseFlatString<char16_t>;

template class nsTSubstringSplitter<char>;
template class nsTSubstringSplitter<char16_t>;

template class nsTDefaultStringComparator<char>;
template class nsTDefaultStringComparator<char16_t>;

// =============================
// Templated top-level functions
// =============================

template
int NS_FASTCALL
Compare<char>(mozilla::detail::nsTStringRepr<char> const&,
              mozilla::detail::nsTStringRepr<char> const&,
              nsTStringComparator<char> const&);

template
int NS_FASTCALL
Compare<char16_t>(mozilla::detail::nsTStringRepr<char16_t> const&,
                  mozilla::detail::nsTStringRepr<char16_t> const&,
                  nsTStringComparator<char16_t> const&);

template
nsTDependentSubstring<char> const
Substring<char>(char const*, char const*);

template
nsTDependentSubstring<char16_t> const
Substring<char16_t>(char16_t const*, char16_t const*);

// =========================================================
// Templated member functions that are conditionally enabled
// =========================================================

template
int32_t
nsTString<char16_t>::Find(const self_type&, int32_t, int32_t) const;

template
int32_t
nsTString<char16_t>::Find(const char_type*, int32_t, int32_t) const;

template
int32_t
nsTString<char16_t>::RFind(const self_type&, int32_t, int32_t) const;

template
int32_t
nsTString<char16_t>::RFind(const char_type*, int32_t, int32_t) const;

template
int32_t
nsTString<char16_t>::FindCharInSet(const char*, int32_t) const;

template
int32_t
nsTString<char>::Compare(const char_type*, bool, int32_t) const;

template
bool
nsTString<char16_t>::EqualsIgnoreCase(const incompatible_char_type*,
                                      int32_t) const;

template
bool
nsTString<char16_t>::StripChars(const incompatible_char_type*,
                                const fallible_t&);

template
void
nsTString<char16_t>::StripChars(const incompatible_char_type*);

template
void
nsTString<char16_t>::ReplaceChar(const char*, char16_t);

/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

void
nsTDependentString_CharT::Rebind( const string_type& str, uint32_t startPos )
  {
    // If we currently own a buffer, release it.
    Finalize();

    size_type strLength = str.Length();

    if (startPos > strLength)
      startPos = strLength;

    mData = const_cast<char_type*>(str.Data()) + startPos;
    mLength = strLength - startPos;

    SetDataFlags(F_TERMINATED);
  }

/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

void
nsTDependentSubstring_CharT::Rebind( const substring_type& str, PRUint32 startPos, PRUint32 length )
  {
    // If we currently own a buffer, release it.
    Finalize();

    size_type strLength = str.Length();

    if (startPos > strLength)
      startPos = strLength;

    mData = const_cast<char_type*>(str.Data()) + startPos;
    mLength = NS_MIN(length, strLength - startPos);

    SetDataFlags(F_NONE);
  }

void
nsTDependentSubstring_CharT::Rebind( const char_type* data, size_type length )
  {
    NS_ASSERTION(data, "nsTDependentSubstring must wrap a non-NULL buffer");

    // If we currently own a buffer, release it.
    Finalize();

    mData = const_cast<char_type*>(data);
    mLength = length;
    SetDataFlags(F_NONE);
  }

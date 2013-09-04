/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

nsTAdoptingString_CharT&
nsTAdoptingString_CharT::operator=( const self_type& str )
  {
    // This'll violate the constness of this argument, that's just
    // the nature of this class...
    self_type* mutable_str = const_cast<self_type*>(&str);

    if (str.mFlags & F_OWNED)
      {
        // We want to do what Adopt() does, but without actually incrementing
        // the Adopt count.  Note that we can be a little more straightforward
        // about this than Adopt() is, because we know that str.mData is
        // non-null.  Should we be able to assert that str is not void here?
        NS_ASSERTION(str.mData, "String with null mData?");
        Finalize();
        mData = str.mData;
        mLength = str.mLength;
        SetDataFlags(F_TERMINATED | F_OWNED);

        // Make str forget the buffer we just took ownership of.
        new (mutable_str) self_type();
      }
    else
      {
        Assign(str);

        mutable_str->Truncate();
      }

    return *this;
  }

void
nsTString_CharT::Rebind( const char_type* data, size_type length )
  {
    // If we currently own a buffer, release it.
    Finalize();

    mData = const_cast<char_type*>(data);
    mLength = length;
    SetDataFlags(F_TERMINATED);
    AssertValidDepedentString();
  }


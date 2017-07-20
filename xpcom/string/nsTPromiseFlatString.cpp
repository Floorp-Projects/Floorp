/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

void
nsTPromiseFlatString_CharT::Init(const substring_type& str)
{
  if (str.IsTerminated()) {
    char_type* newData =
      const_cast<char_type*>(static_cast<const char_type*>(str.Data()));
    size_type newLength = str.Length();
    DataFlags newDataFlags =
      str.GetDataFlags() & (DataFlags::TERMINATED | DataFlags::LITERAL);
    // does not promote DataFlags::VOIDED

    SetData(newData, newLength, newDataFlags);
  } else {
    Assign(str);
  }
}

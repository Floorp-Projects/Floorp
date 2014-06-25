/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

void
nsTDependentString_CharT::Rebind(const string_type& str, uint32_t startPos)
{
  NS_ABORT_IF_FALSE(str.Flags() & F_TERMINATED, "Unterminated flat string");

  // If we currently own a buffer, release it.
  Finalize();

  size_type strLength = str.Length();

  if (startPos > strLength) {
    startPos = strLength;
  }

  mData = const_cast<char_type*>(static_cast<const char_type*>(str.Data())) + startPos;
  mLength = strLength - startPos;

  SetDataFlags(str.Flags() & (F_TERMINATED | F_LITERAL));
}

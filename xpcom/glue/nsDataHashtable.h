/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDataHashtable_h__
#define nsDataHashtable_h__

#include "nsHashKeys.h"
#include "nsBaseHashtable.h"

/**
 * templated hashtable class maps keys to simple datatypes.
 * See nsBaseHashtable for complete declaration
 * @param KeyClass a wrapper-class for the hashtable key, see nsHashKeys.h
 *   for a complete specification.
 * @param DataType the simple datatype being wrapped
 * @see nsInterfaceHashtable, nsClassHashtable
 */
template<class KeyClass, class DataType>
class nsDataHashtable
  : public nsBaseHashtable<KeyClass, DataType, DataType>
{
public:
  nsDataHashtable() {}
  explicit nsDataHashtable(uint32_t aInitSize)
    : nsBaseHashtable<KeyClass, DataType, DataType>(aInitSize)
  {
  }
};

#endif // nsDataHashtable_h__

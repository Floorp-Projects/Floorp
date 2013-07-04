/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsJSThingHashtable_h__
#define nsJSThingHashtable_h__

#include "nsHashKeys.h"
#include "nsBaseHashtable.h"

namespace JS {
template <class T>
class Heap;
} /* namespace JS */

/**
 * A wrapper for hash keys that sets ALLOW_MEMMOVE to false.
 *
 * This is used in the implementation of nsJSThingHashtable and is not intended
 * to be used directly.
 *
 * It is necessary for hash tables containing JS::Heap<T> values as these must
 * be copied rather than memmoved.
 */
template<class T>
class nsHashKeyDisallowMemmove : public T
{
 public:
  nsHashKeyDisallowMemmove(const T& key) : T(key) {}
  enum { ALLOW_MEMMOVE = false };
};


/**
 * Templated hashtable class for use on the heap where the values are JS GC things.
 *
 * Storing JS GC thing pointers on the heap requires wrapping them in a
 * JS::Heap<T>, and this class takes care of that while presenting an interface
 * in terms of the wrapped type T.
 *
 * For example, to store a hashtable mapping strings to JSObject pointers, you
 * can declare a data member like this:
 *
 *   nsJSThingHashtable<nsStringHashKey, JSObject*> mStringToObjectMap;
 *
 * See nsBaseHashtable for complete declaration
 * @param KeyClass a wrapper-class for the hashtable key, see nsHashKeys.h
 *   for a complete specification.
 * @param DataType the datatype being wrapped, must be a JS GC thing.
 * @see nsInterfaceHashtable, nsClassHashtable
 */
template<class KeyClass,class DataType>
class nsJSThingHashtable :
  public nsBaseHashtable<nsHashKeyDisallowMemmove<KeyClass>, JS::Heap<DataType>, DataType>
{ };

#endif // nsJSThingHashtable_h__

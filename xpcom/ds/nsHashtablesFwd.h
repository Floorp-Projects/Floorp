/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef XPCOM_DS_NSHASHTABLESFWD_H_
#define XPCOM_DS_NSHASHTABLESFWD_H_

#include "mozilla/Attributes.h"

struct PLDHashEntryHdr;

template <class T>
class MOZ_IS_REFPTR nsCOMPtr;

template <class T>
class MOZ_IS_REFPTR RefPtr;

template <class EntryType>
class MOZ_NEEDS_NO_VTABLE_TYPE nsTHashtable;

template <class DataType, class UserDataType>
class nsDefaultConverter;

template <class KeyClass, class DataType, class UserDataType,
          class Converter = nsDefaultConverter<DataType, UserDataType>>
class nsBaseHashtable;

template <class KeyClass, class T>
class nsClassHashtable;

/**
 * templated hashtable class maps keys to simple datatypes.
 * See nsBaseHashtable for complete declaration
 * @deprecated This is going to be removed. Use nsTHashMap instead.
 * @param KeyClass a wrapper-class for the hashtable key, see nsHashKeys.h
 *   for a complete specification.
 * @param DataType the simple datatype being wrapped
 * @see nsInterfaceHashtable, nsClassHashtable
 */
template <class KeyClass, class DataType>
using nsDataHashtable = nsBaseHashtable<KeyClass, DataType, DataType>;

template <class KeyClass, class PtrType>
class nsRefCountedHashtable;

/**
 * templated hashtable class maps keys to interface pointers.
 * See nsBaseHashtable for complete declaration.
 * @deprecated This is going to be removed. Use nsTHashMap instead.
 * @param KeyClass a wrapper-class for the hashtable key, see nsHashKeys.h
 *   for a complete specification.
 * @param Interface the interface-type being wrapped
 * @see nsDataHashtable, nsClassHashtable
 */
template <class KeyClass, class Interface>
using nsInterfaceHashtable =
    nsRefCountedHashtable<KeyClass, nsCOMPtr<Interface>>;

/**
 * templated hashtable class maps keys to reference pointers.
 * See nsBaseHashtable for complete declaration.
 * @deprecated This is going to be removed. Use nsTHashMap instead.
 * @param KeyClass a wrapper-class for the hashtable key, see nsHashKeys.h
 *   for a complete specification.
 * @param PtrType the reference-type being wrapped
 * @see nsDataHashtable, nsClassHashtable
 */
template <class KeyClass, class ClassType>
using nsRefPtrHashtable = nsRefCountedHashtable<KeyClass, RefPtr<ClassType>>;

namespace mozilla::detail {
template <class KeyType>
struct nsKeyClass;
}  // namespace mozilla::detail

/**
 * A universal hash map that maps some KeyType to some DataType. It can be used
 * for any DataType, including RefPtr<T>, nsCOMPtr<T> and UniquePtr<T>.
 *
 * For the default hash keys types, the appropriate hash key class is determined
 * automatically, so you can just specify `nsTHashMap<uint32_t,
 * RefPtr<Foo>>`, for example.
 *
 * If you require custom hash behaviour (e.g. case insensitive string handling),
 * you can still specify a hash key class derived from PLDHashEntryHdr
 * explicitly.
 *
 * If you need to use a custom UserDataType, use nsBaseHashtable (or
 * nsTHashtable) directly. However, you should double-check if that's really
 * necessary.
 */
template <class KeyType, class DataType>
using nsTHashMap =
    nsBaseHashtable<typename mozilla::detail::nsKeyClass<KeyType>::type,
                    DataType, DataType>;

#endif  // XPCOM_DS_NSHASHTABLESFWD_H_

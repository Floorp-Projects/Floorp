/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef XPCOM_DS_NSHASHTABLESFWD_H_
#define XPCOM_DS_NSHASHTABLESFWD_H_

#include "mozilla/Attributes.h"

struct PLDHashEntryHdr;

template <class EntryType>
class MOZ_NEEDS_NO_VTABLE_TYPE nsTHashtable;

template <class DataType, class UserDataType>
class nsDefaultConverter;

template <class KeyClass, class DataType, class UserDataType,
          class Converter = nsDefaultConverter<DataType, UserDataType>>
class nsBaseHashtable;

template <class KeyClass, class T>
class nsClassHashtable;

template <class KeyClass, class DataType>
class nsDataHashtable;

template <class KeyClass, class Interface>
class nsInterfaceHashtable;

template <class KeyClass, class PtrType>
class nsRefPtrHashtable;

#endif  // XPCOM_DS_NSHASHTABLESFWD_H_

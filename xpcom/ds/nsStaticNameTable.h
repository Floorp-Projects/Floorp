/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Classes to manage lookup of static names in a table. */

#ifndef nsStaticNameTable_h___
#define nsStaticNameTable_h___

#include "PLDHashTable.h"
#include "nsString.h"

/* This class supports case insensitive lookup.
 *
 * It differs from atom tables:
 * - It supports case insensitive lookup.
 * - It has minimal footprint by not copying the string table.
 * - It does no locking.
 * - It returns zero based indexes and const nsCString& as required by its
 *   callers in the parser.
 * - It is not an xpcom interface - meant for fast lookup in static tables.
 *
 * ***REQUIREMENTS***
 * - It *requires* that all entries in the table be lowercase only.
 * - It *requires* that the table of strings be in memory that lives at least
 *    as long as this table object - typically a static string array.
 */

class nsStaticCaseInsensitiveNameTable
{
public:
  enum { NOT_FOUND = -1 };

  int32_t          Lookup(const nsACString& aName) const;
  int32_t          Lookup(const nsAString& aName) const;
  const nsCString& GetStringValue(int32_t aIndex);

  nsStaticCaseInsensitiveNameTable(const char* const aNames[], int32_t aLength);
  ~nsStaticCaseInsensitiveNameTable();

private:
  nsDependentCString*   mNameArray;
  PLDHashTable          mNameTable;
  nsDependentCString    mNullStr;
};

#endif /* nsStaticNameTable_h___ */

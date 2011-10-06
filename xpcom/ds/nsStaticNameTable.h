/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/* Classes to manage lookup of static names in a table. */

#ifndef nsStaticNameTable_h___
#define nsStaticNameTable_h___

#include "pldhash.h"
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

  bool             Init(const char* const aNames[], PRInt32 Count);
  PRInt32          Lookup(const nsACString& aName);
  PRInt32          Lookup(const nsAString& aName);
  const nsAFlatCString& GetStringValue(PRInt32 index);

  nsStaticCaseInsensitiveNameTable();
  ~nsStaticCaseInsensitiveNameTable();

private:
  nsDependentCString*   mNameArray;
  PLDHashTable mNameTable;
  nsDependentCString    mNullStr;
};

#endif /* nsStaticNameTable_h___ */

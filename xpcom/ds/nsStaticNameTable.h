/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express oqr
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU Public License (the "GPL"), in which case the
 * provisions of the GPL are applicable instead of those above.
 * If you wish to allow use of your version of this file only
 * under the terms of the GPL and not to allow others to use your
 * version of this file under the NPL, indicate your decision by
 * deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL.  If you do not delete
 * the provisions above, a recipient may use your version of this
 * file under either the NPL or the GPL.
 */

/* Classes to manage lookup of static names in a table. */

#ifndef nsStaticNameTable_h___
#define nsStaticNameTable_h___

#include "nsHashtable.h"

/* This class supports case insensitve lookup. 
 *
 * It differs from atom tables: 
 * - It supports case insensitive lookup.
 * - It has minimal footprint by not copying the string table.
 * - It does no locking.
 * - It returns zero based indexes and const nsCSring& as required by its 
 *   callers in the parser.
 * - It is not an xpcom interface - meant for fast lookup in static tables.
 *
 * ***REQUIREMENTS***
 * - It *requires* that all entries in the table be lowewcase only.  
 * - It *requires* that the table of strings be in memory that lives at least
 *    as long as this table object - typically a static string array.
 */

class NS_COM nsStaticCaseInsensitiveNameTable
{
public:
  enum { NOT_FOUND = -1 };

  PRBool           Init(const char* Names[], PRInt32 Count);
  PRInt32          Lookup(const nsACString& aName);
  PRInt32          Lookup(const nsAString& aName);
  const nsCString& GetStringValue(PRInt32 index);
  PRBool           IsNullString(const nsCString& s) {return s == mNullStr;}

  nsStaticCaseInsensitiveNameTable();
  ~nsStaticCaseInsensitiveNameTable();

private:
  nsCString*   mNameArray;
  nsHashtable* mNameTable;
  PRInt32      mCount;
  nsCString    mNullStr;
};

#endif /* nsStaticNameTable_h___ */

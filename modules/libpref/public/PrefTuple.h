/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef preftuple_included
#define preftuple_included

#include "nsTArray.h"
#include "nsString.h"

struct PrefTuple
{
  nsCAutoString key;

  // We don't use a union to avoid allocations when using the string component
  // NOTE: Only one field will be valid at any given time, as indicated by the type enum
  nsCAutoString stringVal;
  PRInt32       intVal;
  bool          boolVal;

  enum {
    PREF_STRING,
    PREF_INT,
    PREF_BOOL
  } type;

};

#endif


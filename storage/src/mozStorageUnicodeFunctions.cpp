/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 sts=2
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
 * The Original Code is unicode functions code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation. 
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * This code is based off of icu.c from the sqlite code
 * whose original author is danielk1977
 *
 * Contributor(s):
 *   Shawn Wilsher <me@shawnwilsher.com> (Original Author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "mozStorageUnicodeFunctions.h"
#include "nsUnicharUtils.h"

int
StorageUnicodeFunctions::RegisterFunctions(sqlite3 *aDB)
{
  struct Functions {
    const char *zName;
    int nArg;
    int enc;
    void *pContext;
    void (*xFunc)(sqlite3_context*, int, sqlite3_value**);
  } functions[] = {
    {"lower", 1, SQLITE_UTF16, 0,        caseFunction},
    {"lower", 1, SQLITE_UTF8,  0,        caseFunction},
    {"upper", 1, SQLITE_UTF16, (void*)1, caseFunction},
    {"upper", 1, SQLITE_UTF8,  (void*)1, caseFunction},

    {"like",  2, SQLITE_UTF16, 0,        likeFunction},
    {"like",  2, SQLITE_UTF8,  0,        likeFunction},
    {"like",  3, SQLITE_UTF16, 0,        likeFunction},
    {"like",  3, SQLITE_UTF8,  0,        likeFunction},
  };

  int rv = SQLITE_OK;
  for (unsigned i = 0; SQLITE_OK == rv && i < NS_ARRAY_LENGTH(functions); ++i) {
    struct Functions *p = &functions[i];
    rv = sqlite3_create_function(aDB, p->zName, p->nArg, p->enc, p->pContext,
                                 p->xFunc, NULL, NULL);
  }

  return rv;
}

void
StorageUnicodeFunctions::caseFunction(sqlite3_context *p,
                                      int aArgc,
                                      sqlite3_value **aArgv)
{
  NS_ASSERTION(1 == aArgc, "Invalid number of arguments!");

  nsAutoString data(static_cast<const PRUnichar *>(sqlite3_value_text16(aArgv[0])));
  PRBool toUpper = sqlite3_user_data(p) ? PR_TRUE : PR_FALSE;

  if (toUpper)
    ToUpperCase(data);
  else 
    ToLowerCase(data);

  // Give sqlite our result
  sqlite3_result_text16(p, data.get(), -1, SQLITE_TRANSIENT);
}

static int
likeCompare(nsAString::const_iterator aPatternItr,
            nsAString::const_iterator aPatternEnd,
            nsAString::const_iterator aStringItr,
            nsAString::const_iterator aStringEnd,
            PRUnichar aEscape)
{
  const PRUnichar MATCH_ALL('%');
  const PRUnichar MATCH_ONE('_');

  PRBool lastWasEscape = PR_FALSE;
  while (aPatternItr != aPatternEnd) {
    /**
     * What we do in here is take a look at each character from the input
     * pattern, and do something with it.  There are 4 possibilities:
     * 1) character is an un-escaped match-all character
     * 2) character is an un-escaped match-one character
     * 3) character is an un-escaped escape character
     * 4) character is not any of the above
     */
    if (!lastWasEscape && *aPatternItr == MATCH_ALL) {
      // CASE 1
      /**
       * Now we need to skip any MATCH_ALL or MATCH_ONE characters that follow a
       * MATCH_ALL character.  For each MATCH_ONE character, skip one character
       * in the pattern string.
       */
      while (*aPatternItr == MATCH_ALL || *aPatternItr == MATCH_ONE) {
        if (*aPatternItr == MATCH_ONE) {
          // If we've hit the end of the string we are testing, no match
          if (aStringItr == aStringEnd)
            return 0;
          aStringItr++;
        }
        aPatternItr++;
      }

      // If we've hit the end of the pattern string, match
      if (aPatternItr == aPatternEnd)
        return 1;

      while (aStringItr != aStringEnd) {
        if (likeCompare(aPatternItr, aPatternEnd, aStringItr, aStringEnd, aEscape)) {
          // we've hit a match, so indicate this
          return 1;
        }
        aStringItr++;
      }

      // No match
      return 0;
    } else if (!lastWasEscape && *aPatternItr == MATCH_ONE) {
      // CASE 2
      if (aStringItr == aStringEnd) {
        // If we've hit the end of the string we are testing, no match
        return 0;
      }
      aStringItr++;
      lastWasEscape = PR_FALSE;
    } else if (!lastWasEscape && *aPatternItr == aEscape) {
      // CASE 3
      lastWasEscape = PR_TRUE;
    } else {
      // CASE 4
      if (ToUpperCase(*aStringItr) != ToUpperCase(*aPatternItr)) {
        // If we've hit a point where the strings don't match, there is no match
        return 0;
      }
      aStringItr++;
      lastWasEscape = PR_FALSE;
    }
    
    aPatternItr++;
  }

  return aStringItr == aStringEnd;
}

/**
 * This implements the like() SQL function.  This is used by the LIKE operator.
 * The SQL statement 'A LIKE B' is implemented as 'like(B, A)', and if there is
 * an escape character, say E, it is implemented as 'like(B, A, E)'.
 */
void
StorageUnicodeFunctions::likeFunction(sqlite3_context *p,
                                      int aArgc,
                                      sqlite3_value **aArgv)
{
  NS_ASSERTION(2 == aArgc || 3 == aArgc, "Invalid number of arguments!");

  if (sqlite3_value_bytes(aArgv[0]) > SQLITE_MAX_LIKE_PATTERN_LENGTH) {
    sqlite3_result_error(p, "LIKE or GLOB pattern too complex", SQLITE_TOOBIG);
    return;
  }

  if (!sqlite3_value_text16(aArgv[0]) || !sqlite3_value_text16(aArgv[1]))
    return;

  nsDependentString A(static_cast<const PRUnichar *>(sqlite3_value_text16(aArgv[1])));
  nsDependentString B(static_cast<const PRUnichar *>(sqlite3_value_text16(aArgv[0])));
  NS_ASSERTION(!B.IsEmpty(), "LIKE string must not be null!");

  PRUnichar E;
  if (3 == aArgc)
    E = static_cast<const PRUnichar *>(sqlite3_value_text16(aArgv[2]))[0];

  nsAString::const_iterator itrString, endString;
  A.BeginReading(itrString);
  A.EndReading(endString);
  nsAString::const_iterator itrPattern, endPattern;
  B.BeginReading(itrPattern);
  B.EndReading(endPattern);
  sqlite3_result_int(p, likeCompare(itrPattern, endPattern,
                                    itrString, endString, E));
}


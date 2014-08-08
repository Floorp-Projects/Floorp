/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ArrayUtils.h"

#include "mozStorageSQLFunctions.h"
#include "nsUnicharUtils.h"
#include <algorithm>

namespace mozilla {
namespace storage {

////////////////////////////////////////////////////////////////////////////////
//// Local Helper Functions

namespace {

/**
 * Performs the LIKE comparison of a string against a pattern.  For more detail
 * see http://www.sqlite.org/lang_expr.html#like.
 *
 * @param aPatternItr
 *        An iterator at the start of the pattern to check for.
 * @param aPatternEnd
 *        An iterator at the end of the pattern to check for.
 * @param aStringItr
 *        An iterator at the start of the string to check for the pattern.
 * @param aStringEnd
 *        An iterator at the end of the string to check for the pattern.
 * @param aEscapeChar
 *        The character to use for escaping symbols in the pattern.
 * @return 1 if the pattern is found, 0 otherwise.
 */
int
likeCompare(nsAString::const_iterator aPatternItr,
            nsAString::const_iterator aPatternEnd,
            nsAString::const_iterator aStringItr,
            nsAString::const_iterator aStringEnd,
            char16_t aEscapeChar)
{
  const char16_t MATCH_ALL('%');
  const char16_t MATCH_ONE('_');

  bool lastWasEscape = false;
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
        if (likeCompare(aPatternItr, aPatternEnd, aStringItr, aStringEnd,
                        aEscapeChar)) {
          // we've hit a match, so indicate this
          return 1;
        }
        aStringItr++;
      }

      // No match
      return 0;
    }
    else if (!lastWasEscape && *aPatternItr == MATCH_ONE) {
      // CASE 2
      if (aStringItr == aStringEnd) {
        // If we've hit the end of the string we are testing, no match
        return 0;
      }
      aStringItr++;
      lastWasEscape = false;
    }
    else if (!lastWasEscape && *aPatternItr == aEscapeChar) {
      // CASE 3
      lastWasEscape = true;
    }
    else {
      // CASE 4
      if (::ToUpperCase(*aStringItr) != ::ToUpperCase(*aPatternItr)) {
        // If we've hit a point where the strings don't match, there is no match
        return 0;
      }
      aStringItr++;
      lastWasEscape = false;
    }

    aPatternItr++;
  }

  return aStringItr == aStringEnd;
}

/**
 * This class manages a dynamic array.  It can represent an array of any 
 * reasonable size, but if the array is "N" elements or smaller, it will be
 * stored using fixed space inside the auto array itself.  If the auto array
 * is a local variable, this internal storage will be allocated cheaply on the
 * stack, similar to nsAutoString.  If a larger size is requested, the memory
 * will be dynamically allocated from the heap.  Since the destructor will
 * free any heap-allocated memory, client code doesn't need to care where the
 * memory came from.
 */
template <class T, size_t N> class AutoArray
{

public:

  explicit AutoArray(size_t size)
  : mBuffer(size <= N ? mAutoBuffer : new T[size])
  {
  }

  ~AutoArray()
  { 
    if (mBuffer != mAutoBuffer)
      delete[] mBuffer; 
  }

  /**
   * Return the pointer to the allocated array.
   * @note If the array allocation failed, get() will return nullptr!
   *
   * @return the pointer to the allocated array
   */
  T *get() 
  {
    return mBuffer; 
  }

private:
  T *mBuffer;           // Points to mAutoBuffer if we can use it, heap otherwise.
  T mAutoBuffer[N];     // The internal memory buffer that we use if we can.
};

/**
 * Compute the Levenshtein Edit Distance between two strings.
 * 
 * @param aStringS
 *        a string
 * @param aStringT
 *        another string
 * @param _result
 *        an outparam that will receive the edit distance between the arguments
 * @return a Sqlite result code, e.g. SQLITE_OK, SQLITE_NOMEM, etc.
 */
int
levenshteinDistance(const nsAString &aStringS,
                    const nsAString &aStringT,
                    int *_result)
{
    // Set the result to a non-sensical value in case we encounter an error.
    *_result = -1;

    const uint32_t sLen = aStringS.Length();
    const uint32_t tLen = aStringT.Length();

    if (sLen == 0) {
      *_result = tLen;
      return SQLITE_OK;
    }
    if (tLen == 0) {
      *_result = sLen;
      return SQLITE_OK;
    }

    // Notionally, Levenshtein Distance is computed in a matrix.  If we 
    // assume s = "span" and t = "spam", the matrix would look like this:
    //    s -->
    //  t          s   p   a   n
    //  |      0   1   2   3   4
    //  V  s   1   *   *   *   *
    //     p   2   *   *   *   *
    //     a   3   *   *   *   *
    //     m   4   *   *   *   *
    //
    // Note that the row width is sLen + 1 and the column height is tLen + 1,
    // where sLen is the length of the string "s" and tLen is the length of "t".
    // The first row and the first column are initialized as shown, and
    // the algorithm computes the remaining cells row-by-row, and
    // left-to-right within each row.  The computation only requires that
    // we be able to see the current row and the previous one.

    // Allocate memory for two rows.  Use AutoArray's to manage the memory
    // so we don't have to explicitly free it, and so we can avoid the expense
    // of memory allocations for relatively small strings.
    AutoArray<int, nsAutoString::kDefaultStorageSize> row1(sLen + 1);
    AutoArray<int, nsAutoString::kDefaultStorageSize> row2(sLen + 1);

    // Declare the raw pointers that will actually be used to access the memory.
    int *prevRow = row1.get();
    NS_ENSURE_TRUE(prevRow, SQLITE_NOMEM);
    int *currRow = row2.get();
    NS_ENSURE_TRUE(currRow, SQLITE_NOMEM);

    // Initialize the first row.
    for (uint32_t i = 0; i <= sLen; i++)
        prevRow[i] = i;

    const char16_t *s = aStringS.BeginReading();
    const char16_t *t = aStringT.BeginReading();

    // Compute the empty cells in the "matrix" row-by-row, starting with
    // the second row.
    for (uint32_t ti = 1; ti <= tLen; ti++) {

        // Initialize the first cell in this row.
        currRow[0] = ti;

        // Get the character from "t" that corresponds to this row.
        const char16_t tch = t[ti - 1];

        // Compute the remaining cells in this row, left-to-right,
        // starting at the second column (and first character of "s").
        for (uint32_t si = 1; si <= sLen; si++) {
            
            // Get the character from "s" that corresponds to this column,
            // compare it to the t-character, and compute the "cost".
            const char16_t sch = s[si - 1];
            int cost = (sch == tch) ? 0 : 1;

            // ............ We want to calculate the value of cell "d" from
            // ...ab....... the previously calculated (or initialized) cells
            // ...cd....... "a", "b", and "c", where d = min(a', b', c').
            // ............ 
            int aPrime = prevRow[si - 1] + cost;
            int bPrime = prevRow[si] + 1;
            int cPrime = currRow[si - 1] + 1;
            currRow[si] = std::min(aPrime, std::min(bPrime, cPrime));
        }

        // Advance to the next row.  The current row becomes the previous
        // row and we recycle the old previous row as the new current row.
        // We don't need to re-initialize the new current row since we will
        // rewrite all of its cells anyway.
        int *oldPrevRow = prevRow;
        prevRow = currRow;
        currRow = oldPrevRow;
    }

    // The final result is the value of the last cell in the last row.
    // Note that that's now in the "previous" row, since we just swapped them.
    *_result = prevRow[sLen];
    return SQLITE_OK;
}

// This struct is used only by registerFunctions below, but ISO C++98 forbids
// instantiating a template dependent on a locally-defined type.  Boo-urns!
struct Functions {
  const char *zName;
  int nArg;
  int enc;
  void *pContext;
  void (*xFunc)(::sqlite3_context*, int, sqlite3_value**);
};

} // anonymous namespace

////////////////////////////////////////////////////////////////////////////////
//// Exposed Functions

int
registerFunctions(sqlite3 *aDB)
{
  Functions functions[] = {
    {"lower",               
      1, 
      SQLITE_UTF16, 
      0,        
      caseFunction},
    {"lower",               
      1, 
      SQLITE_UTF8,  
      0,        
      caseFunction},
    {"upper",               
      1, 
      SQLITE_UTF16, 
      (void*)1, 
      caseFunction},
    {"upper",               
      1, 
      SQLITE_UTF8,  
      (void*)1, 
      caseFunction},

    {"like",                
      2, 
      SQLITE_UTF16, 
      0,        
      likeFunction},
    {"like",                
      2, 
      SQLITE_UTF8,  
      0,        
      likeFunction},
    {"like",                
      3, 
      SQLITE_UTF16, 
      0,        
      likeFunction},
    {"like",                
      3, 
      SQLITE_UTF8,  
      0,        
      likeFunction},

    {"levenshteinDistance", 
      2, 
      SQLITE_UTF16, 
      0,        
      levenshteinDistanceFunction},
    {"levenshteinDistance", 
      2, 
      SQLITE_UTF8,  
      0,        
      levenshteinDistanceFunction},
  };

  int rv = SQLITE_OK;
  for (size_t i = 0; SQLITE_OK == rv && i < ArrayLength(functions); ++i) {
    struct Functions *p = &functions[i];
    rv = ::sqlite3_create_function(aDB, p->zName, p->nArg, p->enc, p->pContext,
                                   p->xFunc, nullptr, nullptr);
  }

  return rv;
}

////////////////////////////////////////////////////////////////////////////////
//// SQL Functions

void
caseFunction(sqlite3_context *aCtx,
             int aArgc,
             sqlite3_value **aArgv)
{
  NS_ASSERTION(1 == aArgc, "Invalid number of arguments!");

  nsAutoString data(static_cast<const char16_t *>(::sqlite3_value_text16(aArgv[0])));
  bool toUpper = ::sqlite3_user_data(aCtx) ? true : false;

  if (toUpper)
    ::ToUpperCase(data);
  else
    ::ToLowerCase(data);

  // Set the result.
  ::sqlite3_result_text16(aCtx, data.get(), -1, SQLITE_TRANSIENT);
}

/**
 * This implements the like() SQL function.  This is used by the LIKE operator.
 * The SQL statement 'A LIKE B' is implemented as 'like(B, A)', and if there is
 * an escape character, say E, it is implemented as 'like(B, A, E)'.
 */
void
likeFunction(sqlite3_context *aCtx,
             int aArgc,
             sqlite3_value **aArgv)
{
  NS_ASSERTION(2 == aArgc || 3 == aArgc, "Invalid number of arguments!");

  if (::sqlite3_value_bytes(aArgv[0]) > SQLITE_MAX_LIKE_PATTERN_LENGTH) {
    ::sqlite3_result_error(aCtx, "LIKE or GLOB pattern too complex",
                           SQLITE_TOOBIG);
    return;
  }

  if (!::sqlite3_value_text16(aArgv[0]) || !::sqlite3_value_text16(aArgv[1]))
    return;

  nsDependentString A(static_cast<const char16_t *>(::sqlite3_value_text16(aArgv[1])));
  nsDependentString B(static_cast<const char16_t *>(::sqlite3_value_text16(aArgv[0])));
  NS_ASSERTION(!B.IsEmpty(), "LIKE string must not be null!");

  char16_t E = 0;
  if (3 == aArgc)
    E = static_cast<const char16_t *>(::sqlite3_value_text16(aArgv[2]))[0];

  nsAString::const_iterator itrString, endString;
  A.BeginReading(itrString);
  A.EndReading(endString);
  nsAString::const_iterator itrPattern, endPattern;
  B.BeginReading(itrPattern);
  B.EndReading(endPattern);
  ::sqlite3_result_int(aCtx, likeCompare(itrPattern, endPattern, itrString,
                                         endString, E));
}

void levenshteinDistanceFunction(sqlite3_context *aCtx,
                                 int aArgc,
                                 sqlite3_value **aArgv)
{
  NS_ASSERTION(2 == aArgc, "Invalid number of arguments!");

  // If either argument is a SQL NULL, then return SQL NULL.
  if (::sqlite3_value_type(aArgv[0]) == SQLITE_NULL ||
      ::sqlite3_value_type(aArgv[1]) == SQLITE_NULL) {
    ::sqlite3_result_null(aCtx);
    return;
  }

  int aLen = ::sqlite3_value_bytes16(aArgv[0]) / sizeof(char16_t);
  const char16_t *a = static_cast<const char16_t *>(::sqlite3_value_text16(aArgv[0]));

  int bLen = ::sqlite3_value_bytes16(aArgv[1]) / sizeof(char16_t);
  const char16_t *b = static_cast<const char16_t *>(::sqlite3_value_text16(aArgv[1]));

  // Compute the Levenshtein Distance, and return the result (or error).
  int distance = -1;
  const nsDependentString A(a, aLen);
  const nsDependentString B(b, bLen);
  int status = levenshteinDistance(A, B, &distance);
  if (status == SQLITE_OK) {
    ::sqlite3_result_int(aCtx, distance);    
  }
  else if (status == SQLITE_NOMEM) {
    ::sqlite3_result_error_nomem(aCtx);
  }
  else {
    ::sqlite3_result_error(aCtx, "User function returned error code", -1);
  }
}

} // namespace storage
} // namespace mozilla

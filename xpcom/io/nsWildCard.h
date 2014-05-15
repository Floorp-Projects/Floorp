/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * nsWildCard.h: Defines and prototypes for shell exp. match routines
 *
 * See nsIZipReader.findEntries docs in nsIZipReader.idl for a description of
 * the supported expression syntax.
 *
 * Note that the syntax documentation explicitly says the results of certain
 * expressions are undefined.  This is intentional to require less robustness
 * in the code.  Regular expression parsing is hard; the smaller the set of
 * features and interactions this code must support, the easier it is to
 * ensure it works.
 *
 */

#ifndef nsWildCard_h__
#define nsWildCard_h__

#include "nscore.h"

/* --------------------------- Public routines ---------------------------- */


/*
 * NS_WildCardValid takes a shell expression exp as input. It returns:
 *
 *  NON_SXP      if exp is a standard string
 *  INVALID_SXP  if exp is a shell expression, but invalid
 *  VALID_SXP    if exp is a valid shell expression
 */

#define NON_SXP -1
#define INVALID_SXP -2
#define VALID_SXP 1

int NS_WildCardValid(const char* aExpr);

int NS_WildCardValid(const char16_t* aExpr);

/* return values for the search routines */
#define MATCH 0
#define NOMATCH 1
#define ABORTED -1

/*
 * NS_WildCardMatch
 *
 * Takes a prevalidated shell expression exp, and a string str.
 *
 * Returns 0 on match and 1 on non-match.
 */

int NS_WildCardMatch(const char* aStr, const char* aExpr,
                     bool aCaseInsensitive);

int NS_WildCardMatch(const char16_t* aStr, const char16_t* aExpr,
                     bool aCaseInsensitive);

#endif /* nsWildCard_h__ */

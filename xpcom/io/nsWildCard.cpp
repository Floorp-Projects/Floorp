/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* *
 *
 *
 * nsWildCard.cpp: shell-like wildcard match routines
 *
 * See nsIZipReader.findEntries documentation in nsIZipReader.idl for
 * a description of the syntax supported by the routines in this file.
 *
 * Rob McCool
 *
 */

#include "nsWildCard.h"
#include "nsXPCOM.h"
#include "nsCRTGlue.h"
#include "nsCharTraits.h"

/* -------------------- ASCII-specific character methods ------------------- */

typedef int static_assert_character_code_arrangement['a' > 'A' ? 1 : -1];

template<class T>
static int
alpha(T aChar)
{
  return ('a' <= aChar && aChar <= 'z') ||
         ('A' <= aChar && aChar <= 'Z');
}

template<class T>
static int
alphanumeric(T aChar)
{
  return ('0' <= aChar && aChar <= '9') || ::alpha(aChar);
}

template<class T>
static int
lower(T aChar)
{
  return ('A' <= aChar && aChar <= 'Z') ? aChar + ('a' - 'A') : aChar;
}

template<class T>
static int
upper(T aChar)
{
  return ('a' <= aChar && aChar <= 'z') ? aChar - ('a' - 'A') : aChar;
}

/* ----------------------------- _valid_subexp ---------------------------- */

template<class T>
static int
_valid_subexp(const T* aExpr, T aStop1, T aStop2)
{
  int x;
  int nsc = 0;     /* Number of special characters */
  int np;          /* Number of pipe characters in union */
  int tld = 0;     /* Number of tilde characters */

  for (x = 0; aExpr[x] && (aExpr[x] != aStop1) && (aExpr[x] != aStop2); ++x) {
    switch (aExpr[x]) {
      case '~':
        if (tld) {              /* at most one exclusion */
          return INVALID_SXP;
        }
        if (aStop1) {           /* no exclusions within unions */
          return INVALID_SXP;
        }
        if (!aExpr[x + 1]) {    /* exclusion cannot be last character */
          return INVALID_SXP;
        }
        if (!x) {               /* exclusion cannot be first character */
          return INVALID_SXP;
        }
        ++tld;
      /* fall through */
      case '*':
      case '?':
      case '$':
        ++nsc;
        break;
      case '[':
        ++nsc;
        if ((!aExpr[++x]) || (aExpr[x] == ']')) {
          return INVALID_SXP;
        }
        for (; aExpr[x] && (aExpr[x] != ']'); ++x) {
          if (aExpr[x] == '\\' && !aExpr[++x]) {
            return INVALID_SXP;
          }
        }
        if (!aExpr[x]) {
          return INVALID_SXP;
        }
        break;
      case '(':
        ++nsc;
        if (aStop1) {           /* no nested unions */
          return INVALID_SXP;
        }
        np = -1;
        do {
          int t = ::_valid_subexp(&aExpr[++x], T(')'), T('|'));
          if (t == 0 || t == INVALID_SXP) {
            return INVALID_SXP;
          }
          x += t;
          if (!aExpr[x]) {
            return INVALID_SXP;
          }
          ++np;
        } while (aExpr[x] == '|');
        if (np < 1) { /* must be at least one pipe */
          return INVALID_SXP;
        }
        break;
      case ')':
      case ']':
      case '|':
        return INVALID_SXP;
      case '\\':
        ++nsc;
        if (!aExpr[++x]) {
          return INVALID_SXP;
        }
        break;
      default:
        break;
    }
  }
  if (!aStop1 && !nsc) { /* must be at least one special character */
    return NON_SXP;
  }
  return ((aExpr[x] == aStop1 || aExpr[x] == aStop2) ? x : INVALID_SXP);
}


template<class T>
int
NS_WildCardValid_(const T* aExpr)
{
  int x = ::_valid_subexp(aExpr, T('\0'), T('\0'));
  return (x < 0 ? x : VALID_SXP);
}

int
NS_WildCardValid(const char* aExpr)
{
  return NS_WildCardValid_(aExpr);
}

int
NS_WildCardValid(const char16_t* aExpr)
{
  return NS_WildCardValid_(aExpr);
}

/* ----------------------------- _shexp_match ----------------------------- */


#define MATCH 0
#define NOMATCH 1
#define ABORTED -1

template<class T>
static int
_shexp_match(const T* aStr, const T* aExpr, bool aCaseInsensitive,
             unsigned int aLevel);

/**
 * Count characters until we reach a NUL character or either of the
 * two delimiter characters, stop1 or stop2.  If we encounter a bracketed
 * expression, look only for NUL or ']' inside it.  Do not look for stop1
 * or stop2 inside it. Return ABORTED if bracketed expression is unterminated.
 * Handle all escaping.
 * Return index in input string of first stop found, or ABORTED if not found.
 * If "dest" is non-nullptr, copy counted characters to it and null terminate.
 */
template<class T>
static int
_scan_and_copy(const T* aExpr, T aStop1, T aStop2, T* aDest)
{
  int sx;     /* source index */
  T cc;

  for (sx = 0; (cc = aExpr[sx]) && cc != aStop1 && cc != aStop2; ++sx) {
    if (cc == '\\') {
      if (!aExpr[++sx]) {
        return ABORTED;  /* should be impossible */
      }
    } else if (cc == '[') {
      while ((cc = aExpr[++sx]) && cc != ']') {
        if (cc == '\\' && !aExpr[++sx]) {
          return ABORTED;
        }
      }
      if (!cc) {
        return ABORTED;  /* should be impossible */
      }
    }
  }
  if (aDest && sx) {
    /* Copy all but the closing delimiter. */
    memcpy(aDest, aExpr, sx * sizeof(T));
    aDest[sx] = 0;
  }
  return cc ? sx : ABORTED; /* index of closing delimiter */
}

/* On input, expr[0] is the opening parenthesis of a union.
 * See if any of the alternatives in the union matches as a pattern.
 * The strategy is to take each of the alternatives, in turn, and append
 * the rest of the expression (after the closing ')' that marks the end of
 * this union) to that alternative, and then see if the resultant expression
 * matches the input string.  Repeat this until some alternative matches,
 * or we have an abort.
 */
template<class T>
static int
_handle_union(const T* aStr, const T* aExpr, bool aCaseInsensitive,
              unsigned int aLevel)
{
  int sx;              /* source index */
  int cp;              /* source index of closing parenthesis */
  int count;
  int ret   = NOMATCH;
  T* e2;

  /* Find the closing parenthesis that ends this union in the expression */
  cp = ::_scan_and_copy(aExpr, T(')'), T('\0'), static_cast<T*>(nullptr));
  if (cp == ABORTED || cp < 4) { /* must be at least "(a|b" before ')' */
    return ABORTED;
  }
  ++cp;                /* now index of char after closing parenthesis */
  e2 = (T*)moz_xmalloc((1 + nsCharTraits<T>::length(aExpr)) * sizeof(T));
  if (!e2) {
    return ABORTED;
  }
  for (sx = 1; ; ++sx) {
    /* Here, aExpr[sx] is one character past the preceding '(' or '|'. */
    /* Copy everything up to the next delimiter to e2 */
    count = ::_scan_and_copy(aExpr + sx, T(')'), T('|'), e2);
    if (count == ABORTED || !count) {
      ret = ABORTED;
      break;
    }
    sx += count;
    /* Append everything after closing parenthesis to e2. This is safe. */
    nsCharTraits<T>::copy(e2 + count, aExpr + cp,
                          nsCharTraits<T>::length(aExpr + cp) + 1);
    ret = ::_shexp_match(aStr, e2, aCaseInsensitive, aLevel + 1);
    if (ret != NOMATCH || !aExpr[sx] || aExpr[sx] == ')') {
      break;
    }
  }
  free(e2);
  if (sx < 2) {
    ret = ABORTED;
  }
  return ret;
}

/* returns 1 if val is in range from start..end, case insensitive. */
static int
_is_char_in_range(unsigned char aStart, unsigned char aEnd, unsigned char aVal)
{
  char map[256];
  memset(map, 0, sizeof(map));
  while (aStart <= aEnd) {
    map[lower(aStart++)] = 1;
  }
  return map[lower(aVal)];
}

template<class T>
static int
_shexp_match(const T* aStr, const T* aExpr, bool aCaseInsensitive,
             unsigned int aLevel)
{
  int x;   /* input string index */
  int y;   /* expression index */
  int ret, neg;

  if (aLevel > 20) {    /* Don't let the stack get too deep. */
    return ABORTED;
  }
  for (x = 0, y = 0; aExpr[y]; ++y, ++x) {
    if (!aStr[x] && aExpr[y] != '$' && aExpr[y] != '*') {
      return NOMATCH;
    }
    switch (aExpr[y]) {
      case '$':
        if (aStr[x]) {
          return NOMATCH;
        }
        --x;                 /* we don't want loop to increment x */
        break;
      case '*':
        while (aExpr[++y] == '*') {
        }
        if (!aExpr[y]) {
          return MATCH;
        }
        while (aStr[x]) {
          ret = ::_shexp_match(&aStr[x++], &aExpr[y], aCaseInsensitive,
                               aLevel + 1);
          switch (ret) {
            case NOMATCH:
              continue;
            case ABORTED:
              return ABORTED;
            default:
              return MATCH;
          }
        }
        if (aExpr[y] == '$' && aExpr[y + 1] == '\0' && !aStr[x]) {
          return MATCH;
        } else {
          return NOMATCH;
        }
      case '[': {
        T start, end = 0;
        int i;
        ++y;
        neg = (aExpr[y] == '^' && aExpr[y + 1] != ']');
        if (neg) {
          ++y;
        }
        i = y;
        start = aExpr[i++];
        if (start == '\\') {
          start = aExpr[i++];
        }
        if (::alphanumeric(start) && aExpr[i++] == '-') {
          end = aExpr[i++];
          if (end == '\\') {
            end = aExpr[i++];
          }
        }
        if (::alphanumeric(end) && aExpr[i] == ']') {
          /* This is a range form: a-b */
          T val = aStr[x];
          if (end < start) { /* swap them */
            T tmp = end;
            end = start;
            start = tmp;
          }
          if (aCaseInsensitive && ::alpha(val)) {
            val = ::_is_char_in_range((unsigned char)start,
                                      (unsigned char)end,
                                      (unsigned char)val);
            if (neg == val) {
              return NOMATCH;
            }
          } else if (neg != (val < start || val > end)) {
            return NOMATCH;
          }
          y = i;
        } else {
          /* Not range form */
          int matched = 0;
          for (; aExpr[y] != ']'; ++y) {
            if (aExpr[y] == '\\') {
              ++y;
            }
            if (aCaseInsensitive) {
              matched |= (::upper(aStr[x]) == ::upper(aExpr[y]));
            } else {
              matched |= (aStr[x] == aExpr[y]);
            }
          }
          if (neg == matched) {
            return NOMATCH;
          }
        }
      }
      break;
      case '(':
        if (!aExpr[y + 1]) {
          return ABORTED;
        }
        return ::_handle_union(&aStr[x], &aExpr[y], aCaseInsensitive,
                               aLevel + 1);
      case '?':
        break;
      case ')':
      case ']':
      case '|':
        return ABORTED;
      case '\\':
        ++y;
      /* fall through */
      default:
        if (aCaseInsensitive) {
          if (::upper(aStr[x]) != ::upper(aExpr[y])) {
            return NOMATCH;
          }
        } else {
          if (aStr[x] != aExpr[y]) {
            return NOMATCH;
          }
        }
        break;
    }
  }
  return (aStr[x] ? NOMATCH : MATCH);
}


template<class T>
static int
ns_WildCardMatch(const T* aStr, const T* aXp, bool aCaseInsensitive)
{
  T* expr = nullptr;
  int ret = MATCH;

  if (!nsCharTraits<T>::find(aXp, nsCharTraits<T>::length(aXp), T('~'))) {
    return ::_shexp_match(aStr, aXp, aCaseInsensitive, 0);
  }

  expr = (T*)moz_xmalloc((nsCharTraits<T>::length(aXp) + 1) * sizeof(T));
  if (!expr) {
    return NOMATCH;
  }
  memcpy(expr, aXp, (nsCharTraits<T>::length(aXp) + 1) * sizeof(T));

  int x = ::_scan_and_copy(expr, T('~'), T('\0'), static_cast<T*>(nullptr));
  if (x != ABORTED && expr[x] == '~') {
    expr[x++] = '\0';
    ret = ::_shexp_match(aStr, &expr[x], aCaseInsensitive, 0);
    switch (ret) {
      case NOMATCH:
        ret = MATCH;
        break;
      case MATCH:
        ret = NOMATCH;
        break;
      default:
        break;
    }
  }
  if (ret == MATCH) {
    ret = ::_shexp_match(aStr, expr, aCaseInsensitive, 0);
  }

  free(expr);
  return ret;
}

template<class T>
int
NS_WildCardMatch_(const T* aStr, const T* aExpr, bool aCaseInsensitive)
{
  int is_valid = NS_WildCardValid(aExpr);
  switch (is_valid) {
    case INVALID_SXP:
      return -1;
    default:
      return ::ns_WildCardMatch(aStr, aExpr, aCaseInsensitive);
  }
}

int
NS_WildCardMatch(const char* aStr, const char* aXp, bool aCaseInsensitive)
{
  return NS_WildCardMatch_(aStr, aXp, aCaseInsensitive);
}

int
NS_WildCardMatch(const char16_t* aStr, const char16_t* aXp,
                 bool aCaseInsensitive)
{
  return NS_WildCardMatch_(aStr, aXp, aCaseInsensitive);
}

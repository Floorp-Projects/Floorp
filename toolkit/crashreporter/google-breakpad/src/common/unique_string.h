
#ifndef COMMON_UNIQUE_STRING_H
#define COMMON_UNIQUE_STRING_H

#include <string>
#include <map>
#include "common/using_std_string.h"

// FIXME-remove, is debugging hack
#include <stdio.h>
#include <assert.h>

// Abstract type
class UniqueString;

// Unique-ify a string.  |toUniqueString| can never return NULL.
const UniqueString* toUniqueString(string);

// ditto, starting instead from the first n characters of a C string
const UniqueString* toUniqueString_n(char* str, size_t n);

// Pull chars out of the string.  No range checking.
const char index(const UniqueString*, int);

// Get the contained C string (debugging only)
const char* const fromUniqueString(const UniqueString*);


// Some handy pre-uniqified strings.  Z is an escape character:
//   ZS        '$'
//   ZD        '.'
//   Zeq       '='
//   Zplus     '+'
//   Zstar     '*'
//   Zslash    '/'
//   Zpercent  '%'
//   Zat       '@'
//   Zcaret    '^'

// Note that ustr__empty and (UniqueString*)NULL are considered
// to be different.
//
// Unfortunately these have to be written as functions so as to
// make them safe to use in static initialisers.

// ""
inline static const UniqueString* ustr__empty() {
  static const UniqueString* us = NULL;
  if (!us) us = toUniqueString("");
  return us;
}

// "$eip"
inline static const UniqueString* ustr__ZSeip() {
  static const UniqueString* us = NULL;
  if (!us) us = toUniqueString("$eip");
  return us;
}

// "$ebp"
inline static const UniqueString* ustr__ZSebp() {
  static const UniqueString* us = NULL;
  if (!us) us = toUniqueString("$ebp");
  return us;
}

// "$esp"
inline static const UniqueString* ustr__ZSesp() {
  static const UniqueString* us = NULL;
  if (!us) us = toUniqueString("$esp");
  return us;
}

// "$ebx"
inline static const UniqueString* ustr__ZSebx() {
  static const UniqueString* us = NULL;
  if (!us) us = toUniqueString("$ebx");
  return us;
}

// "$esi"
inline static const UniqueString* ustr__ZSesi() {
  static const UniqueString* us = NULL;
  if (!us) us = toUniqueString("$esi");
  return us;
}

// "$edi"
inline static const UniqueString* ustr__ZSedi() {
  static const UniqueString* us = NULL;
  if (!us) us = toUniqueString("$edi");
  return us;
}

// ".cbCalleeParams"
inline static const UniqueString* ustr__ZDcbCalleeParams() {
  static const UniqueString* us = NULL;
  if (!us) us = toUniqueString(".cbCalleeParams");
  return us;
}

// ".cbSavedRegs"
inline static const UniqueString* ustr__ZDcbSavedRegs() {
  static const UniqueString* us = NULL;
  if (!us) us = toUniqueString(".cbSavedRegs");
  return us;
}

// ".cbLocals"
inline static const UniqueString* ustr__ZDcbLocals() {
  static const UniqueString* us = NULL;
  if (!us) us = toUniqueString(".cbLocals");
  return us;
}

// ".raSearchStart"
inline static const UniqueString* ustr__ZDraSearchStart() {
  static const UniqueString* us = NULL;
  if (!us) us = toUniqueString(".raSearchStart");
  return us;
}

// ".raSearch"
inline static const UniqueString* ustr__ZDraSearch() {
  static const UniqueString* us = NULL;
  if (!us) us = toUniqueString(".raSearch");
  return us;
}

// ".cbParams"
inline static const UniqueString* ustr__ZDcbParams() {
  static const UniqueString* us = NULL;
  if (!us) us = toUniqueString(".cbParams");
  return us;
}

// "+"
inline static const UniqueString* ustr__Zplus() {
  static const UniqueString* us = NULL;
  if (!us) us = toUniqueString("+");
  return us;
}

// "-"
inline static const UniqueString* ustr__Zminus() {
  static const UniqueString* us = NULL;
  if (!us) us = toUniqueString("-");
  return us;
}

// "*"
inline static const UniqueString* ustr__Zstar() {
  static const UniqueString* us = NULL;
  if (!us) us = toUniqueString("*");
  return us;
}

// "/"
inline static const UniqueString* ustr__Zslash() {
  static const UniqueString* us = NULL;
  if (!us) us = toUniqueString("/");
  return us;
}

// "%"
inline static const UniqueString* ustr__Zpercent() {
  static const UniqueString* us = NULL;
  if (!us) us = toUniqueString("%");
  return us;
}

// "@"
inline static const UniqueString* ustr__Zat() {
  static const UniqueString* us = NULL;
  if (!us) us = toUniqueString("@");
  return us;
}

// "^"
inline static const UniqueString* ustr__Zcaret() {
  static const UniqueString* us = NULL;
  if (!us) us = toUniqueString("^");
  return us;
}

// "="
inline static const UniqueString* ustr__Zeq() {
  static const UniqueString* us = NULL;
  if (!us) us = toUniqueString("=");
  return us;
}

// ".cfa"
inline static const UniqueString* ustr__ZDcfa() {
  static const UniqueString* us = NULL;
  if (!us) us = toUniqueString(".cfa");
  return us;
}

// ".ra"
inline static const UniqueString* ustr__ZDra() {
  static const UniqueString* us = NULL;
  if (!us) us = toUniqueString(".ra");
  return us;
}

template <typename ValueType>
class UniqueStringMap
{
 private:
  static const int N_FIXED = 10;

 public:
  /* __attribute__((noinline)) */ UniqueStringMap()
    : n_fixed_(0), n_sets_(0), n_gets_(0), n_clears_(0)
  {
  };

  /* __attribute__((noinline)) */ ~UniqueStringMap()
  {
    if (0)
    fprintf(stderr,
            "~UniqueStringMap: size %2d, sets %2d, gets %2d, clears %2d\n",
            n_fixed_ + (int)map_.size(),
            n_sets_, n_gets_, n_clears_);
  };

  // Empty out the map.
  /* __attribute__((noinline)) */ void clear()
  {
    n_clears_++;
    map_.clear();
    n_fixed_ = 0;
  }

  // Do "map[ix] = v".
  /* __attribute__((noinline)) */ void set(const UniqueString* ix,
                                           ValueType v)
  {
    n_sets_++;
    int i;
    for (i = 0; i < n_fixed_; i++) {
      if (fixed_keys_[i] == ix) {
        fixed_vals_[i] = v;
        return;
      }
    }
    if (n_fixed_ < N_FIXED) {
      i = n_fixed_;
      fixed_keys_[i] = ix;
      fixed_vals_[i] = v;
      n_fixed_++;
    } else {
      map_[ix] = v;
    }
  }

  // Lookup 'ix' in the map, and also return a success/fail boolean.
  /* __attribute__((noinline)) */ ValueType get(/*OUT*/bool* have,
                                                const UniqueString* ix) const
  {
    n_gets_++;
    int i;
    for (i = 0; i < n_fixed_; i++) {
      if (fixed_keys_[i] == ix) {
        *have = true;
        return fixed_vals_[i];
      }
    }
    typename std::map<const UniqueString*, ValueType>::const_iterator it
      = map_.find(ix);
    if (it == map_.end()) {
      *have = false;
      return ValueType();
    } else {
      *have = true;
      return it->second;
    }
  };

  // Lookup 'ix' in the map, and return zero if it is not present.
  /* __attribute__((noinline)) */ ValueType get(const UniqueString* ix)
  {
    n_gets_++;
    bool found;
    ValueType v = get(&found, ix);
    return found ? v : ValueType();
  }

  // Find out whether 'ix' is in the map.
  /* __attribute__((noinline)) */ bool have(const UniqueString* ix) const
  {
    n_gets_++;
    bool found;
    (void)get(&found, ix);
    return found;
  }

  // Note that users of this class rely on having also a sane
  // assignment operator.  The default one is OK, though.
  // AFAICT there are no uses of the copy constructor, but if
  // there were, the default one would also suffice.

 private:
  // Quick (we hope) cache
  const UniqueString* fixed_keys_[N_FIXED];
  ValueType           fixed_vals_[N_FIXED];
  int                 n_fixed_; /* 0 .. N_FIXED inclusive */
  // Fallback storage when the cache is filled
  std::map<const UniqueString*, ValueType> map_;

  mutable int n_sets_, n_gets_, n_clears_;
};

#endif /* ndef COMMON_UNIQUE_STRING_H */

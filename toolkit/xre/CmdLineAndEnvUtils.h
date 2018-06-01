/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef mozilla_CmdLineAndEnvUtils_h
#define mozilla_CmdLineAndEnvUtils_h

// NB: This code may be used outside of xul and thus must not depend on XPCOM

#if defined(MOZILLA_INTERNAL_API)
#include "prenv.h"
#include "prprf.h"
#include <string.h>
#elif defined(XP_WIN)
#include <stdlib.h>
#endif

#if defined(XP_WIN)
#include "mozilla/Move.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/Vector.h"

#include <wchar.h>
#endif // defined(XP_WIN)

#include "mozilla/TypedEnumBits.h"

#include <ctype.h>
#include <stdint.h>

// Undo X11/X.h's definition of None
#undef None

namespace mozilla {

enum ArgResult {
  ARG_NONE  = 0,
  ARG_FOUND = 1,
  ARG_BAD   = 2 // you wanted a param, but there isn't one
};

template <typename CharT>
inline void
RemoveArg(int& argc, CharT **argv)
{
  do {
    *argv = *(argv + 1);
    ++argv;
  } while (*argv);

  --argc;
}

namespace internal {

template <typename FuncT, typename CharT>
static inline bool
strimatch(FuncT aToLowerFn, const CharT* lowerstr, const CharT* mixedstr)
{
  while(*lowerstr) {
    if (!*mixedstr) return false; // mixedstr is shorter
    if (static_cast<CharT>(aToLowerFn(*mixedstr)) != *lowerstr) return false; // no match

    ++lowerstr;
    ++mixedstr;
  }

  if (*mixedstr) return false; // lowerstr is shorter

  return true;
}

} // namespace internal

inline bool
strimatch(const char* lowerstr, const char* mixedstr)
{
  return internal::strimatch(&tolower, lowerstr, mixedstr);
}

inline bool
strimatch(const wchar_t* lowerstr, const wchar_t* mixedstr)
{
  return internal::strimatch(&towlower, lowerstr, mixedstr);
}

enum class FlagLiteral
{
  osint,
  safemode
};

template <typename CharT, FlagLiteral Literal>
inline const CharT* GetLiteral();

#define DECLARE_FLAG_LITERAL(enum_name, literal)                 \
  template <> inline                                             \
  const char* GetLiteral<char, FlagLiteral::enum_name>()         \
  {                                                              \
    return literal;                                              \
  }                                                              \
                                                                 \
  template <> inline                                             \
  const wchar_t* GetLiteral<wchar_t, FlagLiteral::enum_name>()   \
  {                                                              \
    return L##literal;                                           \
  }

DECLARE_FLAG_LITERAL(osint, "osint")
DECLARE_FLAG_LITERAL(safemode, "safe-mode")

enum class CheckArgFlag : uint32_t
{
  None = 0,
  CheckOSInt = (1 << 0), // Retrun ARG_BAD if osint arg is also present.
  RemoveArg = (1 << 1)   // Remove the argument from the argv array.
};

MOZ_MAKE_ENUM_CLASS_BITWISE_OPERATORS(CheckArgFlag)

/**
 * Check for a commandline flag. If the flag takes a parameter, the
 * parameter is returned in aParam. Flags may be in the form -arg or
 * --arg (or /arg on win32).
 *
 * @param aArgc The argc value.
 * @param aArgv The original argv.
 * @param aArg the parameter to check. Must be lowercase.
 * @param aParam if non-null, the -arg <data> will be stored in this pointer.
 *        This is *not* allocated, but rather a pointer to the argv data.
 * @param aFlags Flags @see CheckArgFlag
 */
template <typename CharT>
inline ArgResult
CheckArg(int& aArgc, CharT** aArgv, const CharT* aArg, const CharT **aParam,
         CheckArgFlag aFlags)
{
  MOZ_ASSERT(aArgv && aArg);

  CharT **curarg = aArgv + 1; // skip argv[0]
  ArgResult ar = ARG_NONE;

  while (*curarg) {
    CharT *arg = curarg[0];

    if (arg[0] == '-'
#if defined(XP_WIN)
        || *arg == '/'
#endif
        ) {
      ++arg;

      if (*arg == '-') {
        ++arg;
      }

      if (strimatch(aArg, arg)) {
        if (aFlags & CheckArgFlag::RemoveArg) {
          RemoveArg(aArgc, curarg);
        } else {
          ++curarg;
        }

        if (!aParam) {
          ar = ARG_FOUND;
          break;
        }

        if (*curarg) {
          if (**curarg == '-'
#if defined(XP_WIN)
              || **curarg == '/'
#endif
              ) {
            return ARG_BAD;
          }

          *aParam = *curarg;

          if (aFlags & CheckArgFlag::RemoveArg) {
            RemoveArg(aArgc, curarg);
          }

          ar = ARG_FOUND;
          break;
        }

        return ARG_BAD;
      }
    }

    ++curarg;
  }

  if ((aFlags & CheckArgFlag::CheckOSInt) && ar == ARG_FOUND) {
    ArgResult arOSInt = CheckArg(aArgc, aArgv,
                                 GetLiteral<CharT, FlagLiteral::osint>(),
                                 static_cast<const CharT**>(nullptr),
                                 CheckArgFlag::None);
    if (arOSInt == ARG_FOUND) {
      ar = ARG_BAD;
#if defined(MOZILLA_INTERNAL_API)
      PR_fprintf(PR_STDERR, "Error: argument --osint is invalid\n");
#endif // defined(MOZILLA_INTERNAL_API)
    }
  }

  return ar;
}

#if defined(XP_WIN)

namespace internal {

/**
 * Get the length that the string will take and takes into account the
 * additional length if the string needs to be quoted and if characters need to
 * be escaped.
 */
inline int
ArgStrLen(const wchar_t *s)
{
  int backslashes = 0;
  int i = wcslen(s);
  bool hasDoubleQuote = wcschr(s, L'"') != nullptr;
  // Only add doublequotes if the string contains a space or a tab
  bool addDoubleQuotes = wcspbrk(s, L" \t") != nullptr;

  if (addDoubleQuotes) {
    i += 2; // initial and final duoblequote
  }

  if (hasDoubleQuote) {
    while (*s) {
      if (*s == '\\') {
        ++backslashes;
      } else {
        if (*s == '"') {
          // Escape the doublequote and all backslashes preceding the doublequote
          i += backslashes + 1;
        }

        backslashes = 0;
      }

      ++s;
    }
  }

  return i;
}

/**
 * Copy string "s" to string "d", quoting the argument as appropriate and
 * escaping doublequotes along with any backslashes that immediately precede
 * doublequotes.
 * The CRT parses this to retrieve the original argc/argv that we meant,
 * see STDARGV.C in the MSVC CRT sources.
 *
 * @return the end of the string
 */
inline wchar_t*
ArgToString(wchar_t *d, const wchar_t *s)
{
  int backslashes = 0;
  bool hasDoubleQuote = wcschr(s, L'"') != nullptr;
  // Only add doublequotes if the string contains a space or a tab
  bool addDoubleQuotes = wcspbrk(s, L" \t") != nullptr;

  if (addDoubleQuotes) {
    *d = '"'; // initial doublequote
    ++d;
  }

  if (hasDoubleQuote) {
    int i;
    while (*s) {
      if (*s == '\\') {
        ++backslashes;
      } else {
        if (*s == '"') {
          // Escape the doublequote and all backslashes preceding the doublequote
          for (i = 0; i <= backslashes; ++i) {
            *d = '\\';
            ++d;
          }
        }

        backslashes = 0;
      }

      *d = *s;
      ++d; ++s;
    }
  } else {
    wcscpy(d, s);
    d += wcslen(s);
  }

  if (addDoubleQuotes) {
    *d = '"'; // final doublequote
    ++d;
  }

  return d;
}

} // namespace internal

/**
 * Creates a command line from a list of arguments.
 */
inline UniquePtr<wchar_t[]>
MakeCommandLine(int argc, wchar_t **argv)
{
  int i;
  int len = 0;

  // The + 1 of the last argument handles the allocation for null termination
  for (i = 0; i < argc; ++i) {
    len += internal::ArgStrLen(argv[i]) + 1;
  }

  // Protect against callers that pass 0 arguments
  if (len == 0) {
    len = 1;
  }

  auto s = MakeUnique<wchar_t[]>(len);
  if (!s) {
    return nullptr;
  }

  wchar_t *c = s.get();
  for (i = 0; i < argc; ++i) {
    c = internal::ArgToString(c, argv[i]);
    if (i + 1 != argc) {
      *c = ' ';
      ++c;
    }
  }

  *c = '\0';

  return std::move(s);
}

#endif // defined(XP_WIN)

// Save literal putenv string to environment variable.
inline void
SaveToEnv(const char *aEnvString)
{
#if defined(MOZILLA_INTERNAL_API)
  char *expr = strdup(aEnvString);
  if (expr) {
    PR_SetEnv(expr);
  }

  // We intentionally leak |expr| here since it is required by PR_SetEnv.
  MOZ_LSAN_INTENTIONALLY_LEAK_OBJECT(expr);
#elif defined(XP_WIN)
  // This is the same as the NSPR implementation
  // (Note that we don't need to do a strdup for this case; the CRT makes a copy)
  _putenv(aEnvString);
#else
#error "Not implemented for this configuration"
#endif
}

inline bool
EnvHasValue(const char* aVarName)
{
#if defined(MOZILLA_INTERNAL_API)
  const char* val = PR_GetEnv(aVarName);
  return val && *val;
#elif defined(XP_WIN)
  // This is the same as the NSPR implementation
  const char* val = getenv(aVarName);
  return val && *val;
#else
#error "Not implemented for this configuration"
#endif
}

} // namespace mozilla

#endif // mozilla_CmdLineAndEnvUtils_h

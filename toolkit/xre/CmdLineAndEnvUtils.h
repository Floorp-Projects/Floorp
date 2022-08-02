/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef mozilla_CmdLineAndEnvUtils_h
#define mozilla_CmdLineAndEnvUtils_h

// NB: This code may be used outside of xul and thus must not depend on XPCOM

#if defined(MOZILLA_INTERNAL_API)
#  include "prenv.h"
#  include "prprf.h"
#  include <string.h>
#endif

#if defined(XP_WIN)
#  include "mozilla/UniquePtr.h"
#  include "mozilla/Vector.h"
#  include "mozilla/WinHeaderOnlyUtils.h"

#  include <wchar.h>
#  include <windows.h>
#endif  // defined(XP_WIN)

#include "mozilla/Maybe.h"
#include "mozilla/MemoryChecking.h"
#include "mozilla/TypedEnumBits.h"

#include <ctype.h>
#include <stdint.h>
#include <stdlib.h>

#ifndef NS_NO_XPCOM
#  include "nsIFile.h"
#  include "mozilla/AlreadyAddRefed.h"
#endif

// Undo X11/X.h's definition of None
#undef None

namespace mozilla {

enum ArgResult {
  ARG_NONE = 0,
  ARG_FOUND = 1,
  ARG_BAD = 2  // you wanted a param, but there isn't one
};

template <typename CharT>
inline void RemoveArg(int& argc, CharT** argv) {
  do {
    *argv = *(argv + 1);
    ++argv;
  } while (*argv);

  --argc;
}

namespace internal {

#if 'a' == '\x61'
// Valid option characters must have the same representation in every locale
// (which is true for most of ASCII, barring \x5C and \x7E).
static inline constexpr bool isValidOptionCharacter(char c) {
  // We specifically avoid the use of `islower` here; it's locale-dependent, and
  // may return true for non-ASCII values in some locales.
  return ('0' <= c && c <= '9') || ('a' <= c && c <= 'z') || c == '-';
};

// Convert uppercase to lowercase, locale-insensitively.
static inline constexpr char toLowercase(char c) {
  // We specifically avoid the use of `tolower` here; it's locale-dependent, and
  // may output ASCII values for non-ASCII input (or vice versa) in some
  // locales.
  return ('A' <= c && c <= 'Z') ? char(c | ' ') : c;
};

// Convert a CharT to a char, ensuring that no CharT is mapped to any valid
// option character except the unique CharT naturally corresponding thereto.
template <typename CharT>
static inline constexpr char toNarrow(CharT c) {
  // confirmed to compile down to nothing when `CharT` is `char`
  return (c & static_cast<CharT>(0xff)) == c ? c : 0xff;
};
#else
// The target system's character set isn't even ASCII-compatible. If you're
// porting Gecko to such a platform, you'll have to implement these yourself.
#  error Character conversion functions not implemented for this platform.
#endif

// Case-insensitively compare a string taken from the command-line (`mixedstr`)
// to the text of some known command-line option (`lowerstr`).
template <typename CharT>
static inline bool strimatch(const char* lowerstr, const CharT* mixedstr) {
  while (*lowerstr) {
    if (!*mixedstr) return false;  // mixedstr is shorter

    // Non-ASCII strings may compare incorrectly depending on the user's locale.
    // Some ASCII-safe characters are also dispermitted for semantic reasons
    // and simplicity.
    if (!isValidOptionCharacter(*lowerstr)) return false;

    if (toLowercase(toNarrow(*mixedstr)) != *lowerstr) {
      return false;  // no match
    }

    ++lowerstr;
    ++mixedstr;
  }

  if (*mixedstr) return false;  // lowerstr is shorter

  return true;
}

// Given a command-line argument, return Nothing if it isn't structurally a
// command-line option, and Some(<the option text>) if it is.
template <typename CharT>
mozilla::Maybe<const CharT*> ReadAsOption(const CharT* str) {
  if (!str) {
    return Nothing();
  }
  if (*str == '-') {
    str++;
    if (*str == '-') {
      str++;
    }
    return Some(str);
  }
#ifdef XP_WIN
  if (*str == '/') {
    return Some(str + 1);
  }
#endif
  return Nothing();
}

}  // namespace internal

using internal::strimatch;

const wchar_t kCommandLineDelimiter[] = L" \t";

enum class CheckArgFlag : uint32_t {
  None = 0,
  // (1 << 0) Used to be CheckOSInt
  RemoveArg = (1 << 1)  // Remove the argument from the argv array.
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
inline ArgResult CheckArg(int& aArgc, CharT** aArgv, const char* aArg,
                          const CharT** aParam = nullptr,
                          CheckArgFlag aFlags = CheckArgFlag::RemoveArg) {
  using internal::ReadAsOption;
  MOZ_ASSERT(aArgv && aArg);

  CharT** curarg = aArgv + 1;  // skip argv[0]
  ArgResult ar = ARG_NONE;

  while (*curarg) {
    if (const auto arg = ReadAsOption(*curarg)) {
      if (strimatch(aArg, arg.value())) {
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
          if (ReadAsOption(*curarg)) {
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

  return ar;
}

template <typename CharT>
inline ArgResult CheckArg(int& aArgc, CharT** aArgv, const char* aArg,
                          std::nullptr_t,
                          CheckArgFlag aFlags = CheckArgFlag::RemoveArg) {
  return CheckArg<CharT>(aArgc, aArgv, aArg,
                         static_cast<const CharT**>(nullptr), aFlags);
}

namespace internal {
// template <typename T>
// constexpr bool IsStringRange =
//     std::convertible_to<std::ranges::range_value_t<T>, const char *>;

template <typename CharT, typename ListT>
// requires IsStringRange<ListT>
static bool MatchesAnyOf(CharT const* unknown, ListT const& known) {
  for (const char* k : known) {
    if (strimatch(k, unknown)) {
      return true;
    }
  }
  return false;
}

template <typename CharT, typename ReqContainerT, typename OptContainerT>
// requires IsStringRange<ReqContainerT> && IsStringRange<OptContainerT>
inline bool EnsureCommandlineSafeImpl(int aArgc, CharT** aArgv,
                                      ReqContainerT const& requiredParams,
                                      OptContainerT const& optionalParams) {
  // We expect either no -osint, or the full commandline to be:
  //
  //   app -osint [<optional-param>...] <required-param> <required-argument>
  //
  // Otherwise, we abort to avoid abuse of other command-line handlers from apps
  // that do a poor job escaping links they give to the OS.
  //
  // Note that the above implies that optional parameters do not themselves take
  // arguments. This is a security feature, to prevent the possible injection of
  // additional parameters via such arguments. (See, e.g., bug 384384.)

  static constexpr const char* osintLit = "osint";

  // If "-osint" (or the equivalent) is not present, then this is trivially
  // satisfied.
  if (CheckArg(aArgc, aArgv, osintLit, nullptr, CheckArgFlag::None) !=
      ARG_FOUND) {
    return true;
  }

  // There should be at least 4 items present:
  //    <app name> -osint <required param> <arg>.
  if (aArgc < 4) {
    return false;
  }

  // The first parameter must be osint.
  const auto arg1 = ReadAsOption(aArgv[1]);
  if (!arg1) return false;
  if (!strimatch(osintLit, arg1.value())) {
    return false;
  }
  // Following this is any number of optional parameters, terminated by a
  // required parameter.
  int pos = 2;
  while (true) {
    if (pos >= aArgc) return false;

    auto const arg = ReadAsOption(aArgv[pos]);
    if (!arg) return false;

    if (MatchesAnyOf(arg.value(), optionalParams)) {
      ++pos;
      continue;
    }

    if (MatchesAnyOf(arg.value(), requiredParams)) {
      ++pos;
      break;
    }

    return false;
  }

  // There must be one argument remaining...
  if (pos + 1 != aArgc) return false;
  // ... which must not be another option.
  if (ReadAsOption(aArgv[pos])) {
    return false;
  }

  // Nothing ill-formed was passed.
  return true;
}

// C (and so C++) disallows empty arrays. Rather than require callers to jump
// through hoops to specify an empty optional-argument list, allow either its
// omission or its specification as `nullptr`, and do the hoop-jumping here.
//
// No such facility is provided for requiredParams, which must have at least one
// entry.
template <typename CharT, typename ReqContainerT>
inline bool EnsureCommandlineSafeImpl(int aArgc, CharT** aArgv,
                                      ReqContainerT const& requiredParams,
                                      std::nullptr_t _ = nullptr) {
  struct {
    inline const char** begin() const { return nullptr; }
    inline const char** end() const { return nullptr; }
  } emptyContainer;
  return EnsureCommandlineSafeImpl(aArgc, aArgv, requiredParams,
                                   emptyContainer);
}
}  // namespace internal

template <typename CharT, typename ReqContainerT,
          typename OptContainerT = std::nullptr_t>
inline void EnsureCommandlineSafe(
    int aArgc, CharT** aArgv, ReqContainerT const& requiredParams,
    OptContainerT const& optionalParams = nullptr) {
  if (!internal::EnsureCommandlineSafeImpl(aArgc, aArgv, requiredParams,
                                           optionalParams)) {
    exit(127);
  }
}

#if defined(XP_WIN)
namespace internal {
/**
 * Get the length that the string will take and takes into account the
 * additional length if the string needs to be quoted and if characters need to
 * be escaped.
 */
inline int ArgStrLen(const wchar_t* s) {
  int backslashes = 0;
  int i = wcslen(s);
  bool hasDoubleQuote = wcschr(s, L'"') != nullptr;
  // Only add doublequotes if the string contains a space or a tab
  bool addDoubleQuotes = wcspbrk(s, kCommandLineDelimiter) != nullptr;

  if (addDoubleQuotes) {
    i += 2;  // initial and final duoblequote
  }

  if (hasDoubleQuote) {
    while (*s) {
      if (*s == '\\') {
        ++backslashes;
      } else {
        if (*s == '"') {
          // Escape the doublequote and all backslashes preceding the
          // doublequote
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
inline wchar_t* ArgToString(wchar_t* d, const wchar_t* s) {
  int backslashes = 0;
  bool hasDoubleQuote = wcschr(s, L'"') != nullptr;
  // Only add doublequotes if the string contains a space or a tab
  bool addDoubleQuotes = wcspbrk(s, kCommandLineDelimiter) != nullptr;

  if (addDoubleQuotes) {
    *d = '"';  // initial doublequote
    ++d;
  }

  if (hasDoubleQuote) {
    int i;
    while (*s) {
      if (*s == '\\') {
        ++backslashes;
      } else {
        if (*s == '"') {
          // Escape the doublequote and all backslashes preceding the
          // doublequote
          for (i = 0; i <= backslashes; ++i) {
            *d = '\\';
            ++d;
          }
        }

        backslashes = 0;
      }

      *d = *s;
      ++d;
      ++s;
    }
  } else {
    wcscpy(d, s);
    d += wcslen(s);
  }

  if (addDoubleQuotes) {
    *d = '"';  // final doublequote
    ++d;
  }

  return d;
}

}  // namespace internal

/**
 * Creates a command line from a list of arguments.
 *
 * @param argc Number of elements in |argv|
 * @param argv Array of arguments
 * @param aArgcExtra Number of elements in |aArgvExtra|
 * @param aArgvExtra Optional array of arguments to be appended to the resulting
 *                   command line after those provided by |argv|.
 */
inline UniquePtr<wchar_t[]> MakeCommandLine(
    int argc, const wchar_t* const* argv, int aArgcExtra = 0,
    const wchar_t* const* aArgvExtra = nullptr) {
  int i;
  int len = 0;

  // The + 1 for each argument reserves space for either a ' ' or the null
  // terminator, depending on the position of the argument.
  for (i = 0; i < argc; ++i) {
    len += internal::ArgStrLen(argv[i]) + 1;
  }

  for (i = 0; i < aArgcExtra; ++i) {
    len += internal::ArgStrLen(aArgvExtra[i]) + 1;
  }

  // Protect against callers that pass 0 arguments
  if (len == 0) {
    len = 1;
  }

  auto s = MakeUnique<wchar_t[]>(len);

  int totalArgc = argc + aArgcExtra;

  wchar_t* c = s.get();
  for (i = 0; i < argc; ++i) {
    c = internal::ArgToString(c, argv[i]);
    if (i + 1 != totalArgc) {
      *c = ' ';
      ++c;
    }
  }

  for (i = 0; i < aArgcExtra; ++i) {
    c = internal::ArgToString(c, aArgvExtra[i]);
    if (i + 1 != aArgcExtra) {
      *c = ' ';
      ++c;
    }
  }

  *c = '\0';

  return s;
}

inline bool SetArgv0ToFullBinaryPath(wchar_t* aArgv[]) {
  if (!aArgv) {
    return false;
  }

  UniquePtr<wchar_t[]> newArgv_0(GetFullBinaryPath());
  if (!newArgv_0) {
    return false;
  }

  // We intentionally leak newArgv_0 into argv[0]
  aArgv[0] = newArgv_0.release();
  MOZ_LSAN_INTENTIONALLY_LEAK_OBJECT(aArgv[0]);
  return true;
}

#  if defined(MOZILLA_INTERNAL_API)
// This class converts a command line string into an array of the arguments.
// It's basically the opposite of MakeCommandLine.  However, the behavior is
// different from ::CommandLineToArgvW in several ways, such as escaping a
// backslash or quoting an argument containing whitespaces.  This satisfies
// the examples at:
// https://docs.microsoft.com/en-us/cpp/cpp/main-function-command-line-args#results-of-parsing-command-lines
// https://docs.microsoft.com/en-us/previous-versions/17w5ykft(v=vs.85)
template <typename T>
class CommandLineParserWin final {
  int mArgc;
  T** mArgv;

  void Release() {
    if (mArgv) {
      while (mArgc) {
        delete[] mArgv[--mArgc];
      }
      delete[] mArgv;
      mArgv = nullptr;
    }
  }

 public:
  CommandLineParserWin() : mArgc(0), mArgv(nullptr) {}
  ~CommandLineParserWin() { Release(); }

  CommandLineParserWin(const CommandLineParserWin&) = delete;
  CommandLineParserWin(CommandLineParserWin&&) = delete;
  CommandLineParserWin& operator=(const CommandLineParserWin&) = delete;
  CommandLineParserWin& operator=(CommandLineParserWin&&) = delete;

  int Argc() const { return mArgc; }
  const T* const* Argv() const { return mArgv; }

  // Returns the number of characters handled
  int HandleCommandLine(const nsTSubstring<T>& aCmdLineString) {
    Release();

    if (aCmdLineString.IsEmpty()) {
      return 0;
    }

    int justCounting = 1;
    // Flags, etc.
    int init = 1;
    int between, quoted, bSlashCount;
    const T* p;
    const T* const pEnd = aCmdLineString.EndReading();
    nsTAutoString<T> arg;

    // We loop if we've not finished the second pass through.
    while (1) {
      // Initialize if required.
      if (init) {
        p = aCmdLineString.BeginReading();
        between = 1;
        mArgc = quoted = bSlashCount = 0;

        init = 0;
      }

      const T charCurr = (p < pEnd) ? *p : 0;
      const T charNext = (p + 1 < pEnd) ? *(p + 1) : 0;

      if (between) {
        // We are traversing whitespace between args.
        // Check for start of next arg.
        if (charCurr != 0 && !wcschr(kCommandLineDelimiter, charCurr)) {
          // Start of another arg.
          between = 0;
          arg.Truncate();
          switch (charCurr) {
            case '\\':
              // Count the backslash.
              bSlashCount = 1;
              break;
            case '"':
              // Remember we're inside quotes.
              quoted = 1;
              break;
            default:
              // Add character to arg.
              arg += charCurr;
              break;
          }
        } else {
          // Another space between args, ignore it.
        }
      } else {
        // We are processing the contents of an argument.
        // Check for whitespace or end.
        if (charCurr == 0 ||
            (!quoted && wcschr(kCommandLineDelimiter, charCurr))) {
          // Process pending backslashes (interpret them
          // literally since they're not followed by a ").
          while (bSlashCount) {
            arg += '\\';
            bSlashCount--;
          }
          // End current arg.
          if (!justCounting) {
            mArgv[mArgc] = new T[arg.Length() + 1];
            memcpy(mArgv[mArgc], arg.get(), (arg.Length() + 1) * sizeof(T));
          }
          mArgc++;
          // We're now between args.
          between = 1;
        } else {
          // Still inside argument, process the character.
          switch (charCurr) {
            case '"':
              // First, digest preceding backslashes (if any).
              while (bSlashCount > 1) {
                // Put one backsplash in arg for each pair.
                arg += '\\';
                bSlashCount -= 2;
              }
              if (bSlashCount) {
                // Quote is literal.
                arg += '"';
                bSlashCount = 0;
              } else {
                // Quote starts or ends a quoted section.
                if (quoted) {
                  // Check for special case of consecutive double
                  // quotes inside a quoted section.
                  if (charNext == '"') {
                    // This implies a literal double-quote.  Fake that
                    // out by causing next double-quote to look as
                    // if it was preceded by a backslash.
                    bSlashCount = 1;
                  } else {
                    quoted = 0;
                  }
                } else {
                  quoted = 1;
                }
              }
              break;
            case '\\':
              // Add to count.
              bSlashCount++;
              break;
            default:
              // Accept any preceding backslashes literally.
              while (bSlashCount) {
                arg += '\\';
                bSlashCount--;
              }
              // Just add next char to the current arg.
              arg += charCurr;
              break;
          }
        }
      }

      // Check for end of input.
      if (charCurr) {
        // Go to next character.
        p++;
      } else {
        // If on first pass, go on to second.
        if (justCounting) {
          // Allocate argv array.
          mArgv = new T*[mArgc];

          // Start second pass
          justCounting = 0;
          init = 1;
        } else {
          // Quit.
          break;
        }
      }
    }

    return p - aCmdLineString.BeginReading();
  }
};
#  endif  // defined(MOZILLA_INTERNAL_API)

#endif  // defined(XP_WIN)

// SaveToEnv and EnvHasValue are only available on Windows or when
// MOZILLA_INTERNAL_API is defined
#if defined(MOZILLA_INTERNAL_API) || defined(XP_WIN)

// Save literal putenv string to environment variable.
MOZ_NEVER_INLINE inline void SaveToEnv(const char* aEnvString) {
#  if defined(MOZILLA_INTERNAL_API)
  char* expr = strdup(aEnvString);
  if (expr) {
    PR_SetEnv(expr);
  }

  // We intentionally leak |expr| here since it is required by PR_SetEnv.
  MOZ_LSAN_INTENTIONALLY_LEAK_OBJECT(expr);
#  elif defined(XP_WIN)
  // This is the same as the NSPR implementation
  // (Note that we don't need to do a strdup for this case; the CRT makes a
  // copy)
  _putenv(aEnvString);
#  endif
}

inline bool EnvHasValue(const char* aVarName) {
#  if defined(MOZILLA_INTERNAL_API)
  const char* val = PR_GetEnv(aVarName);
  return val && *val;
#  elif defined(XP_WIN)
  // This is the same as the NSPR implementation
  const char* val = getenv(aVarName);
  return val && *val;
#  endif
}

#endif  // end windows/internal_api-only definitions

#ifndef NS_NO_XPCOM
already_AddRefed<nsIFile> GetFileFromEnv(const char* name);
#endif

}  // namespace mozilla

#endif  // mozilla_CmdLineAndEnvUtils_h

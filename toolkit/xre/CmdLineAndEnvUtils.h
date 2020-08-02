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
#elif defined(XP_WIN)
#  include <stdlib.h>
#endif

#if defined(XP_WIN)
#  include "mozilla/UniquePtr.h"
#  include "mozilla/Vector.h"
#  include "mozilla/WinHeaderOnlyUtils.h"

#  include <wchar.h>
#  include <windows.h>
#endif  // defined(XP_WIN)

#include "mozilla/MemoryChecking.h"
#include "mozilla/TypedEnumBits.h"

#include <ctype.h>
#include <stdint.h>

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

template <typename FuncT, typename CharT>
static inline bool strimatch(FuncT aToLowerFn, const CharT* lowerstr,
                             const CharT* mixedstr) {
  while (*lowerstr) {
    if (!*mixedstr) return false;  // mixedstr is shorter
    if (static_cast<CharT>(aToLowerFn(*mixedstr)) != *lowerstr)
      return false;  // no match

    ++lowerstr;
    ++mixedstr;
  }

  if (*mixedstr) return false;  // lowerstr is shorter

  return true;
}

}  // namespace internal

inline bool strimatch(const char* lowerstr, const char* mixedstr) {
  return internal::strimatch(&tolower, lowerstr, mixedstr);
}

inline bool strimatch(const wchar_t* lowerstr, const wchar_t* mixedstr) {
  return internal::strimatch(&towlower, lowerstr, mixedstr);
}

const wchar_t kCommandLineDelimiter[] = L" \t";

enum class FlagLiteral { osint, safemode };

template <typename CharT, FlagLiteral Literal>
inline const CharT* GetLiteral();

#define DECLARE_FLAG_LITERAL(enum_name, literal)                        \
  template <>                                                           \
  inline const char* GetLiteral<char, FlagLiteral::enum_name>() {       \
    return literal;                                                     \
  }                                                                     \
                                                                        \
  template <>                                                           \
  inline const wchar_t* GetLiteral<wchar_t, FlagLiteral::enum_name>() { \
    return L##literal;                                                  \
  }

DECLARE_FLAG_LITERAL(osint, "osint")
DECLARE_FLAG_LITERAL(safemode, "safe-mode")

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
inline ArgResult CheckArg(int& aArgc, CharT** aArgv, const CharT* aArg,
                          const CharT** aParam = nullptr,
                          CheckArgFlag aFlags = CheckArgFlag::RemoveArg) {
  MOZ_ASSERT(aArgv && aArg);

  CharT** curarg = aArgv + 1;  // skip argv[0]
  ArgResult ar = ARG_NONE;

  while (*curarg) {
    CharT* arg = curarg[0];

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

  return ar;
}

template <typename CharT>
inline void EnsureCommandlineSafe(int& aArgc, CharT** aArgv,
                                  const CharT** aAcceptableArgs) {
  // We expect either no -osint, or the full commandline to be:
  // app -osint
  // followed by one of the arguments listed in aAcceptableArgs,
  // followed by one parameter for that arg.
  // If this varies, we abort to avoid abuse of other commandline handlers
  // from apps that do a poor job escaping links they give to the OS.

  const CharT* osintLit = GetLiteral<CharT, FlagLiteral::osint>();

  if (CheckArg(aArgc, aArgv, osintLit, static_cast<const CharT**>(nullptr),
               CheckArgFlag::None) == ARG_FOUND) {
    // There should be 4 items left (app name + -osint + (acceptable arg) +
    // param)
    if (aArgc != 4) {
      exit(127);
    }

    // The first should be osint.
    CharT* arg = aArgv[1];
    if (*arg != '-'
#ifdef XP_WIN
        && *arg != '/'
#endif
    ) {
      exit(127);
    }
    ++arg;
    if (*arg == '-') {
      ++arg;
    }
    if (!strimatch(osintLit, arg)) {
      exit(127);
    }
    // Strip it:
    RemoveArg(aArgc, aArgv + 1);

    // Now only an acceptable argument and a parameter for it should be left:
    arg = aArgv[1];
    if (*arg != '-'
#ifdef XP_WIN
        && *arg != '/'
#endif
    ) {
      exit(127);
    }
    ++arg;
    if (*arg == '-') {
      ++arg;
    }
    bool haveAcceptableArg = false;
    const CharT** acceptableArg = aAcceptableArgs;
    while (*acceptableArg) {
      if (strimatch(*acceptableArg, arg)) {
        haveAcceptableArg = true;
        break;
      }
      acceptableArg++;
    }
    if (!haveAcceptableArg) {
      exit(127);
    }
    // The param that is passed afterwards shouldn't be another switch:
    arg = aArgv[2];
    if (*arg == '-'
#ifdef XP_WIN
        || *arg == '/'
#endif
    ) {
      exit(127);
    }
  }
  // Either no osint, so nothing to do, or we ensured nothing nefarious was
  // passed.
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
  if (!s) {
    return s;
  }

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
// backslash or quoting an argument containing whitespaces.
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

  void HandleCommandLine(const T* aCmdLineString) {
    Release();

    int justCounting = 1;
    // Flags, etc.
    int init = 1;
    int between, quoted, bSlashCount;
    const T* p;
    nsTAutoString<T> arg;

    // Parse command line args according to MS spec
    // (see "Parsing C++ Command-Line Arguments" at
    // http://msdn.microsoft.com/library/devprods/vs6/visualc/vclang/_pluslang_parsing_c.2b2b_.command.2d.line_arguments.htm).
    // We loop if we've not finished the second pass through.
    while (1) {
      // Initialize if required.
      if (init) {
        p = aCmdLineString;
        between = 1;
        mArgc = quoted = bSlashCount = 0;

        init = 0;
      }
      if (between) {
        // We are traversing whitespace between args.
        // Check for start of next arg.
        if (*p != 0 && !wcschr(kCommandLineDelimiter, *p)) {
          // Start of another arg.
          between = 0;
          arg.Truncate();
          switch (*p) {
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
              arg += *p;
              break;
          }
        } else {
          // Another space between args, ignore it.
        }
      } else {
        // We are processing the contents of an argument.
        // Check for whitespace or end.
        if (*p == 0 || (!quoted && wcschr(kCommandLineDelimiter, *p))) {
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
          switch (*p) {
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
                  if (*(p + 1) == '"') {
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
              arg += *p;
              break;
          }
        }
      }
      // Check for end of input.
      if (*p) {
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

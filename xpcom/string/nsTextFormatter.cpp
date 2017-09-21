/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Portable safe sprintf code.
 *
 * Code based on mozilla/nsprpub/src/io/prprf.c rev 3.7
 *
 * Contributor(s):
 *   Kipp E.B. Hickman  <kipp@netscape.com>  (original author)
 *   Frank Yung-Fong Tang  <ftang@netscape.com>
 *   Daniele Nicolodi  <daniele@grinta.net>
 */

/*
 * Copied from xpcom/ds/nsTextFormatter.cpp r1.22
 *     Changed to use nsMemory and Frozen linkage
 *                  -- Prasad <prasad@medhas.org>
 */

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "prdtoa.h"
#include "mozilla/Logging.h"
#include "mozilla/Sprintf.h"
#include "nsCRTGlue.h"
#include "nsTextFormatter.h"
#include "nsMemory.h"

struct nsTextFormatter::SprintfStateStr
{
  int (*stuff)(SprintfStateStr* aState, const char16_t* aStr, uint32_t aLen);

  char16_t* base;
  char16_t* cur;
  uint32_t maxlen;

  void* stuffclosure;
};

#define _LEFT		0x1
#define _SIGNED		0x2
#define _SPACED		0x4
#define _ZEROS		0x8
#define _NEG		0x10
#define _UNSIGNED       0x20

#define ELEMENTS_OF(array_) (sizeof(array_) / sizeof(array_[0]))

/*
** Fill into the buffer using the data in src
*/
int
nsTextFormatter::fill2(SprintfStateStr* aState, const char16_t* aSrc, int aSrcLen, int aWidth,
                       int aFlags)
{
  char16_t space = ' ';
  int rv;

  aWidth -= aSrcLen;
  /* Right adjusting */
  if ((aWidth > 0) && ((aFlags & _LEFT) == 0)) {
    if (aFlags & _ZEROS) {
      space = '0';
    }
    while (--aWidth >= 0) {
      rv = (*aState->stuff)(aState, &space, 1);
      if (rv < 0) {
        return rv;
      }
    }
  }

  /* Copy out the source data */
  rv = (*aState->stuff)(aState, aSrc, aSrcLen);
  if (rv < 0) {
    return rv;
  }

  /* Left adjusting */
  if ((aWidth > 0) && ((aFlags & _LEFT) != 0)) {
    while (--aWidth >= 0) {
      rv = (*aState->stuff)(aState, &space, 1);
      if (rv < 0) {
        return rv;
      }
    }
  }
  return 0;
}

/*
** Fill a number. The order is: optional-sign zero-filling conversion-digits
*/
int
nsTextFormatter::fill_n(nsTextFormatter::SprintfStateStr* aState, const char16_t* aSrc,
                        int aSrcLen, int aWidth, int aPrec, int aFlags)
{
  int zerowidth   = 0;
  int precwidth   = 0;
  int signwidth   = 0;
  int leftspaces  = 0;
  int rightspaces = 0;
  int cvtwidth;
  int rv;
  char16_t sign;
  char16_t space = ' ';
  char16_t zero = '0';

  if ((aFlags & _UNSIGNED) == 0) {
    if (aFlags & _NEG) {
      sign = '-';
      signwidth = 1;
    } else if (aFlags & _SIGNED) {
      sign = '+';
      signwidth = 1;
    } else if (aFlags & _SPACED) {
      sign = ' ';
      signwidth = 1;
    }
  }
  cvtwidth = signwidth + aSrcLen;

  if (aPrec > 0) {
    if (aPrec > aSrcLen) {
      /* Need zero filling */
      precwidth = aPrec - aSrcLen;
      cvtwidth += precwidth;
    }
  }

  if ((aFlags & _ZEROS) && (aPrec < 0)) {
    if (aWidth > cvtwidth) {
      /* Zero filling */
      zerowidth = aWidth - cvtwidth;
      cvtwidth += zerowidth;
    }
  }

  if (aFlags & _LEFT) {
    if (aWidth > cvtwidth) {
      /* Space filling on the right (i.e. left adjusting) */
      rightspaces = aWidth - cvtwidth;
    }
  } else {
    if (aWidth > cvtwidth) {
      /* Space filling on the left (i.e. right adjusting) */
      leftspaces = aWidth - cvtwidth;
    }
  }
  while (--leftspaces >= 0) {
    rv = (*aState->stuff)(aState, &space, 1);
    if (rv < 0) {
      return rv;
    }
  }
  if (signwidth) {
    rv = (*aState->stuff)(aState, &sign, 1);
    if (rv < 0) {
      return rv;
    }
  }
  while (--precwidth >= 0) {
    rv = (*aState->stuff)(aState,  &space, 1);
    if (rv < 0) {
      return rv;
    }
  }
  while (--zerowidth >= 0) {
    rv = (*aState->stuff)(aState,  &zero, 1);
    if (rv < 0) {
      return rv;
    }
  }
  rv = (*aState->stuff)(aState, aSrc, aSrcLen);
  if (rv < 0) {
    return rv;
  }
  while (--rightspaces >= 0) {
    rv = (*aState->stuff)(aState,  &space, 1);
    if (rv < 0) {
      return rv;
    }
  }
  return 0;
}

/*
** Convert a 64-bit integer into its printable form
*/
int
nsTextFormatter::cvt_ll(SprintfStateStr* aState, uint64_t aNum, int aWidth, int aPrec, int aRadix,
                        int aFlags, const char16_t* aHexStr)
{
  char16_t cvtbuf[100];
  char16_t* cvt;
  int digits;

  /* according to the man page this needs to happen */
  if (aPrec == 0 && aNum == 0) {
    return 0;
  }

  /*
  ** Converting decimal is a little tricky. In the unsigned case we
  ** need to stop when we hit 10 digits. In the signed case, we can
  ** stop when the number is zero.
  */
  cvt = &cvtbuf[0] + ELEMENTS_OF(cvtbuf);
  digits = 0;
  while (aNum != 0) {
    uint64_t quot = aNum / aRadix;
    uint64_t rem = aNum % aRadix;
    *--cvt = aHexStr[rem & 0xf];
    digits++;
    aNum = quot;
  }
  if (digits == 0) {
    *--cvt = '0';
    digits++;
  }

  /*
  ** Now that we have the number converted without its sign, deal with
  ** the sign and zero padding.
  */
  return fill_n(aState, cvt, digits, aWidth, aPrec, aFlags);
}

/*
** Convert a double precision floating point number into its printable
** form.
*/
int
nsTextFormatter::cvt_f(SprintfStateStr* aState, double aDouble, int aWidth, int aPrec,
                       const char16_t aType, int aFlags)
{
  int    mode = 2;
  int    decpt;
  int    sign;
  char   buf[256];
  char*  bufp = buf;
  int    bufsz = 256;
  char   num[256];
  char*  nump;
  char*  endnum;
  int    numdigits = 0;
  char   exp = 'e';

  if (aPrec == -1) {
    aPrec = 6;
  } else if (aPrec > 50) {
    // limit precision to avoid PR_dtoa bug 108335
    // and to prevent buffers overflows
    aPrec = 50;
  }

  switch (aType) {
    case 'f':
      numdigits = aPrec;
      mode = 3;
      break;
    case 'E':
      exp = 'E';
      MOZ_FALLTHROUGH;
    case 'e':
      numdigits = aPrec + 1;
      mode = 2;
      break;
    case 'G':
      exp = 'E';
      MOZ_FALLTHROUGH;
    case 'g':
      if (aPrec == 0) {
        aPrec = 1;
      }
      numdigits = aPrec;
      mode = 2;
      break;
    default:
      NS_ERROR("invalid aType passed to cvt_f");
  }

  if (PR_dtoa(aDouble, mode, numdigits, &decpt, &sign,
              &endnum, num, bufsz) == PR_FAILURE) {
    buf[0] = '\0';
    return -1;
  }
  numdigits = endnum - num;
  nump = num;

  if (sign) {
    *bufp++ = '-';
  } else if (aFlags & _SIGNED) {
    *bufp++ = '+';
  }

  if (decpt == 9999) {
    while ((*bufp++ = *nump++)) {
    }
  } else {

    switch (aType) {

      case 'E':
      case 'e':

        *bufp++ = *nump++;
        if (aPrec > 0) {
          *bufp++ = '.';
          while (*nump) {
            *bufp++ = *nump++;
            aPrec--;
          }
          while (aPrec-- > 0) {
            *bufp++ = '0';
          }
        }
        *bufp++ = exp;

        ::snprintf(bufp, bufsz - (bufp - buf), "%+03d", decpt - 1);
        break;

      case 'f':

        if (decpt < 1) {
          *bufp++ = '0';
          if (aPrec > 0) {
            *bufp++ = '.';
            while (decpt++ && aPrec-- > 0) {
              *bufp++ = '0';
            }
            while (*nump && aPrec-- > 0) {
              *bufp++ = *nump++;
            }
            while (aPrec-- > 0) {
              *bufp++ = '0';
            }
          }
        } else {
          while (*nump && decpt-- > 0) {
            *bufp++ = *nump++;
          }
          while (decpt-- > 0) {
            *bufp++ = '0';
          }
          if (aPrec > 0) {
            *bufp++ = '.';
            while (*nump && aPrec-- > 0) {
              *bufp++ = *nump++;
            }
            while (aPrec-- > 0) {
              *bufp++ = '0';
            }
          }
        }
        *bufp = '\0';
        break;

      case 'G':
      case 'g':

        if ((decpt < -3) || ((decpt - 1) >= aPrec)) {
          *bufp++ = *nump++;
          numdigits--;
          if (numdigits > 0) {
            *bufp++ = '.';
            while (*nump) {
              *bufp++ = *nump++;
            }
          }
          *bufp++ = exp;
          ::snprintf(bufp, bufsz - (bufp - buf), "%+03d", decpt - 1);
        } else {
          if (decpt < 1) {
            *bufp++ = '0';
            if (aPrec > 0) {
              *bufp++ = '.';
              while (decpt++) {
                *bufp++ = '0';
              }
              while (*nump) {
                *bufp++ = *nump++;
              }
            }
          } else {
            while (*nump && decpt-- > 0) {
              *bufp++ = *nump++;
              numdigits--;
            }
            while (decpt-- > 0) {
              *bufp++ = '0';
            }
            if (numdigits > 0) {
              *bufp++ = '.';
              while (*nump) {
                *bufp++ = *nump++;
              }
            }
          }
          *bufp = '\0';
        }
    }
  }

  char16_t rbuf[256];
  char16_t* rbufp = rbuf;
  bufp = buf;
  // cast to char16_t
  while ((*rbufp++ = *bufp++)) {
  }
  *rbufp = '\0';

  return fill2(aState, rbuf, NS_strlen(rbuf), aWidth, aFlags);
}

/*
** Convert a string into its printable form. |aWidth| is the output
** width. |aPrec| is the maximum number of characters of |aStr| to output,
** where -1 means until NUL.
*/
int
nsTextFormatter::cvt_S(SprintfStateStr* aState, const char16_t* aStr, int aWidth, int aPrec,
                       int aFlags)
{
  int slen;

  if (aPrec == 0) {
    return 0;
  }

  /* Limit string length by precision value */
  slen = aStr ? NS_strlen(aStr) : 6;
  if (aPrec > 0) {
    if (aPrec < slen) {
      slen = aPrec;
    }
  }

  /* and away we go */
  return fill2(aState, aStr ? aStr : u"(null)", slen, aWidth, aFlags);
}

/*
** Convert a string into its printable form.  |aWidth| is the output
** width. |aPrec| is the maximum number of characters of |aStr| to output,
** where -1 means until NUL.
*/
int
nsTextFormatter::cvt_s(nsTextFormatter::SprintfStateStr* aState, const char* aStr, int aWidth,
                       int aPrec, int aFlags)
{
  // Be sure to handle null the same way as %S.
  if (aStr == nullptr) {
    return cvt_S(aState, nullptr, aWidth, aPrec, aFlags);
  }
  NS_ConvertUTF8toUTF16 utf16Val(aStr);
  return cvt_S(aState, utf16Val.get(), aWidth, aPrec, aFlags);
}

/*
** The workhorse sprintf code.
*/
int
nsTextFormatter::dosprintf(SprintfStateStr* aState, const char16_t* aFmt,
                           mozilla::Span<BoxedValue> aValues)
{
  static const char16_t space = ' ';
  static const char16_t hex[] = u"0123456789abcdef";
  static char16_t HEX[] = u"0123456789ABCDEF";
  static const BoxedValue emptyString(u"");

  char16_t c;
  int flags, width, prec, radix;

  const char16_t* hexp;

  // Next argument for non-numbered arguments.
  size_t nextNaturalArg = 0;
  // True if we ever saw a numbered argument.
  bool sawNumberedArg = false;

  while ((c = *aFmt++) != 0) {
    int rv;

    if (c != '%') {
      rv = (*aState->stuff)(aState, aFmt - 1, 1);
      if (rv < 0) {
        return rv;
      }
      continue;
    }

    // Save the location of the "%" in case we decide it isn't a
    // format and want to just emit the text from the format string.
    const char16_t* percentPointer = aFmt - 1;

    /*
    ** Gobble up the % format string. Hopefully we have handled all
    ** of the strange cases!
    */
    flags = 0;
    c = *aFmt++;
    if (c == '%') {
      /* quoting a % with %% */
      rv = (*aState->stuff)(aState, aFmt - 1, 1);
      if (rv < 0) {
        return rv;
      }
      continue;
    }

    // Check for a numbered argument.
    bool sawWidth = false;
    const BoxedValue* thisArg = nullptr;
    if (c >= '0' && c <= '9') {
      size_t argNumber = 0;
      while (c && c >= '0' && c <= '9') {
        argNumber = (argNumber * 10) + (c - '0');
        c = *aFmt++;
      }

      if (c == '$') {
        // Mixing numbered arguments and implicit arguments is
        // disallowed.
        if (nextNaturalArg > 0) {
          return -1;
        }

        c = *aFmt++;

        // Numbered arguments start at 1.
        --argNumber;
        if (argNumber >= aValues.Length()) {
          // A correctness issue but not a safety issue.
          MOZ_ASSERT(false);
          thisArg = &emptyString;
        } else {
          thisArg = &aValues[argNumber];
        }
        sawNumberedArg = true;
      } else {
        width = argNumber;
        sawWidth = true;
      }
    }

    if (thisArg == nullptr) {
      // Mixing numbered arguments and implicit arguments is
      // disallowed.
      if (sawNumberedArg) {
        return -1;
      }

      if (nextNaturalArg >= aValues.Length()) {
        // A correctness issue but not a safety issue.
        MOZ_ASSERT(false);
        thisArg = &emptyString;
      } else {
        thisArg = &aValues[nextNaturalArg++];
      }
    }

    if (!sawWidth) {
      /*
       * Examine optional flags.  Note that we do not implement the
       * '#' flag of sprintf().  The ANSI C spec. of the '#' flag is
       * somewhat ambiguous and not ideal, which is perhaps why
       * the various sprintf() implementations are inconsistent
       * on this feature.
       */
      while ((c == '-') || (c == '+') || (c == ' ') || (c == '0')) {
        if (c == '-') {
          flags |= _LEFT;
        }
        if (c == '+') {
          flags |= _SIGNED;
        }
        if (c == ' ') {
          flags |= _SPACED;
        }
        if (c == '0') {
          flags |= _ZEROS;
        }
        c = *aFmt++;
      }
      if (flags & _SIGNED) {
        flags &= ~_SPACED;
      }
      if (flags & _LEFT) {
        flags &= ~_ZEROS;
      }

      /* width */
      if (c == '*') {
        // Not supported with numbered arguments.
        if (sawNumberedArg) {
          return -1;
        }

        if (nextNaturalArg >= aValues.Length() || !aValues[nextNaturalArg].IntCompatible()) {
          // A correctness issue but not a safety issue.
          MOZ_ASSERT(false);
          width = 0;
        } else {
          width = aValues[nextNaturalArg++].mValue.mInt;
        }
        c = *aFmt++;
      } else {
        width = 0;
        while ((c >= '0') && (c <= '9')) {
          width = (width * 10) + (c - '0');
          c = *aFmt++;
        }
      }
    }

    /* precision */
    prec = -1;
    if (c == '.') {
      c = *aFmt++;
      if (c == '*') {
        // Not supported with numbered arguments.
        if (sawNumberedArg) {
          return -1;
        }

        if (nextNaturalArg >= aValues.Length() || !aValues[nextNaturalArg].IntCompatible()) {
          // A correctness issue but not a safety issue.
          MOZ_ASSERT(false);
        } else {
          prec = aValues[nextNaturalArg++].mValue.mInt;
        }
        c = *aFmt++;
      } else {
        prec = 0;
        while ((c >= '0') && (c <= '9')) {
          prec = (prec * 10) + (c - '0');
          c = *aFmt++;
        }
      }
    }

    /* Size.  Defaults to 32 bits.  */
    uint64_t mask = UINT32_MAX;
    if (c == 'h') {
      c = *aFmt++;
      mask = UINT16_MAX;
    } else if (c == 'L') {
      c = *aFmt++;
      mask = UINT64_MAX;
    } else if (c == 'l') {
      c = *aFmt++;
      if (c == 'l') {
        c = *aFmt++;
        mask = UINT64_MAX;
      } else {
        mask = UINT32_MAX;
      }
    }

    /* format */
    hexp = hex;
    radix = 10;
    // Several `MOZ_ASSERT`s below check for argument compatibility
    // with the format specifier.  These are only debug assertions,
    // not release assertions, and exist to catch problems in C++
    // callers of `nsTextFormatter`, as we do not have compile-time
    // checking of format strings.  In release mode, these assertions
    // will be no-ops, and we will fall through to printing the
    // argument based on the known type of the argument.
    switch (c) {
      case 'd':
      case 'i':                               /* decimal/integer */
        MOZ_ASSERT(thisArg->IntCompatible());
        break;

      case 'o':                               /* octal */
        MOZ_ASSERT(thisArg->IntCompatible());
        radix = 8;
        flags |= _UNSIGNED;
        break;

      case 'u':                               /* unsigned decimal */
        MOZ_ASSERT(thisArg->IntCompatible());
        radix = 10;
        flags |= _UNSIGNED;
        break;

      case 'x':                               /* unsigned hex */
        MOZ_ASSERT(thisArg->IntCompatible());
        radix = 16;
        flags |= _UNSIGNED;
        break;

      case 'X':                               /* unsigned HEX */
        MOZ_ASSERT(thisArg->IntCompatible());
        radix = 16;
        hexp = HEX;
        flags |= _UNSIGNED;
        break;

      case 'e':
      case 'E':
      case 'f':
      case 'g':
      case 'G':
        MOZ_ASSERT(thisArg->mKind == DOUBLE);
        // Type-based printing below.
        break;

      case 'S':
        MOZ_ASSERT(thisArg->mKind == STRING16);
        // Type-based printing below.
        break;

      case 's':
        MOZ_ASSERT(thisArg->mKind == STRING);
        // Type-based printing below.
        break;

      case 'c': {
          if (!thisArg->IntCompatible()) {
            MOZ_ASSERT(false);
            // Type-based printing below.
            break;
          }

          if ((flags & _LEFT) == 0) {
            while (width-- > 1) {
              rv = (*aState->stuff)(aState, &space, 1);
              if (rv < 0) {
                return rv;
              }
            }
          }
          char16_t ch = thisArg->mValue.mInt;
          rv = (*aState->stuff)(aState, &ch, 1);
          if (rv < 0) {
            return rv;
          }
          if (flags & _LEFT) {
            while (width-- > 1) {
              rv = (*aState->stuff)(aState, &space, 1);
              if (rv < 0) {
                return rv;
              }
            }
          }
        }
        continue;

      case 'p':
        if (!thisArg->PointerCompatible()) {
            MOZ_ASSERT(false);
            break;
        }
        static_assert(sizeof(uint64_t) >= sizeof(void*), "pointers are larger than 64 bits");
        rv = cvt_ll(aState, uintptr_t(thisArg->mValue.mPtr), width, prec, 16, flags | _UNSIGNED,
                    hexp);
        if (rv < 0) {
          return rv;
        }
        continue;

      case 'n':
        if (thisArg->mKind != INTPOINTER) {
          return -1;
        }

        if (thisArg->mValue.mIntPtr != nullptr) {
          *thisArg->mValue.mIntPtr = aState->cur - aState->base;
        }
        continue;

      default:
        /* Not a % token after all... skip it */
        rv = (*aState->stuff)(aState, percentPointer, aFmt - percentPointer);
        if (rv < 0) {
          return rv;
        }
        continue;
    }

    // If we get here, we want to handle the argument according to its
    // actual type; modified by the flags as appropriate.
    switch (thisArg->mKind) {
      case INT:
      case UINT: {
          int64_t val = thisArg->mValue.mInt;
          if ((flags & _UNSIGNED) == 0 && val < 0) {
            val = -val;
            flags |= _NEG;
          }
          rv = cvt_ll(aState, uint64_t(val) & mask, width, prec, radix, flags, hexp);
        }
        break;
      case INTPOINTER:
      case POINTER:
        // Always treat these as unsigned hex, no matter the format.
        static_assert(sizeof(uint64_t) >= sizeof(void*), "pointers are larger than 64 bits");
        rv = cvt_ll(aState, uintptr_t(thisArg->mValue.mPtr), width, prec, 16, flags | _UNSIGNED,
                    hexp);
        break;
      case DOUBLE:
        if (c != 'f' && c != 'E' && c != 'e' && c != 'G' && c != 'g') {
          // Pick some default.
          c = 'g';
        }
        rv = cvt_f(aState, thisArg->mValue.mDouble, width, prec, c, flags);
        break;
      case STRING:
        rv = cvt_s(aState, thisArg->mValue.mString, width, prec, flags);
        break;
      case STRING16:
        rv = cvt_S(aState, thisArg->mValue.mString16, width, prec, flags);
        break;
      default:
        // Can't happen.
        MOZ_ASSERT(0);
    }

    if (rv < 0) {
      return rv;
    }
  }

  return 0;
}

/************************************************************************/

int
nsTextFormatter::StringStuff(nsTextFormatter::SprintfStateStr* aState, const char16_t* aStr,
                             uint32_t aLen)
{
  ptrdiff_t off = aState->cur - aState->base;

  nsAString* str = static_cast<nsAString*>(aState->stuffclosure);
  str->Append(aStr, aLen);

  aState->base = str->BeginWriting();
  aState->cur = aState->base + off;

  return 0;
}

void
nsTextFormatter::vssprintf(nsAString& aOut, const char16_t* aFmt,
                           mozilla::Span<BoxedValue> aValues)
{
  SprintfStateStr ss;
  ss.stuff = StringStuff;
  ss.base = 0;
  ss.cur = 0;
  ss.maxlen = 0;
  ss.stuffclosure = &aOut;

  aOut.Truncate();
  dosprintf(&ss, aFmt, aValues);
}

/*
** Stuff routine that discards overflow data
*/
int
nsTextFormatter::LimitStuff(SprintfStateStr* aState, const char16_t* aStr, uint32_t aLen)
{
  uint32_t limit = aState->maxlen - (aState->cur - aState->base);

  if (aLen > limit) {
    aLen = limit;
  }
  while (aLen) {
    --aLen;
    *aState->cur++ = *aStr++;
  }
  return 0;
}

uint32_t
nsTextFormatter::vsnprintf(char16_t* aOut, uint32_t aOutLen,
                           const char16_t* aFmt, mozilla::Span<BoxedValue> aValues)
{
  SprintfStateStr ss;

  MOZ_ASSERT((int32_t)aOutLen > 0);
  if ((int32_t)aOutLen <= 0) {
    return 0;
  }

  ss.stuff = LimitStuff;
  ss.base = aOut;
  ss.cur = aOut;
  ss.maxlen = aOutLen;
  int result = dosprintf(&ss, aFmt, aValues);

  if (ss.cur == ss.base) {
    return 0;
  }

  // Append a NUL.  However, be sure not to count it in the returned
  // length.
  if (ss.cur - ss.base >= ptrdiff_t(ss.maxlen)) {
    --ss.cur;
  }
  *ss.cur = '\0';

  // Check the result now, so that an unterminated string can't
  // possibly escape.
  if (result < 0) {
    return -1;
  }

  return ss.cur - ss.base;
}

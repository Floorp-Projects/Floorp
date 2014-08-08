/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "xpcom-private.h"

//-----------------------------------------------------------------------------
// XP_MACOSX or ANDROID
//-----------------------------------------------------------------------------
#if defined(XP_MACOSX) || defined(ANDROID)

#include "nsAString.h"
#include "nsReadableUtils.h"
#include "nsString.h"

nsresult
NS_CopyNativeToUnicode(const nsACString& aInput, nsAString& aOutput)
{
  CopyUTF8toUTF16(aInput, aOutput);
  return NS_OK;
}

nsresult
NS_CopyUnicodeToNative(const nsAString&  aInput, nsACString& aOutput)
{
  CopyUTF16toUTF8(aInput, aOutput);
  return NS_OK;
}

void
NS_StartupNativeCharsetUtils()
{
}

void
NS_ShutdownNativeCharsetUtils()
{
}


//-----------------------------------------------------------------------------
// XP_UNIX
//-----------------------------------------------------------------------------
#elif defined(XP_UNIX)

#include <stdlib.h>   // mbtowc, wctomb
#include <locale.h>   // setlocale
#include "mozilla/Mutex.h"
#include "nscore.h"
#include "nsAString.h"
#include "nsReadableUtils.h"

using namespace mozilla;

//
// choose a conversion library.  we used to use mbrtowc/wcrtomb under Linux,
// but that doesn't work for non-BMP characters whether we use '-fshort-wchar'
// or not (see bug 206811 and
// news://news.mozilla.org:119/bajml3$fvr1@ripley.netscape.com). we now use
// iconv for all platforms where nltypes.h and nllanginfo.h are present
// along with iconv.
//
#if defined(HAVE_ICONV) && defined(HAVE_NL_TYPES_H) && defined(HAVE_LANGINFO_CODESET)
#define USE_ICONV 1
#else
#define USE_STDCONV 1
#endif

static void
isolatin1_to_utf16(const char** aInput, uint32_t* aInputLeft,
                   char16_t** aOutput, uint32_t* aOutputLeft)
{
  while (*aInputLeft && *aOutputLeft) {
    **aOutput = (unsigned char)** aInput;
    (*aInput)++;
    (*aInputLeft)--;
    (*aOutput)++;
    (*aOutputLeft)--;
  }
}

static void
utf16_to_isolatin1(const char16_t** aInput, uint32_t* aInputLeft,
                   char** aOutput, uint32_t* aOutputLeft)
{
  while (*aInputLeft && *aOutputLeft) {
    **aOutput = (unsigned char)**aInput;
    (*aInput)++;
    (*aInputLeft)--;
    (*aOutput)++;
    (*aOutputLeft)--;
  }
}

//-----------------------------------------------------------------------------
// conversion using iconv
//-----------------------------------------------------------------------------
#if defined(USE_ICONV)
#include <nl_types.h> // CODESET
#include <langinfo.h> // nl_langinfo
#include <iconv.h>    // iconv_open, iconv, iconv_close
#include <errno.h>
#include "plstr.h"

#if defined(HAVE_ICONV_WITH_CONST_INPUT)
#define ICONV_INPUT(x) (x)
#else
#define ICONV_INPUT(x) ((char **)x)
#endif

// solaris definitely needs this, but we'll enable it by default
// just in case... but we know for sure that iconv(3) in glibc
// doesn't need this.
#if !defined(__GLIBC__)
#define ENABLE_UTF8_FALLBACK_SUPPORT
#endif

#define INVALID_ICONV_T ((iconv_t)-1)

static inline size_t
xp_iconv(iconv_t converter,
         const char** aInput, size_t* aInputLeft,
         char** aOutput, size_t* aOutputLeft)
{
  size_t res, outputAvail = aOutputLeft ? *aOutputLeft : 0;
  res = iconv(converter, ICONV_INPUT(aInput), aInputLeft, aOutput, aOutputLeft);
  if (res == (size_t)-1) {
    // on some platforms (e.g., linux) iconv will fail with
    // E2BIG if it cannot convert _all_ of its input.  it'll
    // still adjust all of the in/out params correctly, so we
    // can ignore this error.  the assumption is that we will
    // be called again to complete the conversion.
    if ((errno == E2BIG) && (*aOutputLeft < outputAvail)) {
      res = 0;
    }
  }
  return res;
}

static inline void
xp_iconv_reset(iconv_t converter)
{
  // NOTE: the man pages on Solaris claim that you can pass nullptr
  // for all parameter to reset the converter, but beware the
  // evil Solaris crash if you go down this route >:-)

  const char* zero_char_in_ptr  = nullptr;
  char* zero_char_out_ptr = nullptr;
  size_t zero_size_in = 0;
  size_t zero_size_out = 0;

  xp_iconv(converter,
           &zero_char_in_ptr,
           &zero_size_in,
           &zero_char_out_ptr,
           &zero_size_out);
}

static inline iconv_t
xp_iconv_open(const char** to_list, const char** from_list)
{
  iconv_t res;
  const char** from_name;
  const char** to_name;

  // try all possible combinations to locate a converter.
  to_name = to_list;
  while (*to_name) {
    if (**to_name) {
      from_name = from_list;
      while (*from_name) {
        if (**from_name) {
          res = iconv_open(*to_name, *from_name);
          if (res != INVALID_ICONV_T) {
            return res;
          }
        }
        from_name++;
      }
    }
    to_name++;
  }

  return INVALID_ICONV_T;
}

/*
 * char16_t[] is NOT a UCS-2 array BUT a UTF-16 string. Therefore, we
 * have to use UTF-16 with iconv(3) on platforms where it's supported.
 * However, the way UTF-16 and UCS-2 are interpreted varies across platforms
 * and implementations of iconv(3). On Tru64, it also depends on the environment
 * variable. To avoid the trouble arising from byte-swapping
 * (bug 208809), we have to try UTF-16LE/BE and UCS-2LE/BE before falling
 * back to UTF-16 and UCS-2 and variants. We assume that UTF-16 and UCS-2
 * on systems without UTF-16LE/BE and UCS-2LE/BE have the native endianness,
 * which isn't the case of glibc 2.1.x, for which we use 'UNICODELITTLE'
 * and 'UNICODEBIG'. It's also not true of Tru64 V4 when the environment
 * variable ICONV_BYTEORDER is set to 'big-endian', about which not much
 * can be done other than adding a note in the release notes. (bug 206811)
 */
static const char* UTF_16_NAMES[] = {
#if defined(IS_LITTLE_ENDIAN)
  "UTF-16LE",
#if defined(__GLIBC__)
  "UNICODELITTLE",
#endif
  "UCS-2LE",
#else
  "UTF-16BE",
#if defined(__GLIBC__)
  "UNICODEBIG",
#endif
  "UCS-2BE",
#endif
  "UTF-16",
  "UCS-2",
  "UCS2",
  "UCS_2",
  "ucs-2",
  "ucs2",
  "ucs_2",
  nullptr
};

#if defined(ENABLE_UTF8_FALLBACK_SUPPORT)
static const char* UTF_8_NAMES[] = {
  "UTF-8",
  "UTF8",
  "UTF_8",
  "utf-8",
  "utf8",
  "utf_8",
  nullptr
};
#endif

static const char* ISO_8859_1_NAMES[] = {
  "ISO-8859-1",
#if !defined(__GLIBC__)
  "ISO8859-1",
  "ISO88591",
  "ISO_8859_1",
  "ISO8859_1",
  "iso-8859-1",
  "iso8859-1",
  "iso88591",
  "iso_8859_1",
  "iso8859_1",
#endif
  nullptr
};

class nsNativeCharsetConverter
{
public:
  nsNativeCharsetConverter();
  ~nsNativeCharsetConverter();

  nsresult NativeToUnicode(const char** aInput, uint32_t* aInputLeft,
                           char16_t** aOutput, uint32_t* aOutputLeft);
  nsresult UnicodeToNative(const char16_t** aInput, uint32_t* aInputLeft,
                           char** aOutput, uint32_t* aOutputLeft);

  static void GlobalInit();
  static void GlobalShutdown();
  static bool IsNativeUTF8();

private:
  static iconv_t gNativeToUnicode;
  static iconv_t gUnicodeToNative;
#if defined(ENABLE_UTF8_FALLBACK_SUPPORT)
  static iconv_t gNativeToUTF8;
  static iconv_t gUTF8ToNative;
  static iconv_t gUnicodeToUTF8;
  static iconv_t gUTF8ToUnicode;
#endif
  static Mutex*  gLock;
  static bool    gInitialized;
  static bool    gIsNativeUTF8;

  static void LazyInit();

  static void Lock()
  {
    if (gLock) {
      gLock->Lock();
    }
  }
  static void Unlock()
  {
    if (gLock) {
      gLock->Unlock();
    }
  }
};

iconv_t nsNativeCharsetConverter::gNativeToUnicode = INVALID_ICONV_T;
iconv_t nsNativeCharsetConverter::gUnicodeToNative = INVALID_ICONV_T;
#if defined(ENABLE_UTF8_FALLBACK_SUPPORT)
iconv_t nsNativeCharsetConverter::gNativeToUTF8    = INVALID_ICONV_T;
iconv_t nsNativeCharsetConverter::gUTF8ToNative    = INVALID_ICONV_T;
iconv_t nsNativeCharsetConverter::gUnicodeToUTF8   = INVALID_ICONV_T;
iconv_t nsNativeCharsetConverter::gUTF8ToUnicode   = INVALID_ICONV_T;
#endif
Mutex*  nsNativeCharsetConverter::gLock            = nullptr;
bool    nsNativeCharsetConverter::gInitialized     = false;
bool    nsNativeCharsetConverter::gIsNativeUTF8    = false;

void
nsNativeCharsetConverter::LazyInit()
{
  // LazyInit may be called before NS_StartupNativeCharsetUtils, but
  // the setlocale it does has to be called before nl_langinfo. Like in
  // NS_StartupNativeCharsetUtils, assume we are called early enough that
  // we are the first to care about the locale's charset.
  if (!gLock) {
    setlocale(LC_CTYPE, "");
  }
  const char* blank_list[] = { "", nullptr };
  const char** native_charset_list = blank_list;
  const char* native_charset = nl_langinfo(CODESET);
  if (!native_charset) {
    NS_ERROR("native charset is unknown");
    // fallback to ISO-8859-1
    native_charset_list = ISO_8859_1_NAMES;
  } else {
    native_charset_list[0] = native_charset;
  }

  // Most, if not all, Unixen supporting UTF-8 and nl_langinfo(CODESET)
  // return 'UTF-8' (or 'utf-8')
  if (!PL_strcasecmp(native_charset, "UTF-8")) {
    gIsNativeUTF8 = true;
  }

  gNativeToUnicode = xp_iconv_open(UTF_16_NAMES, native_charset_list);
  gUnicodeToNative = xp_iconv_open(native_charset_list, UTF_16_NAMES);

#if defined(ENABLE_UTF8_FALLBACK_SUPPORT)
  if (gNativeToUnicode == INVALID_ICONV_T) {
    gNativeToUTF8 = xp_iconv_open(UTF_8_NAMES, native_charset_list);
    gUTF8ToUnicode = xp_iconv_open(UTF_16_NAMES, UTF_8_NAMES);
    NS_ASSERTION(gNativeToUTF8 != INVALID_ICONV_T, "no native to utf-8 converter");
    NS_ASSERTION(gUTF8ToUnicode != INVALID_ICONV_T, "no utf-8 to utf-16 converter");
  }
  if (gUnicodeToNative == INVALID_ICONV_T) {
    gUnicodeToUTF8 = xp_iconv_open(UTF_8_NAMES, UTF_16_NAMES);
    gUTF8ToNative = xp_iconv_open(native_charset_list, UTF_8_NAMES);
    NS_ASSERTION(gUnicodeToUTF8 != INVALID_ICONV_T, "no utf-16 to utf-8 converter");
    NS_ASSERTION(gUTF8ToNative != INVALID_ICONV_T, "no utf-8 to native converter");
  }
#else
  NS_ASSERTION(gNativeToUnicode != INVALID_ICONV_T, "no native to utf-16 converter");
  NS_ASSERTION(gUnicodeToNative != INVALID_ICONV_T, "no utf-16 to native converter");
#endif

  /*
   * On Solaris 8 (and newer?), the iconv modules converting to UCS-2
   * prepend a byte order mark unicode character (BOM, u+FEFF) during
   * the first use of the iconv converter. The same is the case of
   * glibc 2.2.9x and Tru64 V5 (see bug 208809) when 'UTF-16' is used.
   * However, we use 'UTF-16LE/BE' in both cases, instead so that we
   * should be safe. But just in case...
   *
   * This dummy conversion gets rid of the BOMs and fixes bug 153562.
   */
  char dummy_input[1] = { ' ' };
  char dummy_output[4];

  if (gNativeToUnicode != INVALID_ICONV_T) {
    const char* input = dummy_input;
    size_t input_left = sizeof(dummy_input);
    char* output = dummy_output;
    size_t output_left = sizeof(dummy_output);

    xp_iconv(gNativeToUnicode, &input, &input_left, &output, &output_left);
  }
#if defined(ENABLE_UTF8_FALLBACK_SUPPORT)
  if (gUTF8ToUnicode != INVALID_ICONV_T) {
    const char* input = dummy_input;
    size_t input_left = sizeof(dummy_input);
    char* output = dummy_output;
    size_t output_left = sizeof(dummy_output);

    xp_iconv(gUTF8ToUnicode, &input, &input_left, &output, &output_left);
  }
#endif

  gInitialized = true;
}

void
nsNativeCharsetConverter::GlobalInit()
{
  gLock = new Mutex("nsNativeCharsetConverter.gLock");
}

void
nsNativeCharsetConverter::GlobalShutdown()
{
  delete gLock;
  gLock = nullptr;

  if (gNativeToUnicode != INVALID_ICONV_T) {
    iconv_close(gNativeToUnicode);
    gNativeToUnicode = INVALID_ICONV_T;
  }

  if (gUnicodeToNative != INVALID_ICONV_T) {
    iconv_close(gUnicodeToNative);
    gUnicodeToNative = INVALID_ICONV_T;
  }

#if defined(ENABLE_UTF8_FALLBACK_SUPPORT)
  if (gNativeToUTF8 != INVALID_ICONV_T) {
    iconv_close(gNativeToUTF8);
    gNativeToUTF8 = INVALID_ICONV_T;
  }
  if (gUTF8ToNative != INVALID_ICONV_T) {
    iconv_close(gUTF8ToNative);
    gUTF8ToNative = INVALID_ICONV_T;
  }
  if (gUnicodeToUTF8 != INVALID_ICONV_T) {
    iconv_close(gUnicodeToUTF8);
    gUnicodeToUTF8 = INVALID_ICONV_T;
  }
  if (gUTF8ToUnicode != INVALID_ICONV_T) {
    iconv_close(gUTF8ToUnicode);
    gUTF8ToUnicode = INVALID_ICONV_T;
  }
#endif

  gInitialized = false;
}

nsNativeCharsetConverter::nsNativeCharsetConverter()
{
  Lock();
  if (!gInitialized) {
    LazyInit();
  }
}

nsNativeCharsetConverter::~nsNativeCharsetConverter()
{
  // reset converters for next time
  if (gNativeToUnicode != INVALID_ICONV_T) {
    xp_iconv_reset(gNativeToUnicode);
  }
  if (gUnicodeToNative != INVALID_ICONV_T) {
    xp_iconv_reset(gUnicodeToNative);
  }
#if defined(ENABLE_UTF8_FALLBACK_SUPPORT)
  if (gNativeToUTF8 != INVALID_ICONV_T) {
    xp_iconv_reset(gNativeToUTF8);
  }
  if (gUTF8ToNative != INVALID_ICONV_T) {
    xp_iconv_reset(gUTF8ToNative);
  }
  if (gUnicodeToUTF8 != INVALID_ICONV_T) {
    xp_iconv_reset(gUnicodeToUTF8);
  }
  if (gUTF8ToUnicode != INVALID_ICONV_T) {
    xp_iconv_reset(gUTF8ToUnicode);
  }
#endif
  Unlock();
}

nsresult
nsNativeCharsetConverter::NativeToUnicode(const char** aInput,
                                          uint32_t* aInputLeft,
                                          char16_t** aOutput,
                                          uint32_t* aOutputLeft)
{
  size_t res = 0;
  size_t inLeft = (size_t)*aInputLeft;
  size_t outLeft = (size_t)*aOutputLeft * 2;

  if (gNativeToUnicode != INVALID_ICONV_T) {

    res = xp_iconv(gNativeToUnicode, aInput, &inLeft, (char**)aOutput, &outLeft);

    *aInputLeft = inLeft;
    *aOutputLeft = outLeft / 2;
    if (res != (size_t)-1) {
      return NS_OK;
    }

    NS_WARNING("conversion from native to utf-16 failed");

    // reset converter
    xp_iconv_reset(gNativeToUnicode);
  }
#if defined(ENABLE_UTF8_FALLBACK_SUPPORT)
  else if ((gNativeToUTF8 != INVALID_ICONV_T) &&
           (gUTF8ToUnicode != INVALID_ICONV_T)) {
    // convert first to UTF8, then from UTF8 to UCS2
    const char* in = *aInput;

    char ubuf[1024];

    // we assume we're always called with enough space in |aOutput|,
    // so convert many chars at a time...
    while (inLeft) {
      char* p = ubuf;
      size_t n = sizeof(ubuf);
      res = xp_iconv(gNativeToUTF8, &in, &inLeft, &p, &n);
      if (res == (size_t)-1) {
        NS_ERROR("conversion from native to utf-8 failed");
        break;
      }
      NS_ASSERTION(outLeft > 0, "bad assumption");
      p = ubuf;
      n = sizeof(ubuf) - n;
      res = xp_iconv(gUTF8ToUnicode, (const char**)&p, &n,
                     (char**)aOutput, &outLeft);
      if (res == (size_t)-1) {
        NS_ERROR("conversion from utf-8 to utf-16 failed");
        break;
      }
    }

    (*aInput) += (*aInputLeft - inLeft);
    *aInputLeft = inLeft;
    *aOutputLeft = outLeft / 2;

    if (res != (size_t)-1) {
      return NS_OK;
    }

    // reset converters
    xp_iconv_reset(gNativeToUTF8);
    xp_iconv_reset(gUTF8ToUnicode);
  }
#endif

  // fallback: zero-pad and hope for the best
  // XXX This is lame and we have to do better.
  isolatin1_to_utf16(aInput, aInputLeft, aOutput, aOutputLeft);

  return NS_OK;
}

nsresult
nsNativeCharsetConverter::UnicodeToNative(const char16_t** aInput,
                                          uint32_t* aInputLeft,
                                          char** aOutput,
                                          uint32_t* aOutputLeft)
{
  size_t res = 0;
  size_t inLeft = (size_t)*aInputLeft * 2;
  size_t outLeft = (size_t)*aOutputLeft;

  if (gUnicodeToNative != INVALID_ICONV_T) {
    res = xp_iconv(gUnicodeToNative, (const char**)aInput, &inLeft,
                   aOutput, &outLeft);

    *aInputLeft = inLeft / 2;
    *aOutputLeft = outLeft;
    if (res != (size_t)-1) {
      return NS_OK;
    }

    NS_ERROR("iconv failed");

    // reset converter
    xp_iconv_reset(gUnicodeToNative);
  }
#if defined(ENABLE_UTF8_FALLBACK_SUPPORT)
  else if ((gUnicodeToUTF8 != INVALID_ICONV_T) &&
           (gUTF8ToNative != INVALID_ICONV_T)) {
    const char* in = (const char*)*aInput;

    char ubuf[6]; // max utf-8 char length (really only needs to be 4 bytes)

    // convert one uchar at a time...
    while (inLeft && outLeft) {
      char* p = ubuf;
      size_t n = sizeof(ubuf), one_uchar = sizeof(char16_t);
      res = xp_iconv(gUnicodeToUTF8, &in, &one_uchar, &p, &n);
      if (res == (size_t)-1) {
        NS_ERROR("conversion from utf-16 to utf-8 failed");
        break;
      }
      p = ubuf;
      n = sizeof(ubuf) - n;
      res = xp_iconv(gUTF8ToNative, (const char**)&p, &n, aOutput, &outLeft);
      if (res == (size_t)-1) {
        if (errno == E2BIG) {
          // not enough room for last uchar... back up and return.
          in -= sizeof(char16_t);
          res = 0;
        } else {
          NS_ERROR("conversion from utf-8 to native failed");
        }
        break;
      }
      inLeft -= sizeof(char16_t);
    }

    (*aInput) += (*aInputLeft - inLeft / 2);
    *aInputLeft = inLeft / 2;
    *aOutputLeft = outLeft;
    if (res != (size_t)-1) {
      return NS_OK;
    }

    // reset converters
    xp_iconv_reset(gUnicodeToUTF8);
    xp_iconv_reset(gUTF8ToNative);
  }
#endif

  // fallback: truncate and hope for the best
  // XXX This is lame and we have to do better.
  utf16_to_isolatin1(aInput, aInputLeft, aOutput, aOutputLeft);

  return NS_OK;
}

bool
nsNativeCharsetConverter::IsNativeUTF8()
{
  if (!gInitialized) {
    Lock();
    if (!gInitialized) {
      LazyInit();
    }
    Unlock();
  }
  return gIsNativeUTF8;
}

#endif // USE_ICONV

//-----------------------------------------------------------------------------
// conversion using mb[r]towc/wc[r]tomb
//-----------------------------------------------------------------------------
#if defined(USE_STDCONV)
#if defined(HAVE_WCRTOMB) || defined(HAVE_MBRTOWC)
#include <wchar.h>    // mbrtowc, wcrtomb
#endif

class nsNativeCharsetConverter
{
public:
  nsNativeCharsetConverter();

  nsresult NativeToUnicode(const char** aInput, uint32_t* aInputLeft,
                           char16_t** aOutput, uint32_t* aOutputLeft);
  nsresult UnicodeToNative(const char16_t** aInput, uint32_t* aInputLeft,
                           char** aOutput, uint32_t* aOutputLeft);

  static void GlobalInit();
  static void GlobalShutdown() { }
  static bool IsNativeUTF8();

private:
  static bool gWCharIsUnicode;

#if defined(HAVE_WCRTOMB) || defined(HAVE_MBRTOWC)
  mbstate_t ps;
#endif
};

bool nsNativeCharsetConverter::gWCharIsUnicode = false;

nsNativeCharsetConverter::nsNativeCharsetConverter()
{
#if defined(HAVE_WCRTOMB) || defined(HAVE_MBRTOWC)
  memset(&ps, 0, sizeof(ps));
#endif
}

void
nsNativeCharsetConverter::GlobalInit()
{
  // verify that wchar_t for the current locale is actually unicode.
  // if it is not, then we should avoid calling mbtowc/wctomb and
  // just fallback on zero-pad/truncation conversion.
  //
  // this test cannot be done at build time because the encoding of
  // wchar_t may depend on the runtime locale.  sad, but true!!
  //
  // so, if wchar_t is unicode then converting an ASCII character
  // to wchar_t should not change its numeric value.  we'll just
  // check what happens with the ASCII 'a' character.
  //
  // this test is not perfect... obviously, it could yield false
  // positives, but then at least ASCII text would be converted
  // properly (or maybe just the 'a' character) -- oh well :(

  char a = 'a';
  unsigned int w = 0;

  int res = mbtowc((wchar_t*)&w, &a, 1);

  gWCharIsUnicode = (res != -1 && w == 'a');

#ifdef DEBUG
  if (!gWCharIsUnicode) {
    NS_WARNING("wchar_t is not unicode (unicode conversion will be lossy)");
  }
#endif
}

nsresult
nsNativeCharsetConverter::NativeToUnicode(const char** aInput,
                                          uint32_t* aInputLeft,
                                          char16_t** aOutput,
                                          uint32_t* aOutputLeft)
{
  if (gWCharIsUnicode) {
    int incr;

    // cannot use wchar_t here since it may have been redefined (e.g.,
    // via -fshort-wchar).  hopefully, sizeof(tmp) is sufficient XP.
    unsigned int tmp = 0;
    while (*aInputLeft && *aOutputLeft) {
#ifdef HAVE_MBRTOWC
      incr = (int)mbrtowc((wchar_t*)&tmp, *aInput, *aInputLeft, &ps);
#else
      // XXX is this thread-safe?
      incr = (int)mbtowc((wchar_t*)&tmp, *aInput, *aInputLeft);
#endif
      if (incr < 0) {
        NS_WARNING("mbtowc failed: possible charset mismatch");
        // zero-pad and hope for the best
        tmp = (unsigned char)**aInput;
        incr = 1;
      }
      ** aOutput = (char16_t)tmp;
      (*aInput) += incr;
      (*aInputLeft) -= incr;
      (*aOutput)++;
      (*aOutputLeft)--;
    }
  } else {
    // wchar_t isn't unicode, so the best we can do is treat the
    // input as if it is isolatin1 :(
    isolatin1_to_utf16(aInput, aInputLeft, aOutput, aOutputLeft);
  }

  return NS_OK;
}

nsresult
nsNativeCharsetConverter::UnicodeToNative(const char16_t** aInput,
                                          uint32_t* aInputLeft,
                                          char** aOutput,
                                          uint32_t* aOutputLeft)
{
  if (gWCharIsUnicode) {
    int incr;

    while (*aInputLeft && *aOutputLeft >= MB_CUR_MAX) {
#ifdef HAVE_WCRTOMB
      incr = (int)wcrtomb(*aOutput, (wchar_t)**aInput, &ps);
#else
      // XXX is this thread-safe?
      incr = (int)wctomb(*aOutput, (wchar_t)**aInput);
#endif
      if (incr < 0) {
        NS_WARNING("mbtowc failed: possible charset mismatch");
        ** aOutput = (unsigned char)**aInput; // truncate
        incr = 1;
      }
      // most likely we're dead anyways if this assertion should fire
      NS_ASSERTION(uint32_t(incr) <= *aOutputLeft, "wrote beyond end of string");
      (*aOutput) += incr;
      (*aOutputLeft) -= incr;
      (*aInput)++;
      (*aInputLeft)--;
    }
  } else {
    // wchar_t isn't unicode, so the best we can do is treat the
    // input as if it is isolatin1 :(
    utf16_to_isolatin1(aInput, aInputLeft, aOutput, aOutputLeft);
  }

  return NS_OK;
}

// XXX : for now, return false
bool
nsNativeCharsetConverter::IsNativeUTF8()
{
  return false;
}

#endif // USE_STDCONV

//-----------------------------------------------------------------------------
// API implementation
//-----------------------------------------------------------------------------

nsresult
NS_CopyNativeToUnicode(const nsACString& aInput, nsAString& aOutput)
{
  aOutput.Truncate();

  uint32_t inputLen = aInput.Length();

  nsACString::const_iterator iter;
  aInput.BeginReading(iter);

  //
  // OPTIMIZATION: preallocate space for largest possible result; convert
  // directly into the result buffer to avoid intermediate buffer copy.
  //
  // this will generally result in a larger allocation, but that seems
  // better than an extra buffer copy.
  //
  if (!aOutput.SetLength(inputLen, fallible_t())) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  nsAString::iterator out_iter;
  aOutput.BeginWriting(out_iter);

  char16_t* result = out_iter.get();
  uint32_t resultLeft = inputLen;

  const char* buf = iter.get();
  uint32_t bufLeft = inputLen;

  nsNativeCharsetConverter conv;
  nsresult rv = conv.NativeToUnicode(&buf, &bufLeft, &result, &resultLeft);
  if (NS_SUCCEEDED(rv)) {
    NS_ASSERTION(bufLeft == 0, "did not consume entire input buffer");
    aOutput.SetLength(inputLen - resultLeft);
  }
  return rv;
}

nsresult
NS_CopyUnicodeToNative(const nsAString& aInput, nsACString& aOutput)
{
  aOutput.Truncate();

  nsAString::const_iterator iter, end;
  aInput.BeginReading(iter);
  aInput.EndReading(end);

  // cannot easily avoid intermediate buffer copy.
  char temp[4096];

  nsNativeCharsetConverter conv;

  const char16_t* buf = iter.get();
  uint32_t bufLeft = Distance(iter, end);
  while (bufLeft) {
    char* p = temp;
    uint32_t tempLeft = sizeof(temp);

    nsresult rv = conv.UnicodeToNative(&buf, &bufLeft, &p, &tempLeft);
    if (NS_FAILED(rv)) {
      return rv;
    }

    if (tempLeft < sizeof(temp)) {
      aOutput.Append(temp, sizeof(temp) - tempLeft);
    }
  }
  return NS_OK;
}

bool
NS_IsNativeUTF8()
{
  return nsNativeCharsetConverter::IsNativeUTF8();
}

void
NS_StartupNativeCharsetUtils()
{
  //
  // need to initialize the locale or else charset conversion will fail.
  // better not delay this in case some other component alters the locale
  // settings.
  //
  // XXX we assume that we are called early enough that we should
  // always be the first to care about the locale's charset.
  //
  setlocale(LC_CTYPE, "");

  nsNativeCharsetConverter::GlobalInit();
}

void
NS_ShutdownNativeCharsetUtils()
{
  nsNativeCharsetConverter::GlobalShutdown();
}

//-----------------------------------------------------------------------------
// XP_WIN
//-----------------------------------------------------------------------------
#elif defined(XP_WIN)

#include <windows.h>
#include "nsString.h"
#include "nsAString.h"
#include "nsReadableUtils.h"

using namespace mozilla;

nsresult
NS_CopyNativeToUnicode(const nsACString& aInput, nsAString& aOutput)
{
  uint32_t inputLen = aInput.Length();

  nsACString::const_iterator iter;
  aInput.BeginReading(iter);

  const char* buf = iter.get();

  // determine length of result
  uint32_t resultLen = 0;
  int n = ::MultiByteToWideChar(CP_ACP, 0, buf, inputLen, nullptr, 0);
  if (n > 0) {
    resultLen += n;
  }

  // allocate sufficient space
  if (!aOutput.SetLength(resultLen, fallible_t())) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  if (resultLen > 0) {
    nsAString::iterator out_iter;
    aOutput.BeginWriting(out_iter);

    char16_t* result = out_iter.get();

    ::MultiByteToWideChar(CP_ACP, 0, buf, inputLen, wwc(result), resultLen);
  }
  return NS_OK;
}

nsresult
NS_CopyUnicodeToNative(const nsAString&  aInput, nsACString& aOutput)
{
  uint32_t inputLen = aInput.Length();

  nsAString::const_iterator iter;
  aInput.BeginReading(iter);

  char16ptr_t buf = iter.get();

  // determine length of result
  uint32_t resultLen = 0;

  int n = ::WideCharToMultiByte(CP_ACP, 0, buf, inputLen, nullptr, 0,
                                nullptr, nullptr);
  if (n > 0) {
    resultLen += n;
  }

  // allocate sufficient space
  if (!aOutput.SetLength(resultLen, fallible_t())) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  if (resultLen > 0) {
    nsACString::iterator out_iter;
    aOutput.BeginWriting(out_iter);

    // default "defaultChar" is '?', which is an illegal character on windows
    // file system.  That will cause file uncreatable. Change it to '_'
    const char defaultChar = '_';

    char* result = out_iter.get();

    ::WideCharToMultiByte(CP_ACP, 0, buf, inputLen, result, resultLen,
                          &defaultChar, nullptr);
  }
  return NS_OK;
}

// moved from widget/windows/nsToolkit.cpp
int32_t
NS_ConvertAtoW(const char* aStrInA, int aBufferSize, char16_t* aStrOutW)
{
  return MultiByteToWideChar(CP_ACP, 0, aStrInA, -1, wwc(aStrOutW), aBufferSize);
}

int32_t
NS_ConvertWtoA(const char16_t* aStrInW, int aBufferSizeOut,
               char* aStrOutA, const char* aDefault)
{
  if ((!aStrInW) || (!aStrOutA) || (aBufferSizeOut <= 0)) {
    return 0;
  }

  int numCharsConverted = WideCharToMultiByte(CP_ACP, 0, char16ptr_t(aStrInW), -1,
                                              aStrOutA, aBufferSizeOut,
                                              aDefault, nullptr);

  if (!numCharsConverted) {
    if (GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
      // Overflow, add missing null termination but return 0
      aStrOutA[aBufferSizeOut - 1] = '\0';
    } else {
      // Other error, clear string and return 0
      aStrOutA[0] = '\0';
    }
  } else if (numCharsConverted < aBufferSizeOut) {
    // Add 2nd null (really necessary?)
    aStrOutA[numCharsConverted] = '\0';
  }

  return numCharsConverted;
}

#else

#include "nsReadableUtils.h"

nsresult
NS_CopyNativeToUnicode(const nsACString& aInput, nsAString& aOutput)
{
  CopyASCIItoUTF16(aInput, aOutput);
  return NS_OK;
}

nsresult
NS_CopyUnicodeToNative(const nsAString& aInput, nsACString& aOutput)
{
  LossyCopyUTF16toASCII(aInput, aOutput);
  return NS_OK;
}

void
NS_StartupNativeCharsetUtils()
{
}

void
NS_ShutdownNativeCharsetUtils()
{
}

#endif

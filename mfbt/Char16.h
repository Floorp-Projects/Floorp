/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Implements a UTF-16 character type. */

#ifndef mozilla_Char16_h
#define mozilla_Char16_h

/*
 * C11 and C++11 introduce a char16_t type and support for UTF-16 string and
 * character literals. C++11's char16_t is a distinct builtin type. C11's
 * char16_t is a typedef for uint_least16_t. Technically, char16_t is a 16-bit
 * code unit of a Unicode code point, not a "character".
 */

#ifdef _MSC_VER
   /*
    * C++11 says char16_t is a distinct builtin type, but Windows's yvals.h
    * typedefs char16_t as an unsigned short. We would like to alias char16_t
    * to Windows's 16-bit wchar_t so we can declare UTF-16 literals as constant
    * expressions (and pass char16_t pointers to Windows APIs). We #define
    * _CHAR16T here in order to prevent yvals.h from overriding our char16_t
    * typedefs, which we set to wchar_t for C++ code and to unsigned short for
    * C code.
    *
    * In addition, #defining _CHAR16T will prevent yvals.h from defining a
    * char32_t type, so we have to undo that damage here and provide our own,
    * which is identical to the yvals.h type.
    */
#  define MOZ_UTF16_HELPER(s) L##s
#  define _CHAR16T
#  ifdef __cplusplus
     typedef wchar_t char16_t;
#  else
     typedef unsigned short char16_t;
#  endif
   typedef unsigned int char32_t;
#elif defined(__cplusplus) && \
      (__cplusplus >= 201103L || defined(__GXX_EXPERIMENTAL_CXX0X__))
   /* C++11 has a builtin char16_t type. */
#  define MOZ_UTF16_HELPER(s) u##s
   /**
    * This macro is used to distinguish when char16_t would be a distinct
    * typedef from wchar_t.
    */
#  define MOZ_CHAR16_IS_NOT_WCHAR
#  ifdef WIN32
#    define MOZ_USE_CHAR16_WRAPPER
#  endif
#elif !defined(__cplusplus)
#  if defined(WIN32)
#    include <yvals.h>
     typedef wchar_t char16_t;
#  else
     /**
      * We can't use the stdint.h uint16_t type here because including
      * stdint.h will break building some of our C libraries, such as
      * sqlite.
      */
     typedef unsigned short char16_t;
#  endif
#else
#  error "Char16.h requires C++11 (or something like it) for UTF-16 support."
#endif

#ifdef MOZ_USE_CHAR16_WRAPPER
# include <string>
  /**
   * Win32 API extensively uses wchar_t, which is represented by a separated
   * builtin type than char16_t per spec. It's not the case for MSVC, but GCC
   * follows the spec. We want to mix wchar_t and char16_t on Windows builds.
   * This class is supposed to make it easier. It stores char16_t const pointer,
   * but provides implicit casts for wchar_t as well. On other platforms, we
   * simply use |typedef const char16_t* char16ptr_t|. Here, we want to make
   * the class as similar to this typedef, including providing some casts that
   * are allowed by the typedef.
   */
class char16ptr_t
{
  private:
    const char16_t* ptr;
    static_assert(sizeof(char16_t) == sizeof(wchar_t), "char16_t and wchar_t sizes differ");

  public:
    char16ptr_t(const char16_t* ptr) : ptr(ptr) {}
    char16ptr_t(const wchar_t* ptr) : ptr(reinterpret_cast<const char16_t*>(ptr)) {}

    /* Without this, nullptr assignment would be ambiguous. */
    constexpr char16ptr_t(decltype(nullptr)) : ptr(nullptr) {}

    operator const char16_t*() const {
      return ptr;
    }
    operator const wchar_t*() const {
      return reinterpret_cast<const wchar_t*>(ptr);
    }
    operator const void*() const {
      return ptr;
    }
    operator bool() const {
      return ptr != nullptr;
    }
    operator std::wstring() const {
      return std::wstring(static_cast<const wchar_t*>(*this));
    }

    /* Explicit cast operators to allow things like (char16_t*)str. */
    explicit operator char16_t*() const {
      return const_cast<char16_t*>(ptr);
    }
    explicit operator wchar_t*() const {
      return const_cast<wchar_t*>(static_cast<const wchar_t*>(*this));
    }

    /**
     * Some Windows API calls accept BYTE* but require that data actually be WCHAR*.
     * Supporting this requires explicit operators to support the requisite explicit
     * casts.
     */
    explicit operator const char*() const {
      return reinterpret_cast<const char*>(ptr);
    }
    explicit operator const unsigned char*() const {
      return reinterpret_cast<const unsigned char*>(ptr);
    }
    explicit operator unsigned char*() const {
      return const_cast<unsigned char*>(reinterpret_cast<const unsigned char*>(ptr));
    }
    explicit operator void*() const {
      return const_cast<char16_t*>(ptr);
    }

    /* Some operators used on pointers. */
    char16_t operator[](size_t i) const {
      return ptr[i];
    }
    bool operator==(const char16ptr_t &x) const {
      return ptr == x.ptr;
    }
    bool operator==(decltype(nullptr)) const {
      return ptr == nullptr;
    }
    bool operator!=(const char16ptr_t &x) const {
      return ptr != x.ptr;
    }
    bool operator!=(decltype(nullptr)) const {
      return ptr != nullptr;
    }
    char16ptr_t operator+(size_t add) const {
      return char16ptr_t(ptr + add);
    }
    ptrdiff_t operator-(const char16ptr_t &other) const {
      return ptr - other.ptr;
    }
};

inline decltype((char*)0-(char*)0)
operator-(const char16_t* x, const char16ptr_t y) {
  return x - static_cast<const char16_t*>(y);
}

#else

typedef const char16_t* char16ptr_t;

#endif

/* This is a temporary hack until bug 927728 is fixed. */
#define __PRUNICHAR__
typedef char16_t PRUnichar;

/*
 * Macro arguments used in concatenation or stringification won't be expanded.
 * Therefore, in order for |MOZ_UTF16(FOO)| to work as expected (which is to
 * expand |FOO| before doing whatever |MOZ_UTF16| needs to do to it) a helper
 * macro, |MOZ_UTF16_HELPER| needs to be inserted in between to allow the macro
 * argument to expand. See "3.10.6 Separate Expansion of Macro Arguments" of the
 * CPP manual for a more accurate and precise explanation.
 */
#define MOZ_UTF16(s) MOZ_UTF16_HELPER(s)

#if defined(__cplusplus) && \
    (__cplusplus >= 201103L || defined(__GXX_EXPERIMENTAL_CXX0X__))
static_assert(sizeof(char16_t) == 2, "Is char16_t type 16 bits?");
static_assert(char16_t(-1) > char16_t(0), "Is char16_t type unsigned?");
static_assert(sizeof(MOZ_UTF16('A')) == 2, "Is char literal 16 bits?");
static_assert(sizeof(MOZ_UTF16("")[0]) == 2, "Is string char 16 bits?");
#endif

#endif /* mozilla_Char16_h */

/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jsctypes-test.h"
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include "typedefs.h"

template <typename T>
struct ValueTraits {
  static T literal() { return static_cast<T>(109.25); }
  static T sum(T a, T b) { return a + b; }
  static T sum_many(T a, T b, T c, T d, T e, T f, T g, T h, T i, T j, T k, T l,
                    T m, T n, T o, T p, T q, T r) {
    return a + b + c + d + e + f + g + h + i + j + k + l + m + n + o + p + q +
           r;
  }
};

template <>
struct ValueTraits<bool> {
  typedef bool T;
  static T literal() { return true; }
  static T sum(T a, T b) { return a || b; }
  static T sum_many(T a, T b, T c, T d, T e, T f, T g, T h, T i, T j, T k, T l,
                    T m, T n, T o, T p, T q, T r) {
    return a || b || c || d || e || f || g || h || i || j || k || l || m || n ||
           o || p || q || r;
  }
};

void test_void_t_cdecl() {
  // do nothing
}

// The "AndUnderscore" bit here is an unfortunate hack: the first argument to
// DEFINE_CDECL_FUNCTIONS and DEFINE_STDCALL_FUNCTIONS, in addition to being a
// type, may also be a *macro* on NetBSD -- #define int8_t __int8_t and so on.
// See <http://mail-index.netbsd.org/tech-toolchain/2014/12/18/msg002479.html>.
// And unfortunately, passing that macro as an argument to this macro causes it
// to be expanded -- producing get___int8_t_cdecl() and so on.  Concatenating
// int8_t with _ slightly muddies this code but inhibits expansion. See also
// bug 1113379.
#define FUNCTION_TESTS(nameAndUnderscore, type, ffiType, suffix)              \
  type ABI get_##nameAndUnderscore##suffix() {                                \
    return ValueTraits<type>::literal();                                      \
  }                                                                           \
                                                                              \
  type ABI set_##nameAndUnderscore##suffix(type x) { return x; }              \
                                                                              \
  type ABI sum_##nameAndUnderscore##suffix(type x, type y) {                  \
    return ValueTraits<type>::sum(x, y);                                      \
  }                                                                           \
                                                                              \
  type ABI sum_alignb_##nameAndUnderscore##suffix(char a, type x, char b,     \
                                                  type y, char c) {           \
    return ValueTraits<type>::sum(x, y);                                      \
  }                                                                           \
                                                                              \
  type ABI sum_alignf_##nameAndUnderscore##suffix(float a, type x, float b,   \
                                                  type y, float c) {          \
    return ValueTraits<type>::sum(x, y);                                      \
  }                                                                           \
                                                                              \
  type ABI sum_many_##nameAndUnderscore##suffix(                              \
      type a, type b, type c, type d, type e, type f, type g, type h, type i, \
      type j, type k, type l, type m, type n, type o, type p, type q,         \
      type r) {                                                               \
    return ValueTraits<type>::sum_many(a, b, c, d, e, f, g, h, i, j, k, l, m, \
                                       n, o, p, q, r);                        \
  }

#define ABI /* cdecl */
#define DEFINE_CDECL_FUNCTIONS(x, y, z) FUNCTION_TESTS(x##_, y, z, cdecl)
CTYPES_FOR_EACH_TYPE(DEFINE_CDECL_FUNCTIONS)
#undef DEFINE_CDECL_FUNCTIONS
#undef ABI

#if defined(_WIN32)

void NS_STDCALL test_void_t_stdcall() {
  // do nothing
  return;
}

#  define ABI NS_STDCALL
#  define DEFINE_STDCALL_FUNCTIONS(x, y, z) FUNCTION_TESTS(x##_, y, z, stdcall)
CTYPES_FOR_EACH_TYPE(DEFINE_STDCALL_FUNCTIONS)
#  undef DEFINE_STDCALL_FUNCTIONS
#  undef ABI

#endif /* defined(_WIN32) */

#define DEFINE_CDECL_TYPE_STATS(name, type, ffiType)                   \
  struct align_##name {                                                \
    char x;                                                            \
    type y;                                                            \
  };                                                                   \
  struct nested_##name {                                               \
    char a;                                                            \
    align_##name b;                                                    \
    char c;                                                            \
  };                                                                   \
                                                                       \
  void get_##name##_stats(size_t* align, size_t* size, size_t* nalign, \
                          size_t* nsize, size_t offsets[]) {           \
    *align = offsetof(align_##name, y);                                \
    *size = sizeof(align_##name);                                      \
    *nalign = offsetof(nested_##name, b);                              \
    *nsize = sizeof(nested_##name);                                    \
    offsets[0] = offsetof(align_##name, y);                            \
    offsets[1] = offsetof(nested_##name, b);                           \
    offsets[2] = offsetof(nested_##name, c);                           \
  }
CTYPES_FOR_EACH_TYPE(DEFINE_CDECL_TYPE_STATS)
#undef DEFINE_CDECL_TYPE_STATS

template <typename T>
int32_t StrLen(const T* string) {
  const T* end;
  for (end = string; *end; ++end)
    ;
  return end - string;
}

int32_t test_ansi_len(const char* string) { return StrLen(string); }

int32_t test_wide_len(const char16_t* string) { return StrLen(string); }

const char* test_ansi_ret() { return "success"; }

const char16_t* test_wide_ret() {
  static const char16_t kSuccess[] = {'s', 'u', 'c', 'c', 'e', 's', 's', '\0'};
  return kSuccess;
}

char* test_ansi_echo(const char* string) { return (char*)string; }

int32_t test_pt_in_rect(myRECT rc, myPOINT pt) {
  if (pt.x < rc.left || pt.x > rc.right) return 0;
  if (pt.y < rc.bottom || pt.y > rc.top) return 0;
  return 1;
}

void test_init_pt(myPOINT* pt, int32_t x, int32_t y) {
  pt->x = x;
  pt->y = y;
}

int32_t test_nested_struct(NESTED n) {
  return int32_t(n.n1 + n.n2 + n.inner.i1 + n.inner.i2 + n.inner.i3 + n.n3 +
                 n.n4);
}

myPOINT test_struct_return(myRECT r) {
  myPOINT p;
  p.x = r.left;
  p.y = r.top;
  return p;
}

myRECT test_large_struct_return(myRECT a, myRECT b) {
  myRECT r;
  r.left = a.left;
  r.right = a.right;
  r.top = b.top;
  r.bottom = b.bottom;
  return r;
}

ONE_BYTE
test_1_byte_struct_return(myRECT r) {
  ONE_BYTE s;
  s.a = r.top;
  return s;
}

TWO_BYTE
test_2_byte_struct_return(myRECT r) {
  TWO_BYTE s;
  s.a = r.top;
  s.b = r.left;
  return s;
}

THREE_BYTE
test_3_byte_struct_return(myRECT r) {
  THREE_BYTE s;
  s.a = r.top;
  s.b = r.left;
  s.c = r.bottom;
  return s;
}

FOUR_BYTE
test_4_byte_struct_return(myRECT r) {
  FOUR_BYTE s;
  s.a = r.top;
  s.b = r.left;
  s.c = r.bottom;
  s.d = r.right;
  return s;
}

FIVE_BYTE
test_5_byte_struct_return(myRECT r) {
  FIVE_BYTE s;
  s.a = r.top;
  s.b = r.left;
  s.c = r.bottom;
  s.d = r.right;
  s.e = r.top;
  return s;
}

SIX_BYTE
test_6_byte_struct_return(myRECT r) {
  SIX_BYTE s;
  s.a = r.top;
  s.b = r.left;
  s.c = r.bottom;
  s.d = r.right;
  s.e = r.top;
  s.f = r.left;
  return s;
}

SEVEN_BYTE
test_7_byte_struct_return(myRECT r) {
  SEVEN_BYTE s;
  s.a = r.top;
  s.b = r.left;
  s.c = r.bottom;
  s.d = r.right;
  s.e = r.top;
  s.f = r.left;
  s.g = r.bottom;
  return s;
}

void* test_fnptr() { return (void*)(uintptr_t)test_ansi_len; }

int32_t test_closure_cdecl(int8_t i, test_func_ptr f) { return f(i); }

#if defined(_WIN32)
int32_t test_closure_stdcall(int8_t i, test_func_ptr_stdcall f) { return f(i); }
#endif /* defined(_WIN32) */

template <typename T>
struct PromotedTraits {
  typedef T type;
};
#define DECL_PROMOTED(FROM, TO) \
  template <>                   \
  struct PromotedTraits<FROM> { \
    typedef TO type;            \
  }
DECL_PROMOTED(bool, int);
DECL_PROMOTED(char, int);
DECL_PROMOTED(short, int);

int32_t test_sum_va_cdecl(uint8_t n, ...) {
  va_list list;
  int32_t sum = 0;
  va_start(list, n);
  for (uint8_t i = 0; i < n; ++i)
    sum += va_arg(list, PromotedTraits<int32_t>::type);
  va_end(list);
  return sum;
}

uint8_t test_count_true_va_cdecl(uint8_t n, ...) {
  va_list list;
  uint8_t count = 0;
  va_start(list, n);
  for (uint8_t i = 0; i < n; ++i)
    if (va_arg(list, PromotedTraits<bool>::type)) count += 1;
  va_end(list);
  return count;
}

void test_add_char_short_int_va_cdecl(uint32_t* result, ...) {
  va_list list;
  va_start(list, result);
  *result += va_arg(list, PromotedTraits<char>::type);
  *result += va_arg(list, PromotedTraits<short>::type);
  *result += va_arg(list, PromotedTraits<int>::type);
  va_end(list);
}

int32_t* test_vector_add_va_cdecl(uint8_t num_vecs, uint8_t vec_len,
                                  int32_t* result, ...) {
  va_list list;
  va_start(list, result);
  uint8_t i;
  for (i = 0; i < vec_len; ++i) result[i] = 0;
  for (i = 0; i < num_vecs; ++i) {
    int32_t* vec = va_arg(list, int32_t*);
    for (uint8_t j = 0; j < vec_len; ++j) result[j] += vec[j];
  }
  va_end(list);
  return result;
}

myRECT data_rect = {-1, -2, 3, 4};

TestClass::TestClass(int32_t a) { mInt = a; }

int32_t TestClass::Add(int32_t aOther) {
  mInt += aOther;
  return mInt;
}

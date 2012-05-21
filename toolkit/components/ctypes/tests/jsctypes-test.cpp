/* -*-  Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jsctypes-test.h"
#include "jsapi.h"
#include "nsCRTGlue.h"
#include <math.h>
#include <stdarg.h>
#include <stdio.h>

#if defined(XP_WIN)
#define snprintf _snprintf
#endif // defined(XP_WIN)

template <typename T> struct ValueTraits {
  static T literal() { return static_cast<T>(109.25); }
  static T sum(T a, T b) { return a + b; }
  static T sum_many(
    T a, T b, T c, T d, T e, T f, T g, T h, T i,
    T j, T k, T l, T m, T n, T o, T p, T q, T r)
  {
    return a + b + c + d + e + f + g + h + i + j + k + l + m + n + o + p + q + r;
  }
};

template <> struct ValueTraits<bool> {
  typedef bool T;
  static T literal() { return true; }
  static T sum(T a, T b) { return a || b; }
  static T sum_many(
    T a, T b, T c, T d, T e, T f, T g, T h, T i,
    T j, T k, T l, T m, T n, T o, T p, T q, T r)
  {
    return a || b || c || d || e || f || g || h || i ||
           j || k || l || m || n || o || p || q || r;
  }
};

void
test_void_t_cdecl()
{
  // do nothing
  return;
}

#define FUNCTION_TESTS(name, type, ffiType, suffix)                            \
type ABI                                                                       \
get_##name##_##suffix()                                                        \
{                                                                              \
  return ValueTraits<type>::literal();                                         \
}                                                                              \
                                                                               \
type ABI                                                                       \
set_##name##_##suffix(type x)                                                  \
{                                                                              \
  return x;                                                                    \
}                                                                              \
                                                                               \
type ABI                                                                       \
sum_##name##_##suffix(type x, type y)                                          \
{                                                                              \
  return ValueTraits<type>::sum(x, y);                                         \
}                                                                              \
                                                                               \
type ABI                                                                       \
sum_alignb_##name##_##suffix(char a, type x, char b, type y, char c)           \
{                                                                              \
  return ValueTraits<type>::sum(x, y);                                         \
}                                                                              \
                                                                               \
type ABI                                                                       \
sum_alignf_##name##_##suffix(float a, type x, float b, type y, float c)        \
{                                                                              \
  return ValueTraits<type>::sum(x, y);                                         \
}                                                                              \
                                                                               \
type ABI                                                                       \
sum_many_##name##_##suffix(                                                    \
  type a, type b, type c, type d, type e, type f, type g, type h, type i,      \
  type j, type k, type l, type m, type n, type o, type p, type q, type r)      \
{                                                                              \
  return ValueTraits<type>::sum_many(a, b, c, d, e, f, g, h, i,                \
                                     j, k, l, m, n, o, p, q, r);               \
}

#define ABI /* cdecl */
#define DEFINE_TYPE(x, y, z) FUNCTION_TESTS(x, y, z, cdecl)
#include "typedefs.h"
#undef ABI

#if defined(_WIN32)

void NS_STDCALL
test_void_t_stdcall()
{
  // do nothing
  return;
}

#define ABI NS_STDCALL
#define DEFINE_TYPE(x, y, z) FUNCTION_TESTS(x, y, z, stdcall)
#include "typedefs.h"
#undef ABI

#endif /* defined(_WIN32) */

#define DEFINE_TYPE(name, type, ffiType)                                       \
struct align_##name {                                                          \
  char x;                                                                      \
  type y;                                                                      \
};                                                                             \
struct nested_##name {                                                         \
  char a;                                                                      \
  align_##name b;                                                              \
  char c;                                                                      \
};                                                                             \
                                                                               \
void                                                                           \
get_##name##_stats(size_t* align, size_t* size, size_t* nalign, size_t* nsize, \
                   size_t offsets[])                                           \
{                                                                              \
  *align = offsetof(align_##name, y);                                          \
  *size = sizeof(align_##name);                                                \
  *nalign = offsetof(nested_##name, b);                                        \
  *nsize = sizeof(nested_##name);                                              \
  offsets[0] = offsetof(align_##name, y);                                      \
  offsets[1] = offsetof(nested_##name, b);                                     \
  offsets[2] = offsetof(nested_##name, c);                                     \
}
#include "typedefs.h"

template <typename T>
PRInt32 StrLen(const T* string)
{
  const T *end;
  for (end = string; *end; ++end);
  return end - string;
}

PRInt32
test_ansi_len(const char* string)
{
  return StrLen(string);
}

PRInt32
test_wide_len(const PRUnichar* string)
{
  return StrLen(string);
}

const char *
test_ansi_ret()
{
  return "success";
}

const PRUnichar *
test_wide_ret()
{
  static const PRUnichar kSuccess[] = {'s', 'u', 'c', 'c', 'e', 's', 's', '\0'};
  return kSuccess;
}

char *
test_ansi_echo(const char* string)
{
  return (char*)string;
}

PRInt32
test_pt_in_rect(RECT rc, POINT pt)
{
  if (pt.x < rc.left || pt.x > rc.right)
    return 0;
  if (pt.y < rc.bottom || pt.y > rc.top)
    return 0;
  return 1;
}

void
test_init_pt(POINT* pt, PRInt32 x, PRInt32 y)
{
  pt->x = x;
  pt->y = y;
}

PRInt32
test_nested_struct(NESTED n)
{
  return PRInt32(n.n1 + n.n2 + n.inner.i1 + n.inner.i2 + n.inner.i3 + n.n3 + n.n4);
}

POINT
test_struct_return(RECT r)
{
  POINT p;
  p.x = r.left; p.y = r.top;
  return p;
}

RECT
test_large_struct_return(RECT a, RECT b)
{
  RECT r;
  r.left = a.left; r.right = a.right;
  r.top = b.top; r.bottom = b.bottom;
  return r;
}

ONE_BYTE
test_1_byte_struct_return(RECT r)
{
  ONE_BYTE s;
  s.a = r.top;
  return s;
}

TWO_BYTE
test_2_byte_struct_return(RECT r)
{
  TWO_BYTE s;
  s.a = r.top;
  s.b = r.left;
  return s;
}

THREE_BYTE
test_3_byte_struct_return(RECT r)
{
  THREE_BYTE s;
  s.a = r.top;
  s.b = r.left;
  s.c = r.bottom;
  return s;
}

FOUR_BYTE
test_4_byte_struct_return(RECT r)
{
  FOUR_BYTE s;
  s.a = r.top;
  s.b = r.left;
  s.c = r.bottom;
  s.d = r.right;
  return s;
}

FIVE_BYTE
test_5_byte_struct_return(RECT r)
{
  FIVE_BYTE s;
  s.a = r.top;
  s.b = r.left;
  s.c = r.bottom;
  s.d = r.right;
  s.e = r.top;
  return s;
}

SIX_BYTE
test_6_byte_struct_return(RECT r)
{
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
test_7_byte_struct_return(RECT r)
{
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

void *
test_fnptr()
{
  return (void*)(uintptr_t)test_ansi_len;
}

PRInt32
test_closure_cdecl(PRInt8 i, test_func_ptr f)
{
  return f(i);
}

#if defined(_WIN32)
PRInt32
test_closure_stdcall(PRInt8 i, test_func_ptr_stdcall f)
{
  return f(i);
}
#endif /* defined(_WIN32) */

template <typename T> struct PromotedTraits {
  typedef T type;
};
#define DECL_PROMOTED(FROM, TO)                 \
  template <> struct PromotedTraits<FROM> {     \
    typedef TO type;                            \
  }
DECL_PROMOTED(bool, int);
DECL_PROMOTED(char, int);
DECL_PROMOTED(short, int);

PRInt32
test_sum_va_cdecl(PRUint8 n, ...)
{
  va_list list;
  PRInt32 sum = 0;
  va_start(list, n);
  for (PRUint8 i = 0; i < n; ++i)
    sum += va_arg(list, PromotedTraits<PRInt32>::type);
  va_end(list);
  return sum;
}

PRUint8
test_count_true_va_cdecl(PRUint8 n, ...)
{
  va_list list;
  PRUint8 count = 0;
  va_start(list, n);
  for (PRUint8 i = 0; i < n; ++i)
    if (va_arg(list, PromotedTraits<bool>::type))
      count += 1;
  va_end(list);
  return count;
}

void
test_add_char_short_int_va_cdecl(PRUint32* result, ...)
{
  va_list list;
  va_start(list, result);
  *result += va_arg(list, PromotedTraits<char>::type);
  *result += va_arg(list, PromotedTraits<short>::type);
  *result += va_arg(list, PromotedTraits<int>::type);
  va_end(list);
}

PRInt32*
test_vector_add_va_cdecl(PRUint8 num_vecs,
                         PRUint8 vec_len,
                         PRInt32* result, ...)
{
  va_list list;
  va_start(list, result);
  PRUint8 i;
  for (i = 0; i < vec_len; ++i)
    result[i] = 0;
  for (i = 0; i < num_vecs; ++i) {
    PRInt32* vec = va_arg(list, PRInt32*);
    for (PRUint8 j = 0; j < vec_len; ++j)
      result[j] += vec[j];
  }
  va_end(list);
  return result;
}

RECT data_rect = { -1, -2, 3, 4 };

/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jsctypes_test_h
#define jsctypes_test_h

#include "mozilla/Attributes.h"
#include "mozilla/Types.h"
#include "jspubtd.h"
#include "typedefs.h"

#include <sys/types.h>

#define EXPORT_CDECL(type) MOZ_EXPORT type
#if defined(_WIN32)
#  if defined(_WIN64)
#    define NS_STDCALL
#  else
#    define NS_STDCALL __stdcall
#  endif
#  define EXPORT_STDCALL(type) MOZ_EXPORT type NS_STDCALL
#endif

MOZ_BEGIN_EXTERN_C

EXPORT_CDECL(void) test_void_t_cdecl();

EXPORT_CDECL(void*) get_voidptr_t_cdecl();
EXPORT_CDECL(void*) set_voidptr_t_cdecl(void*);

#define DECLARE_CDECL_FUNCTIONS(name, type, ffiType)                          \
  EXPORT_CDECL(type) get_##name##_cdecl();                                    \
  EXPORT_CDECL(type) set_##name##_cdecl(type);                                \
  EXPORT_CDECL(type) sum_##name##_cdecl(type, type);                          \
  EXPORT_CDECL(type) sum_alignb_##name##_cdecl(char, type, char, type, char); \
  EXPORT_CDECL(type)                                                          \
  sum_alignf_##name##_cdecl(float, type, float, type, float);                 \
  EXPORT_CDECL(type)                                                          \
  sum_many_##name##_cdecl(type, type, type, type, type, type, type, type,     \
                          type, type, type, type, type, type, type, type,     \
                          type, type);                                        \
                                                                              \
  EXPORT_CDECL(void)                                                          \
  get_##name##_stats(size_t* align, size_t* size, size_t* nalign,             \
                     size_t* nsize, size_t offsets[]);
CTYPES_FOR_EACH_TYPE(DECLARE_CDECL_FUNCTIONS)
#undef DECLARE_CDECL_FUNCTIONS

#if defined(_WIN32)
EXPORT_STDCALL(void) test_void_t_stdcall();

EXPORT_STDCALL(void*) get_voidptr_t_stdcall();
EXPORT_STDCALL(void*) set_voidptr_t_stdcall(void*);

#  define DECLARE_STDCALL_FUNCTIONS(name, type, ffiType)                      \
    EXPORT_STDCALL(type) get_##name##_stdcall();                              \
    EXPORT_STDCALL(type) set_##name##_stdcall(type);                          \
    EXPORT_STDCALL(type) sum_##name##_stdcall(type, type);                    \
    EXPORT_STDCALL(type)                                                      \
    sum_alignb_##name##_stdcall(char, type, char, type, char);                \
    EXPORT_STDCALL(type)                                                      \
    sum_alignf_##name##_stdcall(float, type, float, type, float);             \
    EXPORT_STDCALL(type)                                                      \
    sum_many_##name##_stdcall(type, type, type, type, type, type, type, type, \
                              type, type, type, type, type, type, type, type, \
                              type, type);
CTYPES_FOR_EACH_TYPE(DECLARE_STDCALL_FUNCTIONS)
#  undef DECLARE_STDCALL_FUNCTIONS

#endif /* defined(_WIN32) */

MOZ_EXPORT int32_t test_ansi_len(const char*);
MOZ_EXPORT int32_t test_wide_len(const char16_t*);
MOZ_EXPORT const char* test_ansi_ret();
MOZ_EXPORT const char16_t* test_wide_ret();
MOZ_EXPORT char* test_ansi_echo(const char*);

struct ONE_BYTE {
  char a;
};

struct TWO_BYTE {
  char a;
  char b;
};

struct THREE_BYTE {
  char a;
  char b;
  char c;
};

struct FOUR_BYTE {
  char a;
  char b;
  char c;
  char d;
};

struct FIVE_BYTE {
  char a;
  char b;
  char c;
  char d;
  char e;
};

struct SIX_BYTE {
  char a;
  char b;
  char c;
  char d;
  char e;
  char f;
};

struct SEVEN_BYTE {
  char a;
  char b;
  char c;
  char d;
  char e;
  char f;
  char g;
};

struct myPOINT {
  int32_t x;
  int32_t y;
};

struct myRECT {
  int32_t top;
  int32_t left;
  int32_t bottom;
  int32_t right;
};

struct INNER {
  uint8_t i1;
  int64_t i2;
  uint8_t i3;
};

struct NESTED {
  int32_t n1;
  int16_t n2;
  INNER inner;
  int64_t n3;
  int32_t n4;
};

MOZ_EXPORT int32_t test_pt_in_rect(myRECT, myPOINT);
MOZ_EXPORT void test_init_pt(myPOINT* pt, int32_t x, int32_t y);

MOZ_EXPORT int32_t test_nested_struct(NESTED);
MOZ_EXPORT myPOINT test_struct_return(myRECT);
MOZ_EXPORT myRECT test_large_struct_return(myRECT, myRECT);
MOZ_EXPORT ONE_BYTE test_1_byte_struct_return(myRECT);
MOZ_EXPORT TWO_BYTE test_2_byte_struct_return(myRECT);
MOZ_EXPORT THREE_BYTE test_3_byte_struct_return(myRECT);
MOZ_EXPORT FOUR_BYTE test_4_byte_struct_return(myRECT);
MOZ_EXPORT FIVE_BYTE test_5_byte_struct_return(myRECT);
MOZ_EXPORT SIX_BYTE test_6_byte_struct_return(myRECT);
MOZ_EXPORT SEVEN_BYTE test_7_byte_struct_return(myRECT);

MOZ_EXPORT void* test_fnptr();

typedef int32_t (*test_func_ptr)(int8_t);
MOZ_EXPORT int32_t test_closure_cdecl(int8_t, test_func_ptr);
#if defined(_WIN32)
typedef int32_t(NS_STDCALL* test_func_ptr_stdcall)(int8_t);
MOZ_EXPORT int32_t test_closure_stdcall(int8_t, test_func_ptr_stdcall);
#endif /* defined(_WIN32) */

MOZ_EXPORT int32_t test_callme(int8_t);
MOZ_EXPORT void* test_getfn();

EXPORT_CDECL(int32_t) test_sum_va_cdecl(uint8_t n, ...);
EXPORT_CDECL(uint8_t) test_count_true_va_cdecl(uint8_t n, ...);
EXPORT_CDECL(void) test_add_char_short_int_va_cdecl(uint32_t* result, ...);
EXPORT_CDECL(int32_t*)
test_vector_add_va_cdecl(uint8_t num_vecs, uint8_t vec_len, int32_t* result,
                         ...);

MOZ_EXPORT extern myRECT data_rect;

MOZ_END_EXTERN_C

class MOZ_EXPORT TestClass final {
 public:
  explicit TestClass(int32_t);
  int32_t Add(int32_t);

 private:
  int32_t mInt;
};

#endif

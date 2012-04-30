/* -*-  Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is js-ctypes.
 *
 * The Initial Developer of the Original Code is
 * The Mozilla Foundation <http://www.mozilla.org/>.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Fredrik Larsson <nossralf@gmail.com>
 *  Mark Finkle <mark.finkle@gmail.com>, <mfinkle@mozilla.com>
 *  Dan Witte <dwitte@mozilla.com>
 *  David Rajchenbach-Teller <dteller@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nscore.h"
#include "prtypes.h"
#include "jsapi.h"

#define EXPORT_CDECL(type)   NS_EXPORT type
#define EXPORT_STDCALL(type) NS_EXPORT type NS_STDCALL

NS_EXTERN_C
{
  EXPORT_CDECL(void) test_void_t_cdecl();

  EXPORT_CDECL(void*) get_voidptr_t_cdecl();
  EXPORT_CDECL(void*) set_voidptr_t_cdecl(void*);

#define DEFINE_TYPE(name, type, ffiType)                                       \
  EXPORT_CDECL(type) get_##name##_cdecl();                                     \
  EXPORT_CDECL(type) set_##name##_cdecl(type);                                 \
  EXPORT_CDECL(type) sum_##name##_cdecl(type, type);                           \
  EXPORT_CDECL(type) sum_alignb_##name##_cdecl(char, type, char, type, char);  \
  EXPORT_CDECL(type) sum_alignf_##name##_cdecl(                                \
    float, type, float, type, float);                                          \
  EXPORT_CDECL(type) sum_many_##name##_cdecl(                                  \
    type, type, type, type, type, type, type, type, type,                      \
    type, type, type, type, type, type, type, type, type);                     \
                                                                               \
  EXPORT_CDECL(void) get_##name##_stats(size_t* align, size_t* size,           \
                                        size_t* nalign, size_t* nsize,         \
                                        size_t offsets[]);

#include "typedefs.h"

#if defined(_WIN32)
  EXPORT_STDCALL(void) test_void_t_stdcall();

  EXPORT_STDCALL(void*) get_voidptr_t_stdcall();
  EXPORT_STDCALL(void*) set_voidptr_t_stdcall(void*);

#define DEFINE_TYPE(name, type, ffiType)                                       \
  EXPORT_STDCALL(type) get_##name##_stdcall();                                 \
  EXPORT_STDCALL(type) set_##name##_stdcall(type);                             \
  EXPORT_STDCALL(type) sum_##name##_stdcall(type, type);                       \
  EXPORT_STDCALL(type) sum_alignb_##name##_stdcall(                            \
    char, type, char, type, char);                                             \
  EXPORT_STDCALL(type) sum_alignf_##name##_stdcall(                            \
    float, type, float, type, float);                                          \
  EXPORT_STDCALL(type) sum_many_##name##_stdcall(                              \
    type, type, type, type, type, type, type, type, type,                      \
    type, type, type, type, type, type, type, type, type);

#include "typedefs.h"

#endif /* defined(_WIN32) */

  NS_EXPORT PRInt32 test_ansi_len(const char*);
  NS_EXPORT PRInt32 test_wide_len(const PRUnichar*);
  NS_EXPORT const char* test_ansi_ret();
  NS_EXPORT const PRUnichar* test_wide_ret();
  NS_EXPORT char* test_ansi_echo(const char*);

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

  struct POINT {
    PRInt32 x;
    PRInt32 y;
  };

  struct RECT {
    PRInt32 top;
    PRInt32 left;
    PRInt32 bottom;
    PRInt32 right;
  };

  struct INNER {
    PRUint8 i1;
    PRInt64 i2;
    PRUint8 i3;
  };

  struct NESTED {
    PRInt32 n1;
    PRInt16 n2;
    INNER   inner;
    PRInt64 n3;
    PRInt32 n4;
  };

  NS_EXPORT PRInt32 test_pt_in_rect(RECT, POINT);
  NS_EXPORT void test_init_pt(POINT* pt, PRInt32 x, PRInt32 y);

  NS_EXPORT PRInt32 test_nested_struct(NESTED);
  NS_EXPORT POINT test_struct_return(RECT);
  NS_EXPORT RECT test_large_struct_return(RECT, RECT);
  NS_EXPORT ONE_BYTE test_1_byte_struct_return(RECT);
  NS_EXPORT TWO_BYTE test_2_byte_struct_return(RECT);
  NS_EXPORT THREE_BYTE test_3_byte_struct_return(RECT);
  NS_EXPORT FOUR_BYTE test_4_byte_struct_return(RECT);
  NS_EXPORT FIVE_BYTE test_5_byte_struct_return(RECT);
  NS_EXPORT SIX_BYTE test_6_byte_struct_return(RECT);
  NS_EXPORT SEVEN_BYTE test_7_byte_struct_return(RECT);

  NS_EXPORT void * test_fnptr();

  typedef PRInt32 (* test_func_ptr)(PRInt8);
  NS_EXPORT PRInt32 test_closure_cdecl(PRInt8, test_func_ptr);
#if defined(_WIN32)
  typedef PRInt32 (NS_STDCALL * test_func_ptr_stdcall)(PRInt8);
  NS_EXPORT PRInt32 test_closure_stdcall(PRInt8, test_func_ptr_stdcall);
#endif /* defined(_WIN32) */

  NS_EXPORT PRInt32 test_callme(PRInt8);
  NS_EXPORT void* test_getfn();

  EXPORT_CDECL(PRInt32) test_sum_va_cdecl(PRUint8 n, ...);
  EXPORT_CDECL(PRUint8) test_count_true_va_cdecl(PRUint8 n, ...);
  EXPORT_CDECL(void) test_add_char_short_int_va_cdecl(PRUint32* result, ...);
  EXPORT_CDECL(PRInt32*) test_vector_add_va_cdecl(PRUint8 num_vecs,
                                                  PRUint8 vec_len,
                                                  PRInt32* result, ...);

  NS_EXPORT extern RECT data_rect;
}

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
 * The Original Code is a SPARC/VIS optimized multiply and add function.
 *
 * The Initial Developer of the Original Code is
 * Sun Microsystems Inc.
 * Portions created by the Initial Developer are Copyright (C) 1999-2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#include "vis_proto.h"

/***************************************************************/

typedef  int                t_s32;
typedef  unsigned int       t_u32;
#if defined(__sparcv9)
typedef  long               t_s64;
typedef  unsigned long      t_u64;
#else
typedef  long long          t_s64;
typedef  unsigned long long t_u64;
#endif
typedef  double             t_d64;

/***************************************************************/

typedef union {
  t_d64 d64;
  struct {
    t_s32 i0;
    t_s32 i1;
  } i32s;
} d64_2_i32;

/***************************************************************/

#define BUFF_SIZE  256

#define A_BITS  19
#define A_MASK  ((1 << A_BITS) - 1)

/***************************************************************/

static t_u64 mask_cnst[] = {
  0x8000000080000000ull
};

/***************************************************************/

#define DEF_VARS(N)                     \
  t_d64 *py = (t_d64*)y;                \
  t_d64 mask = *((t_d64*)mask_cnst);    \
  t_d64 ca = (1u << 31) - 1;            \
  t_d64 da = (t_d64)a;                  \
  t_s64 buff[N], s;                     \
  d64_2_i32 dy

/***************************************************************/

#define MUL_U32_S64_2(i)                                \
  dy.d64 = vis_fxnor(mask, py[i]);                      \
  buff[2*(i)  ] = (ca - (t_d64)dy.i32s.i0) * da;        \
  buff[2*(i)+1] = (ca - (t_d64)dy.i32s.i1) * da

#define MUL_U32_S64_2_D(i)              \
  dy.d64 = vis_fxnor(mask, py[i]);      \
  d0 = ca - (t_d64)dy.i32s.i0;          \
  d1 = ca - (t_d64)dy.i32s.i1;          \
  buff[4*(i)  ] = (t_s64)(d0 * da);     \
  buff[4*(i)+1] = (t_s64)(d0 * db);     \
  buff[4*(i)+2] = (t_s64)(d1 * da);     \
  buff[4*(i)+3] = (t_s64)(d1 * db)

/***************************************************************/

#define ADD_S64_U32(i)          \
  s = buff[i] + x[i] + c;       \
  z[i] = s;                     \
  c = (s >> 32)

#define ADD_S64_U32_D(i)                        \
  s = buff[2*(i)] +(((t_s64)(buff[2*(i)+1]))<<A_BITS) + x[i] + uc;   \
  z[i] = s;                                     \
  uc = ((t_u64)s >> 32)

/***************************************************************/

#define MUL_U32_S64_8(i)        \
  MUL_U32_S64_2(i);             \
  MUL_U32_S64_2(i+1);           \
  MUL_U32_S64_2(i+2);           \
  MUL_U32_S64_2(i+3)

#define MUL_U32_S64_D_8(i)      \
  MUL_U32_S64_2_D(i);           \
  MUL_U32_S64_2_D(i+1);         \
  MUL_U32_S64_2_D(i+2);         \
  MUL_U32_S64_2_D(i+3)

/***************************************************************/

#define ADD_S64_U32_8(i)        \
  ADD_S64_U32(i);               \
  ADD_S64_U32(i+1);             \
  ADD_S64_U32(i+2);             \
  ADD_S64_U32(i+3);             \
  ADD_S64_U32(i+4);             \
  ADD_S64_U32(i+5);             \
  ADD_S64_U32(i+6);             \
  ADD_S64_U32(i+7)

#define ADD_S64_U32_D_8(i)      \
  ADD_S64_U32_D(i);             \
  ADD_S64_U32_D(i+1);           \
  ADD_S64_U32_D(i+2);           \
  ADD_S64_U32_D(i+3);           \
  ADD_S64_U32_D(i+4);           \
  ADD_S64_U32_D(i+5);           \
  ADD_S64_U32_D(i+6);           \
  ADD_S64_U32_D(i+7)

/***************************************************************/

t_u32 mul_add(t_u32 *z, t_u32 *x, t_u32 *y, int n, t_u32 a)
{
  if (a < (1 << A_BITS)) {

    if (n == 8) {
      DEF_VARS(8);
      t_s32 c = 0;

      MUL_U32_S64_8(0);
      ADD_S64_U32_8(0);

      return c;

    } else if (n == 16) {
      DEF_VARS(16);
      t_s32 c = 0;

      MUL_U32_S64_8(0);
      MUL_U32_S64_8(4);
      ADD_S64_U32_8(0);
      ADD_S64_U32_8(8);

      return c;

    } else {
      DEF_VARS(BUFF_SIZE);
      t_s32 i, c = 0;

#pragma pipeloop(0)
      for (i = 0; i < (n+1)/2; i ++) {
        MUL_U32_S64_2(i);
      }

#pragma pipeloop(0)
      for (i = 0; i < n; i ++) {
        ADD_S64_U32(i);
      }

      return c;

    }
  } else {

    if (n == 8) {
      DEF_VARS(2*8);
      t_d64 d0, d1, db;
      t_u32 uc = 0;

      da = (t_d64)(a &  A_MASK);
      db = (t_d64)(a >> A_BITS);

      MUL_U32_S64_D_8(0);
      ADD_S64_U32_D_8(0);

      return uc;

    } else if (n == 16) {
      DEF_VARS(2*16);
      t_d64 d0, d1, db;
      t_u32 uc = 0;

      da = (t_d64)(a &  A_MASK);
      db = (t_d64)(a >> A_BITS);

      MUL_U32_S64_D_8(0);
      MUL_U32_S64_D_8(4);
      ADD_S64_U32_D_8(0);
      ADD_S64_U32_D_8(8);

      return uc;

    } else {
      DEF_VARS(2*BUFF_SIZE);
      t_d64 d0, d1, db;
      t_u32 i, uc = 0;

      da = (t_d64)(a &  A_MASK);
      db = (t_d64)(a >> A_BITS);

#pragma pipeloop(0)
      for (i = 0; i < (n+1)/2; i ++) {
        MUL_U32_S64_2_D(i);
      }

#pragma pipeloop(0)
      for (i = 0; i < n; i ++) {
        ADD_S64_U32_D(i);
      }

      return uc;
    }
  }
}

/***************************************************************/

t_u32 mul_add_inp(t_u32 *x, t_u32 *y, int n, t_u32 a)
{
  return mul_add(x, x, y, n, a);
}

/***************************************************************/

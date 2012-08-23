
/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "xptcprivate.h"

#include <stdint.h>

// "This code is for IA64 only"


/* invoke_copy_to_stack() will copy from variant array 's' to
 * the stack argument area 'mloc', the integer register area 'iloc', and
 * the float register area 'floc'.
 *
 */
extern "C" void
invoke_copy_to_stack(uint64_t* mloc, uint64_t* iloc, uint64_t* floc,
  const uint32_t paramCount, nsXPTCVariant* s)
{
  uint64_t* dest = mloc;
  uint32_t len = paramCount;
  nsXPTCVariant* source = s;

  uint32_t indx;
  uint32_t endlen;
  endlen = (len > 7) ? 7 : len;
  /* handle the memory arguments */
  for (indx = 7; indx < len; ++indx)
  {
    if (source[indx].IsPtrData())
    {
      /* 64 bit pointer mode */
      *((void**) dest) = source[indx].ptr;
    }
    else
    switch (source[indx].type)
    {
    case nsXPTType::T_I8    : *(dest) = source[indx].val.i8;  break;
    case nsXPTType::T_I16   : *(dest) = source[indx].val.i16; break;
    case nsXPTType::T_I32   : *(dest) = source[indx].val.i32; break;
    case nsXPTType::T_I64   : *(dest) = source[indx].val.i64; break;
    case nsXPTType::T_U8    : *(dest) = source[indx].val.u8;  break;
    case nsXPTType::T_U16   : *(dest) = source[indx].val.u16; break;
    case nsXPTType::T_U32   : *(dest) = source[indx].val.u32; break;
    case nsXPTType::T_U64   : *(dest) = source[indx].val.u64; break;
    case nsXPTType::T_FLOAT : *(dest) = source[indx].val.u32; break;
    case nsXPTType::T_DOUBLE: *(dest) = source[indx].val.u64; break;
    case nsXPTType::T_BOOL  : *(dest) = source[indx].val.b; break;
    case nsXPTType::T_CHAR  : *(dest) = source[indx].val.c; break;
    case nsXPTType::T_WCHAR : *(dest) = source[indx].val.wc; break;
    default:
    // all the others are plain pointer types
      /* 64 bit pointer mode */
      *((void**) dest) = source[indx].val.p;
    }
    ++dest;
  }
  /* process register arguments */
  dest = iloc;
  for (indx = 0; indx < endlen; ++indx)
  {
    if (source[indx].IsPtrData())
    {
      /* 64 bit pointer mode */
      *((void**) dest) = source[indx].ptr;
    }
    else
    switch (source[indx].type)
    {
    case nsXPTType::T_I8    : *(dest) = source[indx].val.i8; break;
    case nsXPTType::T_I16   : *(dest) = source[indx].val.i16; break;
    case nsXPTType::T_I32   : *(dest) = source[indx].val.i32; break;
    case nsXPTType::T_I64   : *(dest) = source[indx].val.i64; break;
    case nsXPTType::T_U8    : *(dest) = source[indx].val.u8; break;
    case nsXPTType::T_U16   : *(dest) = source[indx].val.u16; break;
    case nsXPTType::T_U32   : *(dest) = source[indx].val.u32; break;
    case nsXPTType::T_U64   : *(dest) = source[indx].val.u64; break;
    case nsXPTType::T_FLOAT :
      *((double*) (floc++)) = (double) source[indx].val.f;
      break;
    case nsXPTType::T_DOUBLE:
      *((double*) (floc++)) = source[indx].val.d;
      break;
    case nsXPTType::T_BOOL  : *(dest) = source[indx].val.b; break;
    case nsXPTType::T_CHAR  : *(dest) = source[indx].val.c; break;
    case nsXPTType::T_WCHAR : *(dest) = source[indx].val.wc; break;
    default:
    // all the others are plain pointer types
      /* 64 bit pointer mode */
      *((void**) dest) = source[indx].val.p;
    }
    ++dest;
  }

}



/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "xptcprivate.h"

#include <iostream.h>

// "This code is for IA64 only"


/* invoke_copy_to_stack() will copy from variant array 's' to
 * the stack argument area 'mloc', the integer register area 'iloc', and
 * the float register area 'floc'.
 *
 */
extern "C" void
invoke_copy_to_stack(uint64_t* mloc, uint64_t* iloc, uint64_t* floc,
  const PRUint32 paramCount, nsXPTCVariant* s)
{
  uint64_t* dest = mloc;
  PRUint32 len = paramCount;
  nsXPTCVariant* source = s;

  PRUint32 indx;
  PRUint32 endlen;
  endlen = (len > 7) ? 7 : len;
  /* handle the memory arguments */
  for (indx = 7; indx < len; ++indx)
  {
    if (source[indx].IsPtrData())
    {
#ifdef __LP64__
      /* 64 bit pointer mode */
      *((void**) dest) = source[indx].ptr;
#else
      /* 32 bit pointer mode */
      uint32_t* adr = (uint32_t*) dest;
      *(adr) = 0;
      *(adr+1) = (uint32_t) source[indx].ptr;
#endif
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
#ifdef __LP64__
      /* 64 bit pointer mode */
      *((void**) dest) = source[indx].val.p;
#else
      {
      /* 32 bit pointer mode */
      uint32_t* adr = (uint32_t*) dest;
      *(adr) = 0;
      *(adr+1) = (uint32_t) source[indx].val.p;
      }
#endif
    }
    ++dest;
  }
  /* process register arguments */
  dest = iloc;
  for (indx = 0; indx < endlen; ++indx)
  {
    if (source[indx].IsPtrData())
    {
#ifdef __LP64__
      /* 64 bit pointer mode */
      *((void**) dest) = source[indx].ptr;
#else
      /* 32 bit pointer mode */
      uint32_t* adr = (uint32_t*) dest;
      *(adr) = 0;
      *(adr+1) = (uint32_t) source[indx].ptr;
#endif
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
#ifdef __LP64__
      /* 64 bit pointer mode */
      *((void**) dest) = source[indx].val.p;
#else
      {
      /* 32 bit pointer mode */
      uint32_t* adr = (uint32_t*) dest;
      *(adr) = 0;
      *(adr+1) = (uint32_t) source[indx].val.p;
      }
#endif
    }
    ++dest;
  }

}


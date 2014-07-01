/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/* Platform specific code to invoke XPCOM methods on native objects */

#include "xptcprivate.h"

#if !defined(__sparc) && !defined(__sparc__)
#error "This code is for Sparc only"
#endif

/* Prototype specifies unmangled function name */
extern "C" uint64_t
invoke_copy_to_stack(uint64_t* d, uint32_t paramCount, nsXPTCVariant* s);

extern "C" uint64_t
invoke_copy_to_stack(uint64_t* d, uint32_t paramCount, nsXPTCVariant* s)
{
  /*
    We need to copy the parameters for this function to locals and use them
    from there since the parameters occupy the same stack space as the stack
    we're trying to populate.
  */
  uint64_t *l_d = d;
  nsXPTCVariant *l_s = s;
  uint64_t l_paramCount = paramCount;
  uint64_t regCount = 0;  // return the number of registers to load from the stack

  for(uint64_t i = 0; i < l_paramCount; i++, l_d++, l_s++)
  {
    if (regCount < 5) regCount++;

    if (l_s->IsPtrData())
    {
      *l_d = (uint64_t)l_s->ptr;
      continue;
    }
    switch (l_s->type)
    {
      case nsXPTType::T_I8    : *((int64_t*)l_d)     = l_s->val.i8;    break;
      case nsXPTType::T_I16   : *((int64_t*)l_d)     = l_s->val.i16;   break;
      case nsXPTType::T_I32   : *((int64_t*)l_d)     = l_s->val.i32;   break;
      case nsXPTType::T_I64   : *((int64_t*)l_d)     = l_s->val.i64;   break;
      
      case nsXPTType::T_U8    : *((uint64_t*)l_d)    = l_s->val.u8;    break;
      case nsXPTType::T_U16   : *((uint64_t*)l_d)    = l_s->val.u16;   break;
      case nsXPTType::T_U32   : *((uint64_t*)l_d)    = l_s->val.u32;   break;
      case nsXPTType::T_U64   : *((uint64_t*)l_d)    = l_s->val.u64;   break;

      /* in the case of floats, we want to put the bits in to the
         64bit space right justified... floats in the parameter array on
         sparcv9 use odd numbered registers.. %f1, %f3, so we have to skip
         the space that would be occupied by %f0, %f2, etc.
      */
      case nsXPTType::T_FLOAT : *(((float*)l_d) + 1) = l_s->val.f;     break;
      case nsXPTType::T_DOUBLE: *((double*)l_d)      = l_s->val.d;     break;
      case nsXPTType::T_BOOL  : *((uint64_t*)l_d)    = l_s->val.b;     break;
      case nsXPTType::T_CHAR  : *((uint64_t*)l_d)    = l_s->val.c;     break;
      case nsXPTType::T_WCHAR : *((int64_t*)l_d)     = l_s->val.wc;    break;

      default:
        // all the others are plain pointer types
        *((void**)l_d) = l_s->val.p;
        break;
    }
  }
  
  return regCount;
}

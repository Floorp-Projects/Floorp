
/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "xptcprivate.h"

#if _HPUX
#error "This code is for HP-PA RISC 32 bit mode only"
#endif

#include <alloca.h>

typedef unsigned nsXPCVariant;

extern "C" int32_t
invoke_count_bytes(nsISupports* that, const uint32_t methodIndex,
  const uint32_t paramCount, const nsXPTCVariant* s)
{
  int32_t result = 4; /* variant records do not include self pointer */

  /* counts the number of bytes required by the argument stack,
     64 bit integer, and double requires 8 bytes.  All else requires
     4 bytes.
   */

  {
    uint32_t indx;
    for (indx = paramCount; indx > 0; --indx, ++s)
    {
      if (! s->IsPtrData())
      {
        if (s->type == nsXPTType::T_I64 || s->type == nsXPTType::T_U64 ||
            s->type == nsXPTType::T_DOUBLE)
        {
          /* 64 bit integer and double aligned on 8 byte boundaries */
          result += (result & 4) + 8;
          continue;
        }
      }
      result += 4; /* all other cases use 4 bytes */
    }
  }
  result -= 72; /* existing stack buffer is 72 bytes */
  if (result < 0)
    return 0;
  {
    /* round up to 64 bytes boundary */
    int32_t remainder = result & 63;
    return (remainder == 0) ? result : (result + 64 - remainder);
  }
}

extern "C" uint32_t
invoke_copy_to_stack(uint32_t* d,
  const uint32_t paramCount, nsXPTCVariant* s)
{

  typedef struct
  {
    uint32_t hi;
    uint32_t lo;
  } DU;

  uint32_t* dest = d;
  nsXPTCVariant* source = s;
  /* we clobber param vars by copying stuff on stack, have to use local var */

  uint32_t floatflags = 0;
  /* flag indicating which floating point registers to load */

  uint32_t regwords = 1; /* register 26 is reserved for ptr to 'that' */
  uint32_t indx;

  for (indx = paramCount; indx > 0; --indx, --dest, ++source)
  {
    if (source->IsPtrData())
    {
      *((void**) dest) = source->ptr;
      ++regwords;
      continue;
    }
    switch (source->type)
    {
    case nsXPTType::T_I8    : *((int32_t*) dest) = source->val.i8;  break;
    case nsXPTType::T_I16   : *((int32_t*) dest) = source->val.i16; break;
    case nsXPTType::T_I32   : *((int32_t*) dest) = source->val.i32; break;
    case nsXPTType::T_I64   :
    case nsXPTType::T_U64   :
      if (regwords & 1)
      {
        /* align on double word boundary */
        --dest;
        ++regwords;
      }
      *((uint32_t*) dest) = ((DU *) source)->lo;
      *((uint32_t*) --dest) = ((DU *) source)->hi;
      /* big endian - hi word in low addr */
      regwords += 2;
      continue;
    case nsXPTType::T_DOUBLE :
      if (regwords & 1)
      {
        /* align on double word boundary */
        --dest;
        ++regwords;
      }
      switch (regwords) /* load double precision float register */
      {
      case 2:
        floatflags |= 1;
      }
      *((uint32_t*) dest) = ((DU *) source)->lo;
      *((uint32_t*) --dest) = ((DU *) source)->hi;
      /* big endian - hi word in low addr */
      regwords += 2;
      continue;
    case nsXPTType::T_FLOAT :
      switch (regwords) /* load single precision float register */
      {
      case 1:
        floatflags |= 2;
        break;
      case 2:
        floatflags |= 4;
        break;
      case 3:
        floatflags |= 8;
      }
      *((float*) dest) = source->val.f;
      break;
    case nsXPTType::T_U8    : *((uint32_t*) (dest)) = source->val.u8; break;
    case nsXPTType::T_U16   : *((uint32_t*) (dest)) = source->val.u16; break;
    case nsXPTType::T_U32   : *((uint32_t*) (dest)) = source->val.u32; break;
    case nsXPTType::T_BOOL  : *((uint32_t*) (dest)) = source->val.b; break;
    case nsXPTType::T_CHAR  : *((uint32_t*) (dest)) = source->val.c; break;
    case nsXPTType::T_WCHAR : *((int32_t*)  (dest)) = source->val.wc; break;

    default:
      // all the others are plain pointer types
      *((void**) dest) = source->val.p;
    }
    ++regwords;
  }
  return floatflags;
}


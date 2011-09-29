
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

#if _HPUX
#error "This code is for HP-PA RISC 32 bit mode only"
#endif

#include <alloca.h>

typedef unsigned nsXPCVariant;

extern "C" PRInt32
invoke_count_bytes(nsISupports* that, const PRUint32 methodIndex,
  const PRUint32 paramCount, const nsXPTCVariant* s)
{
  PRInt32 result = 4; /* variant records do not include self pointer */

  /* counts the number of bytes required by the argument stack,
     64 bit integer, and double requires 8 bytes.  All else requires
     4 bytes.
   */

  {
    PRUint32 indx;
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
    PRInt32 remainder = result & 63;
    return (remainder == 0) ? result : (result + 64 - remainder);
  }
}

extern "C" PRUint32
invoke_copy_to_stack(PRUint32* d,
  const PRUint32 paramCount, nsXPTCVariant* s)
{

  typedef struct
  {
    PRUint32 hi;
    PRUint32 lo;
  } DU;

  PRUint32* dest = d;
  nsXPTCVariant* source = s;
  /* we clobber param vars by copying stuff on stack, have to use local var */

  PRUint32 floatflags = 0;
  /* flag indicating which floating point registers to load */

  PRUint32 regwords = 1; /* register 26 is reserved for ptr to 'that' */
  PRUint32 indx;

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
    case nsXPTType::T_I8    : *((PRInt32*) dest) = source->val.i8;  break;
    case nsXPTType::T_I16   : *((PRInt32*) dest) = source->val.i16; break;
    case nsXPTType::T_I32   : *((PRInt32*) dest) = source->val.i32; break;
    case nsXPTType::T_I64   :
    case nsXPTType::T_U64   :
      if (regwords & 1)
      {
        /* align on double word boundary */
        --dest;
        ++regwords;
      }
      *((uint32*) dest) = ((DU *) source)->lo;
      *((uint32*) --dest) = ((DU *) source)->hi;
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
      *((uint32*) dest) = ((DU *) source)->lo;
      *((uint32*) --dest) = ((DU *) source)->hi;
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
    case nsXPTType::T_U8    : *((PRUint32*) (dest)) = source->val.u8; break;
    case nsXPTType::T_U16   : *((PRUint32*) (dest)) = source->val.u16; break;
    case nsXPTType::T_U32   : *((PRUint32*) (dest)) = source->val.u32; break;
    case nsXPTType::T_BOOL  : *((bool*)   (dest)) = source->val.b; break;
    case nsXPTType::T_CHAR  : *((PRUint32*) (dest)) = source->val.c; break;
    case nsXPTType::T_WCHAR : *((PRInt32*)  (dest)) = source->val.wc; break;

    default:
      // all the others are plain pointer types
      *((void**) dest) = source->val.p;
    }
    ++regwords;
  }
  return floatflags;
}


/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

/* Platform specific code to invoke XPCOM methods on native objects */

#include "xptcprivate.h"

typedef unsigned nsXPCVariant;

#define FIRST_GP_REG  3
#define MAX_GP_REG   10
#define FIRST_FP_REG  1
#define MAX_FP_REG    8

extern "C" PRUint32
invoke_count_words(PRUint32 paramCount, nsXPTCVariant* s)
{
  return (PRUint32)(((paramCount * 2) + 3) & ~3);
}

extern "C" void
invoke_copy_to_stack(PRUint32* d, PRUint32 paramCount, nsXPTCVariant* s, 
		     PRUint32* gpregs, double* fpregs)
{
  uint32 gpr = FIRST_GP_REG + 1; // skip one GP reg for 'this'
  uint32 fpr = FIRST_FP_REG;

  for(uint32 i = 0; i < paramCount; i++, s++)
    {
      if(s->IsPtrData())
        {
	  if (gpr > MAX_GP_REG)
	    *((void**) d++)        = s->ptr;
	  else
	    {
	      *((void**) gpregs++) = s->ptr;
	      gpr++;
	    }
	  continue;
        }
      switch(s->type)
        {
	case nsXPTType::T_I8:
	  if (gpr > MAX_GP_REG)
	    *((PRInt32*) d++)        = s->val.i8;
	  else
	    {
	      *((PRInt32*) gpregs++) = s->val.i8;
	      gpr++;
	    }
          break;
	case nsXPTType::T_I16:
	  if (gpr > MAX_GP_REG)
	    *((PRInt32*) d++)        = s->val.i16;
	  else
	    {
	      *((PRInt32*) gpregs++) = s->val.i16;
	      gpr++;
	    }
	  break;
        case nsXPTType::T_I32:
	  if (gpr > MAX_GP_REG)
	    *((PRInt32*) d++)        = s->val.i32;
	  else
	    {
	      *((PRInt32*) gpregs++) = s->val.i32;
	      gpr++;
	    }
	  break;
        case nsXPTType::T_I64:
	  if ((gpr + 1) > MAX_GP_REG)
	    {
	      if (((PRUint32) d) & 4 != 0) d++;
	      *(((PRInt64*) d)++)      = s->val.i64;
	    }
	  else
	    {
	      if ((gpr & 1) == 0)
		{
		  gpr++;
		  gpregs++;
		}
	      *(((PRInt64*) gpregs)++) = s->val.i64;
	      gpr += 2;
	    }
	  break;
	case nsXPTType::T_U8:
	  if (gpr > MAX_GP_REG)
	    *d++        = s->val.u8;
	  else
	    {
	      *gpregs++ = s->val.u8;
	      gpr++;
	    }
          break;
	case nsXPTType::T_U16:
	  if (gpr > MAX_GP_REG)
	    *d++        = s->val.u16;
	  else
	    {
	      *gpregs++ = s->val.u16;
	      gpr++;
	    }
	  break;
        case nsXPTType::T_U32:
	  if (gpr > MAX_GP_REG)
	    *d++        = s->val.u32;
	  else
	    {
	      *gpregs++ = s->val.u32;
	      gpr++;
	    }
	  break;
        case nsXPTType::T_U64:
	  if ((gpr + 1) > MAX_GP_REG)
	    {
	      if (((PRUint32) d) & 4 != 0) d++;
	      *(((PRUint64*) d)++)      = s->val.u64;
	    }
	  else
	    {
	      if ((gpr & 1) == 0)
		{
		  gpr++;
		  gpregs++;
		}
	      *(((PRUint64*) gpregs)++) = s->val.u64;
	      gpr    += 2;
	    }
	  break;
        case nsXPTType::T_FLOAT:
	  if (fpr > MAX_FP_REG)
	      *((float*) d++) = s->val.f;
	  else
	    {
	      *fpregs++       = s->val.f;
	      fpr++;
	    }
	  break;
        case nsXPTType::T_DOUBLE:
	  if (fpr > MAX_FP_REG)
	    {
	      if (((PRUint32) d) & 4 != 0) d++;
	      *(((double*) d)++) = s->val.d;
	    }
	  else
	    {
	      *fpregs++          = s->val.d;
	      fpr++;
	    }
	  break;
        case nsXPTType::T_BOOL:
	  if (gpr > MAX_GP_REG)
	    *((PRBool*) d++)        = s->val.b;
	  else
	    {
	      *((PRBool*) gpregs++) = s->val.b;
	      gpr++;
	    }
          break;
        case nsXPTType::T_CHAR:
	  if (gpr > MAX_GP_REG)
	    *d++        = s->val.c;
	  else
	    {
	      *gpregs++ = s->val.c;
	      gpr++;
	    }
          break;
        case nsXPTType::T_WCHAR:
	  if (gpr > MAX_GP_REG)
	    *((PRInt32*) d++)        = s->val.wc;
	  else
	    {
	      *((PRInt32*) gpregs++) = s->val.wc;
	      gpr++;
	    }
          break;
        default:
	  // all the others are plain pointer types
	  if (gpr > MAX_GP_REG)
	    *((void**) d++)        = s->val.p;
	  else
	    {
	      *((void**) gpregs++) = s->val.p;
	      gpr++;
	    }
          break;
        }
    }
}

extern "C"
XPTC_PUBLIC_API(nsresult)
XPTC_InvokeByIndex(nsISupports* that, PRUint32 methodIndex,
		   PRUint32 paramCount, nsXPTCVariant* params);

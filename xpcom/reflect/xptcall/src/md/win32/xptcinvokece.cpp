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
 * The Original Code is Windows CE Reflect
 *
 * The Initial Developer of the Original Code is John Wolfe
 * Portions created by the Initial Developer are Copyright (C) 2005
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

/* Platform specific code to invoke XPCOM methods on native objects */

#include "xptcprivate.h"

extern "C" nsresult
asmXPTC_InvokeByIndex( nsISupports* that,
                       PRUint32 methodIndex,
  					   PRUint32 paramCount,
					   nsXPTCVariant* params,
					   PRUint32 pfn_CopyToStack,
					   PRUint32 pfn_CountWords);  

// NOTE: This does the actual copying into the stack - based upon the
// pointer "d" -- which is placed at the lowest location on the block
// of memory that has been allocated on the stack

extern "C" static void
invoke_copy_to_stack(PRUint32* d, PRUint32 paramCount, nsXPTCVariant* s)
{
  for(; paramCount > 0; paramCount--, d++, s++)
  {
    if(s->IsPtrData())
    {
      *((void**)d) = s->ptr;
      continue;
    }
    switch(s->type)
    {
      case nsXPTType::T_I8     : *((PRInt8*)  d) = s->val.i8;          break;
      case nsXPTType::T_I16    : *((PRInt16*) d) = s->val.i16;         break;
      case nsXPTType::T_I32    : *((PRInt32*) d) = s->val.i32;         break;
      case nsXPTType::T_I64    : *((PRInt64*) d) = s->val.i64; d++;    break;
      case nsXPTType::T_U8     : *((PRUint8*) d) = s->val.u8;          break;
      case nsXPTType::T_U16    : *((PRUint16*)d) = s->val.u16;         break;
      case nsXPTType::T_U32    : *((PRUint32*)d) = s->val.u32;         break;
      case nsXPTType::T_U64    : *((PRUint64*)d) = s->val.u64; d++;    break;
      case nsXPTType::T_FLOAT  : *((float*)   d) = s->val.f;           break;
      case nsXPTType::T_DOUBLE : *((double*)  d) = s->val.d;   d++;    break;
      case nsXPTType::T_BOOL   : *((PRBool*)  d) = s->val.b;           break;
      case nsXPTType::T_CHAR   : *((char*)    d) = s->val.c;           break;
      case nsXPTType::T_WCHAR  : *((wchar_t*) d) = s->val.wc;          break;
      default:
        // all the others are plain pointer types
        *((void**)d) = s->val.p;
        break;
    }
  }
}


extern "C" static uint32
invoke_count_words(PRUint32 paramCount, nsXPTCVariant* s)
{
  uint32 nCnt = 0;
  
  for(; paramCount > 0; paramCount--, s++)
  {
    if(s->IsPtrData())
    {
      nCnt++;
      continue;
    }
    switch(s->type)
    {
      case nsXPTType::T_I64    : 
      case nsXPTType::T_U64    : 
      case nsXPTType::T_DOUBLE : 
        nCnt += 2;
        break;
        
      case nsXPTType::T_I8     : 
      case nsXPTType::T_I16    : 
      case nsXPTType::T_I32    : 
      case nsXPTType::T_U8     : 
      case nsXPTType::T_U16    : 
      case nsXPTType::T_U32    : 
      case nsXPTType::T_FLOAT  : 
      case nsXPTType::T_BOOL   : 
      case nsXPTType::T_CHAR   : 
      case nsXPTType::T_WCHAR  : 
      default:
        nCnt++;
        break;
    }
  }
  
  // 4 Bytes for everything that uses 4 bytes or less, 8 bytes for
  // everything else.
  return nCnt;
}


extern "C" NS_EXPORT  nsresult NS_FROZENCALL
NS_InvokeByIndex_P(nsISupports* that, PRUint32 methodIndex,
                 PRUint32 paramCount, nsXPTCVariant* params)
{
  return asmXPTC_InvokeByIndex(that,
                               methodIndex,
                               paramCount,
                               params,
                               (PRUint32) &invoke_copy_to_stack,
                               (PRUint32) &invoke_count_words);
}


int g_xptcinvokece_marker;


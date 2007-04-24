/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Benjamin Smedberg <benjamin@smedbergs.us>
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

/* Public declarations for xptcall. */

#ifndef xptcall_h___
#define xptcall_h___

#ifdef MOZILLA_INTERNAL_API
# define NS_GetXPTCallStub     NS_GetXPTCallStub_P
# define NS_DestroyXPTCallStub NS_DestroyXPTCallStub_P
# define NS_InvokeByIndex      NS_InvokeByIndex_P
#endif

#include "prtypes.h"
#include "nscore.h"
#include "nsISupports.h"
#include "xpt_struct.h"
#include "xptinfo.h"

struct nsXPTCMiniVariant
{
// No ctors or dtors so that we can use arrays of these on the stack
// with no penalty.
    union
    {
        PRInt8    i8;
        PRInt16   i16;
        PRInt32   i32;
        PRInt64   i64;
        PRUint8   u8;
        PRUint16  u16;
        PRUint32  u32;
        PRUint64  u64;
        float     f;
        double    d;
        PRBool    b;
        char      c;
        PRUnichar wc;
        void*     p;
    } val;
};

struct nsXPTCVariant : public nsXPTCMiniVariant
{
// No ctors or dtors so that we can use arrays of these on the stack
// with no penalty.

    // inherits 'val' here
    void*     ptr;
    nsXPTType type;
    PRUint8   flags;

    enum
    {
        // these are bitflags!
        PTR_IS_DATA    = 0x1,  // ptr points to 'real' data in val
        VAL_IS_ALLOCD  = 0x2,  // val.p holds alloc'd ptr that must be freed
        VAL_IS_IFACE   = 0x4,  // val.p holds interface ptr that must be released
        VAL_IS_ARRAY   = 0x8,  // val.p holds a pointer to an array needing cleanup
        VAL_IS_DOMSTR  = 0x10, // val.p holds a pointer to domstring needing cleanup
        VAL_IS_UTF8STR = 0x20, // val.p holds a pointer to utf8string needing cleanup
        VAL_IS_CSTR    = 0x40  // val.p holds a pointer to cstring needing cleanup        
    };

    void ClearFlags()         {flags = 0;}
    void SetPtrIsData()       {flags |= PTR_IS_DATA;}
    void SetValIsAllocated()  {flags |= VAL_IS_ALLOCD;}
    void SetValIsInterface()  {flags |= VAL_IS_IFACE;}
    void SetValIsArray()      {flags |= VAL_IS_ARRAY;}
    void SetValIsDOMString()  {flags |= VAL_IS_DOMSTR;}
    void SetValIsUTF8String() {flags |= VAL_IS_UTF8STR;}
    void SetValIsCString()    {flags |= VAL_IS_CSTR;}    

    PRBool IsPtrData()       const  {return 0 != (flags & PTR_IS_DATA);}
    PRBool IsValAllocated()  const  {return 0 != (flags & VAL_IS_ALLOCD);}
    PRBool IsValInterface()  const  {return 0 != (flags & VAL_IS_IFACE);}
    PRBool IsValArray()      const  {return 0 != (flags & VAL_IS_ARRAY);}
    PRBool IsValDOMString()  const  {return 0 != (flags & VAL_IS_DOMSTR);}
    PRBool IsValUTF8String() const  {return 0 != (flags & VAL_IS_UTF8STR);}
    PRBool IsValCString()    const  {return 0 != (flags & VAL_IS_CSTR);}    

    void Init(const nsXPTCMiniVariant& mv, const nsXPTType& t, PRUint8 f)
    {
        type = t;
        flags = f;

        if(f & PTR_IS_DATA)
        {
            ptr = mv.val.p;
            val.p = nsnull;
        }
        else
        {
            ptr = nsnull;
            switch(t.TagPart()) {
              case nsXPTType::T_I8:                val.i8  = mv.val.i8;  break;
              case nsXPTType::T_I16:               val.i16 = mv.val.i16; break;
              case nsXPTType::T_I32:               val.i32 = mv.val.i32; break;
              case nsXPTType::T_I64:               val.i64 = mv.val.i64; break;
              case nsXPTType::T_U8:                val.u8  = mv.val.u8;  break;
              case nsXPTType::T_U16:               val.u16 = mv.val.u16; break;
              case nsXPTType::T_U32:               val.u32 = mv.val.u32; break;
              case nsXPTType::T_U64:               val.u64 = mv.val.u64; break;
              case nsXPTType::T_FLOAT:             val.f   = mv.val.f;   break;
              case nsXPTType::T_DOUBLE:            val.d   = mv.val.d;   break;
              case nsXPTType::T_BOOL:              val.b   = mv.val.b;   break;
              case nsXPTType::T_CHAR:              val.c   = mv.val.c;   break;
              case nsXPTType::T_WCHAR:             val.wc  = mv.val.wc;  break;
              case nsXPTType::T_VOID:              /* fall through */
              case nsXPTType::T_IID:               /* fall through */
              case nsXPTType::T_DOMSTRING:         /* fall through */
              case nsXPTType::T_CHAR_STR:          /* fall through */
              case nsXPTType::T_WCHAR_STR:         /* fall through */
              case nsXPTType::T_INTERFACE:         /* fall through */
              case nsXPTType::T_INTERFACE_IS:      /* fall through */
              case nsXPTType::T_ARRAY:             /* fall through */
              case nsXPTType::T_PSTRING_SIZE_IS:   /* fall through */
              case nsXPTType::T_PWSTRING_SIZE_IS:  /* fall through */
              case nsXPTType::T_UTF8STRING:        /* fall through */
              case nsXPTType::T_CSTRING:           /* fall through */              
              default:                             val.p   = mv.val.p;   break;
            }
        }
    }
};

class nsIXPTCProxy : public nsISupports
{
public:
    NS_IMETHOD CallMethod(PRUint16 aMethodIndex,
                          const XPTMethodDescriptor *aInfo,
                          nsXPTCMiniVariant *aParams) = 0;
};

/**
 * This is a typedef to avoid confusion between the canonical
 * nsISupports* that provides object identity and an interface pointer
 * for inheriting interfaces that aren't known at compile-time.
 */
typedef nsISupports nsISomeInterface;

/**
 * Get a proxy object to implement the specified interface.
 *
 * @param aIID    The IID of the interface to implement.
 * @param aOuter  An object to receive method calls from the proxy object.
 *                The stub forwards QueryInterface/AddRef/Release to the
 *                outer object. The proxy object does not hold a reference to
 *                the outer object; it is the caller's responsibility to
 *                ensure that this pointer remains valid until the stub has
 *                been destroyed.
 * @param aStub   Out parameter for the new proxy object. The object is
 *                not addrefed. The object never destroys itself. It must be
 *                explicitly destroyed by calling
 *                NS_DestroyXPTCallStub when it is no longer needed.
 */
XPCOM_API(nsresult)
NS_GetXPTCallStub(REFNSIID aIID, nsIXPTCProxy* aOuter,
                  nsISomeInterface* *aStub);

/**
 * Destroys an XPTCall stub previously created with NS_GetXPTCallStub.
 */
XPCOM_API(void)
NS_DestroyXPTCallStub(nsISomeInterface* aStub);

XPCOM_API(nsresult)
NS_InvokeByIndex(nsISupports* that, PRUint32 methodIndex,
                 PRUint32 paramCount, nsXPTCVariant* params);

#endif /* xptcall_h___ */

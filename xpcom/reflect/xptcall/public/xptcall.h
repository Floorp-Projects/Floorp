/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
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
 * Copyright (C) 1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

/* Public declarations for xptcall. */

#ifndef xptcall_h___
#define xptcall_h___

#include "prtypes.h"
#include "nscore.h"
#include "nsISupports.h"
#include "xpt_struct.h"
#include "xptinfo.h"
#include "nsIInterfaceInfo.h"

/***************************************************************************/
/*
 * The linkage of XPTC API functions differs depending on whether the file is
 * used within the XPTC library or not.  Any source file within the XPTC
 * library should define EXPORT_XPTC_API whereas any client of the library
 * should not.
 */
#ifdef EXPORT_XPTC_API
#define XPTC_PUBLIC_API(t)    PR_IMPLEMENT(t)
#define XPTC_PUBLIC_DATA(t)   PR_IMPLEMENT_DATA(t)
#ifdef _WIN32
#    define XPTC_EXPORT           _declspec(dllexport)
#else
#    define XPTC_EXPORT
#endif
#else
#ifdef _WIN32
#    define XPTC_PUBLIC_API(t)    _declspec(dllimport) t
#    define XPTC_PUBLIC_DATA(t)   _declspec(dllimport) t
#    define XPTC_EXPORT           _declspec(dllimport)
#else
#    define XPTC_PUBLIC_API(t)    PR_IMPLEMENT(t)
#    define XPTC_PUBLIC_DATA(t)   t
#    define XPTC_EXPORT
#endif
#endif
#define XPTC_FRIEND_API(t)    XPTC_PUBLIC_API(t)
#define XPTC_FRIEND_DATA(t)   XPTC_PUBLIC_DATA(t)
/***************************************************************************/

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
        PTR_IS_DATA  = 0x1, // ptr points to 'real' data in val
        VAL_IS_OWNED = 0x2, // val.p holds alloc'd ptr that must be freed
        VAL_IS_IFACE = 0x4, // val.p holds interface ptr that must be released
        VAL_IS_ARRAY = 0x8  // val.p holds a pointer to an array needing cleanup
    };

    PRBool IsPtrData()      const {return (PRBool) (flags & PTR_IS_DATA);}
    PRBool IsValOwned()     const {return (PRBool) (flags & VAL_IS_OWNED);}
    PRBool IsValInterface() const {return (PRBool) (flags & VAL_IS_IFACE);}
    PRBool IsValArray()     const {return (PRBool) (flags & VAL_IS_ARRAY);}

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
          case nsXPTType::T_BSTR:              /* fall through */
          case nsXPTType::T_CHAR_STR:          /* fall through */
          case nsXPTType::T_WCHAR_STR:         /* fall through */
          case nsXPTType::T_INTERFACE:         /* fall through */
          case nsXPTType::T_INTERFACE_IS:      /* fall through */
          case nsXPTType::T_ARRAY:             /* fall through */
          case nsXPTType::T_PSTRING_SIZE_IS:   /* fall through */
          case nsXPTType::T_PWSTRING_SIZE_IS:  /* fall through */
          default:                             val.p   = mv.val.p;   break;
        }
        }
    }
};

/***************************************************************************/

class nsXPTCStubBase : public nsISupports
{
public:
    // We are going to implement this to force the compiler to generate a 
    // vtbl for this class. Since this is overridden in the inheriting class
    // we expect it to never be called. 
    // *This is needed by the Irix implementation.*
    XPTC_EXPORT NS_IMETHOD QueryInterface(REFNSIID aIID, void** aInstancePtr);

    // Include generated vtbl stub declarations.
    // These are virtual and *also* implemented by this class..
#include "xptcstubsdecl.inc"

    // The following methods must be provided by inheritor of this class.

    // return a refcounted pointer to the InterfaceInfo for this object
    // NOTE: on some platforms this MUST not fail or we crash!
    NS_IMETHOD GetInterfaceInfo(nsIInterfaceInfo** info) = 0;

    // call this method and return result
    NS_IMETHOD CallMethod(PRUint16 methodIndex,
                          const nsXPTMethodInfo* info,
                          nsXPTCMiniVariant* params) = 0;
};


PR_BEGIN_EXTERN_C

XPTC_PUBLIC_API(nsresult)
XPTC_InvokeByIndex(nsISupports* that, PRUint32 methodIndex,
                   PRUint32 paramCount, nsXPTCVariant* params);

// Used to force linking of these obj for the static library into the dll
extern void xptc_dummy();
extern void xptc_dummy2();

PR_END_EXTERN_C

#endif /* xptcall_h___ */

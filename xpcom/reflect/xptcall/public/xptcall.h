/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
#include "jsapi.h"

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
        bool      b;
        char      c;
        PRUnichar wc;
        void*     p;

        // Types below here are unknown to the assembly implementations, and
        // therefore _must_ be passed with indirect semantics. We put them in
        // the union here for type safety, so that we can avoid void* tricks.
        jsval j;
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
        //
        // Bitflag definitions
        //

        // Indicates that ptr (above, and distinct from val.p) is the value that
        // should be passed on the stack.
        //
        // In theory, ptr could point anywhere. But in practice it always points
        // to &val. So this flag is used to pass 'val' by reference, letting us
        // avoid the extra allocation we would incur if we were to use val.p.
        //
        // Various parts of XPConnect assume that ptr==&val, so we enforce it
        // explicitly with SetIndirect() and IsIndirect().
        //
        // Since ptr always points to &val, the semantics of this flag are kind of
        // dumb, since the ptr field is unnecessary. But changing them would
        // require changing dozens of assembly files, so they're likely to stay
        // the way they are.
        PTR_IS_DATA    = 0x1,

        // Indicates that the value we hold requires some sort of cleanup (memory
        // deallocation, interface release, jsval unrooting, etc). The precise
        // cleanup that is performed depends on the 'type' field above.
        // If the value is an array, this flag specifies whether the elements
        // within the array require cleanup (we always clean up the array itself,
        // so this flag would be redundant for that purpose).
        VAL_NEEDS_CLEANUP = 0x2
    };

    void ClearFlags()         {flags = 0;}
    void SetIndirect()        {ptr = &val; flags |= PTR_IS_DATA;}
    void SetValNeedsCleanup() {flags |= VAL_NEEDS_CLEANUP;}

    bool IsIndirect()         const  {return 0 != (flags & PTR_IS_DATA);}
    bool DoesValNeedCleanup() const  {return 0 != (flags & VAL_NEEDS_CLEANUP);}

    // Internal use only. Use IsIndirect() instead.
    bool IsPtrData()       const  {return 0 != (flags & PTR_IS_DATA);}

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
            val.p = nsnull; // make sure 'val.p' is always initialized
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

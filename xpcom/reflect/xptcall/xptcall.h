/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Public declarations for xptcall. */

#ifndef xptcall_h___
#define xptcall_h___

#include "nscore.h"
#include "nsISupports.h"
#include "xptinfo.h"
#include "js/Value.h"
#include "mozilla/Attributes.h"
#include "mozilla/MemoryReporting.h"

struct nsXPTCMiniVariant
{
// No ctors or dtors so that we can use arrays of these on the stack
// with no penalty.
    union Union
    {
        int8_t    i8;
        int16_t   i16;
        int32_t   i32;
        int64_t   i64;
        uint8_t   u8;
        uint16_t  u16;
        uint32_t  u32;
        uint64_t  u64;
        float     f;
        double    d;
        bool      b;
        char      c;
        char16_t wc;
        void*     p;
    };

    Union val;
};

static_assert(offsetof(nsXPTCMiniVariant, val) == 0,
              "nsXPTCMiniVariant must be a thin wrapper");

struct nsXPTCVariant
{
// No ctors or dtors so that we can use arrays of these on the stack with no
// penalty.
    union ExtendedVal
    {
    // ExtendedVal is an extension on nsXPTCMiniVariant. It contains types
    // unknown to the assembly implementations which must be passed by indirect
    // semantics.
    //
    // nsXPTCVariant contains enough space to store ExtendedVal inline, which
    // can be used to store these types when IsIndirect() is true.
        nsXPTCMiniVariant mini;

        nsCString  nscstr;
        nsString   nsstr;
        JS::Value  jsval;

        // This type contains non-standard-layout types, so needs an explicit
        // Ctor/Dtor - we'll just delete them.
        ExtendedVal() = delete;
        ~ExtendedVal() = delete;
    };

    union
    {
        // The `val` field from nsXPTCMiniVariant.
        nsXPTCMiniVariant::Union val;

        // Storage for any extended variants.
        ExtendedVal ext;
    };

    void*     ptr;
    nsXPTType type;
    uint8_t   flags;

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
        // deallocation, interface release, JS::Value unrooting, etc). The precise
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

    // Implicitly convert to nsXPTCMiniVariant.
    operator nsXPTCMiniVariant&() {
        return *(nsXPTCMiniVariant*) &val;
    }
    operator const nsXPTCMiniVariant&() const {
        return *(const nsXPTCMiniVariant*) &val;
    }

    // As this type contains an anonymous union, we need to provide explicit
    // constructors & destructors.
    nsXPTCVariant() { }
    ~nsXPTCVariant() { }
};

static_assert(offsetof(nsXPTCVariant, val) == offsetof(nsXPTCVariant, ext),
              "nsXPTCVariant::{ext,val} must have matching offsets");

class nsIXPTCProxy : public nsISupports
{
public:
    NS_IMETHOD CallMethod(uint16_t aMethodIndex,
                          const nsXPTMethodInfo *aInfo,
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

/**
 * Measures the size of an XPTCall stub previously created with NS_GetXPTCallStub.
 */
XPCOM_API(size_t)
NS_SizeOfIncludingThisXPTCallStub(const nsISomeInterface* aStub, mozilla::MallocSizeOf aMallocSizeOf);

// this is extern "C" because on some platforms it is implemented in assembly
extern "C" nsresult
NS_InvokeByIndex(nsISupports* that, uint32_t methodIndex,
                 uint32_t paramCount, nsXPTCVariant* params);

#endif /* xptcall_h___ */

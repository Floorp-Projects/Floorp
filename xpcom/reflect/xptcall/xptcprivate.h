/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* All the xptcall private declarations - only include locally. */

#ifndef xptcprivate_h___
#define xptcprivate_h___

#include "xptcall.h"
#include "nsAutoPtr.h"
#include "mozilla/Attributes.h"

class xptiInterfaceEntry;

#if !defined(__ia64) || (!defined(__hpux) && !defined(__linux__) && !defined(__FreeBSD__))
#define STUB_ENTRY(n) NS_IMETHOD Stub##n() = 0;
#else
#define STUB_ENTRY(n) NS_IMETHOD Stub##n(uint64_t,uint64_t,uint64_t,uint64_t,uint64_t,uint64_t,uint64_t,uint64_t) = 0;
#endif

#define SENTINEL_ENTRY(n) NS_IMETHOD Sentinel##n() = 0;

class nsIXPTCStubBase : public nsISupports
{
public:
#include "xptcstubsdef.inc"
};

#undef STUB_ENTRY
#undef SENTINEL_ENTRY

#if !defined(__ia64) || (!defined(__hpux) && !defined(__linux__) && !defined(__FreeBSD__))
#define STUB_ENTRY(n) NS_IMETHOD Stub##n();
#else
#define STUB_ENTRY(n) NS_IMETHOD Stub##n(uint64_t,uint64_t,uint64_t,uint64_t,uint64_t,uint64_t,uint64_t,uint64_t);
#endif

#define SENTINEL_ENTRY(n) NS_IMETHOD Sentinel##n();

class nsXPTCStubBase MOZ_FINAL : public nsIXPTCStubBase
{
public:
    NS_DECL_ISUPPORTS_INHERITED

#include "xptcstubsdef.inc"

    nsXPTCStubBase(nsIXPTCProxy* aOuter, xptiInterfaceEntry *aEntry) :
        mOuter(aOuter), mEntry(aEntry) { }

    nsIXPTCProxy*          mOuter;
    xptiInterfaceEntry*    mEntry;

    ~nsXPTCStubBase() { }
};

#undef STUB_ENTRY
#undef SENTINEL_ENTRY

#if defined(__clang__) || defined(__GNUC__)
#define ATTRIBUTE_USED __attribute__ ((__used__))
#else
#define ATTRIBUTE_USED
#endif

#endif /* xptcprivate_h___ */

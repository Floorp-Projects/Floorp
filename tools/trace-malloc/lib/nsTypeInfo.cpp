/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express oqr
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is nsTypeInfo.cpp code, released
 * November 27, 2000.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 2000 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *    Patrick C. Beard <beard@netscape.com>
 *    Chris Waterson <waterson@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU Public License (the "GPL"), in which case the
 * provisions of the GPL are applicable instead of those above.
 * If you wish to allow use of your version of this file only
 * under the terms of the GPL and not to allow others to use your
 * version of this file under the MPL, indicate your decision by
 * deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL.  If you do not delete
 * the provisions above, a recipient may use your version of this
 * file under either the MPL or the GPL.
 */

/*
  typeinfo.cpp
	
  Speculatively use RTTI on a random object. If it contains a pointer at offset 0
  that is in the current process' address space, and that so on, then attempt to
  use C++ RTTI's typeid operation to obtain the name of the type.
  
  by Patrick C. Beard.
 */

#include <typeinfo>
#include <ctype.h>

extern "C" const char* nsGetTypeName(void* ptr);

class IUnknown {
public:
    virtual long QueryInterface() = 0;
    virtual long AddRef() = 0;
    virtual long Release() = 0;
};

#if defined(MACOS)

#include <Processes.h>

class AddressSpace {
public:
    AddressSpace();
    Boolean contains(void* ptr);
private:
    ProcessInfoRec mInfo;
};

AddressSpace::AddressSpace()
{
    ProcessSerialNumber psn = { 0, kCurrentProcess };
    mInfo.processInfoLength = sizeof(mInfo);
    ::GetProcessInformation(&psn, &mInfo);
}

Boolean AddressSpace::contains(void* ptr)
{
    UInt32 start = UInt32(mInfo.processLocation);
    return (UInt32(ptr) >= start && UInt32(ptr) < (start + mInfo.processSize));
}

const char* nsGetTypeName(void* ptr)
{
    // construct only one of these per process.
    static AddressSpace space;
	
    // sanity check the vtable pointer, before trying to use RTTI on the object.
    void** vt = *(void***)ptr;
    if (vt && !(unsigned(vt) & 0x3) && space.contains(vt) && space.contains(*vt)) {
	IUnknown* u = static_cast<IUnknown*>(ptr);
	const char* type = typeid(*u).name();
        // make sure it looks like a C++ identifier.
	if (type && (isalnum(type[0]) || type[0] == '_'))
	    return type;
    }
    return "void*";
}

#endif

#if defined(linux)

#include <signal.h>
#include <setjmp.h>

static jmp_buf context;

static void handler(int signum)
{
    longjmp(context, signum);
}

#define attempt() setjmp(context)

class Signaller {
public:
    Signaller(int signum);
    ~Signaller();

private:
    typedef void (*handler_t) (int signum);
    int mSignal;
    handler_t mOldHandler;
};

Signaller::Signaller(int signum)
    : mSignal(signum), mOldHandler(signal(signum, &handler))
{
}

Signaller::~Signaller()
{
    signal(mSignal, mOldHandler);
}

// The following are pointers that bamboozle are otherwise feeble
// attempts to "safely" collect type names.
//
// XXX this kind of sucks because it means that anyone trying to use
// this without NSPR will get unresolved symbols when this library
// loads. It's also not very extensable. Oh well: FIX ME!
extern "C" {
    // from nsprpub/pr/src/io/priometh.c (libnspr4.so)
    extern void* _pr_faulty_methods;

    // from nsprpub/pr/src/io/prlayer.c (libnspr4.so)
    extern void* pl_methods;
};

static int
is_bamboozler(void* vt)
{
    static void* bamboolzers[] = {
        _pr_faulty_methods,
        pl_methods,
        0
    };

    for (void** fn = bamboolzers; *fn; ++fn) {
        if (*fn == vt)
            return 1;
    }

    return 0;
}



static inline int
sanity_check_vtable_i386(void** vt)
{
    // Now that we're "safe" inside the signal handler, we can
    // start poking around. If we're really an object with
    // RTTI, then the second entry in the vtable should point
    // to a function.
    //
    // Let's see if the second entry:
    //
    // 1) looks like a 4-byte aligned pointer
    //
    // 2) points to something that looks like the following
    //    i386 instructions:
    //
    //    55     push %ebp
    //    89e5   mov  %esp,%ebp
    //
    //    (which is the standard function prologue generated
    //    by egcs).
    unsigned** i = reinterpret_cast<unsigned**>(vt) + 1;
    return !(unsigned(*i) & 3) && ((**i & 0xffffff) == 0xe58955);
}

static inline int
sanity_check_vtable_ppc(void** vt)
{
    // XXX write me!
    return 1;
}

#if defined(__i386)
#  define SANITY_CHECK_VTABLE(vt) (sanity_check_vtable_i386(vt))
#elif defined(PPC)
#  define SANITY_CHECK_VTABLE(vt) (sanity_check_vtable_ppc(vt))
#else
#  define SANITY_CHECK_VTABLE(vt) (1)
#endif

const char* nsGetTypeName(void* ptr)
{
    // sanity check the vtable pointer, before trying to use RTTI on the object.
    void** vt = *(void***)ptr;
    if (vt && !(unsigned(vt) & 3) && !is_bamboozler(vt)) {
        Signaller s1(SIGSEGV);
        if (attempt() == 0) {
            if (SANITY_CHECK_VTABLE(vt)) {
                // Looks like a function: what the hell, let's call it.
                IUnknown* u = static_cast<IUnknown*>(ptr);
                const char* type = typeid(*u).name();
                // EGCS seems to prefix a length string.
                while (isdigit(*type)) ++type;
                return type;
            }
        }
    }
    return "void*";
}

#endif

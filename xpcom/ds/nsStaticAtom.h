/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsStaticAtom_h__
#define nsStaticAtom_h__

#include "nsIAtom.h"
#include "nsStringBuffer.h"
#include "prlog.h"

#if defined(HAVE_CPP_CHAR16_T)
#define NS_STATIC_ATOM_USE_WIDE_STRINGS
typedef char16_t nsStaticAtomStringType;
#elif defined(HAVE_CPP_2BYTE_WCHAR_T)
#define NS_STATIC_ATOM_USE_WIDE_STRINGS
typedef wchar_t nsStaticAtomStringType;
#else
typedef char nsStaticAtomStringType;
#endif

#define NS_STATIC_ATOM(buffer_name, atom_ptr)  { (nsStringBuffer*) &buffer_name, atom_ptr }
#define NS_STATIC_ATOM_BUFFER(buffer_name, str_data) static nsFakeStringBuffer< sizeof(str_data) > buffer_name = { 1, sizeof(str_data) * sizeof(nsStaticAtomStringType), NS_L(str_data) };

/**
 * Holds data used to initialize large number of atoms during startup. Use
 * the above macros to initialize these structs. They should never be accessed
 * directly other than from AtomTable.cpp
 */
struct nsStaticAtom {
    nsStringBuffer* mStringBuffer;
    nsIAtom ** mAtom;
};

/**
 * This is a struct with the same binary layout as a nsStringBuffer.
 */
template <PRUint32 size>
struct nsFakeStringBuffer {
    PRInt32 mRefCnt;
    PRUint32 mSize;
    nsStaticAtomStringType mStringData[size];
};

// Register an array of static atoms with the atom table
template<PRUint32 N>
nsresult
NS_RegisterStaticAtoms(const nsStaticAtom (&atoms)[N])
{
    extern nsresult RegisterStaticAtoms(const nsStaticAtom*, PRUint32 aAtomCount);
    return RegisterStaticAtoms(atoms, N);
}

#endif

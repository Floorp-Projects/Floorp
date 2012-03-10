/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Original Code is Static Atom classes.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corp.
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Alec Flett <alecf@netscape.com>
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

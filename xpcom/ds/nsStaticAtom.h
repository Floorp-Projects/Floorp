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

// see http://www.mozilla.org/projects/xpcom/atoms.html to use this stuff

// class for declaring a static list of atoms, for use with gperf
// Keep this VERY simple
// mString: the value of the atom - the policy is that this is only
//          ASCII, in order to avoid unnecessary conversions if
//          someone asks for this in unicode
// mAtom:   a convienience pointer - if you want to store the value of
//          the atom created by this structure somewhere, put its
//          address here
struct nsStaticAtom {
    const char* mString;
    nsIAtom ** mAtom;
};


// register your lookup function with the atom table. Your function
// will be called when at atom is not found in the main atom table.
NS_COM nsresult
NS_RegisterStaticAtoms(const nsStaticAtom*, PRUint32 aAtomCount);

#endif

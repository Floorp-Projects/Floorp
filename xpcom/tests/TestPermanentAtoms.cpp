/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
// vim:cindent:ts=8:et:sw=4:
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
 * The Original Code is TestPermanentAtoms.cpp.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   L. David Baron <dbaron@dbaron.org> (original author)
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

#include "nsIAtom.h"
#include "nsAtomTable.h"
#include "nsCOMPtr.h"
#include <stdio.h>
#include "nsString.h"
#include "nsReadableUtils.h"

static void Assert(PRBool aCondition, const char* aStatement)
{
    printf("%s: %s\n", aCondition?"PASS":"FAIL", aStatement);
}

static void AssertString(nsIAtom *aAtom, const nsACString& aString)
{
    const char *str;
    NS_STATIC_CAST(AtomImpl*,aAtom)->GetUTF8String(&str);
    Assert(nsDependentCString(str) == aString, "string is correct");
}

static void AssertPermanence(nsIAtom *aAtom, PRBool aPermanence)
{
    Assert(NS_STATIC_CAST(AtomImpl*,aAtom)->IsPermanent() == aPermanence,
           aPermanence ? "atom is permanent" : "atom is not permanent");
}

int main()
{
    nsCOMPtr<nsIAtom> foo = do_GetAtom("foo");
    AssertString(foo, NS_LITERAL_CSTRING("foo"));
    AssertPermanence(foo, PR_FALSE);

    nsCOMPtr<nsIAtom> foop = do_GetPermanentAtom("foo");
    AssertString(foop, NS_LITERAL_CSTRING("foo"));
    AssertPermanence(foop, PR_TRUE);
    
    Assert(foo == foop, "atoms are equal");
    
    nsCOMPtr<nsIAtom> barp = do_GetPermanentAtom("bar");
    AssertString(barp, NS_LITERAL_CSTRING("bar"));
    AssertPermanence(barp, PR_TRUE);
    
    nsCOMPtr<nsIAtom> bar = do_GetAtom("bar");
    AssertString(bar, NS_LITERAL_CSTRING("bar"));
    AssertPermanence(bar, PR_TRUE);

    Assert(bar == barp, "atoms are equal");
    
    return 0;
}

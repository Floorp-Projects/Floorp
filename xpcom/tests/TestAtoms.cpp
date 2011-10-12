/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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
#include "nsString.h"
#include "UTFStrings.h"
#include "nsIServiceManager.h"
#include "nsStaticAtom.h"

namespace TestAtoms {

bool
test_basic()
{
  for (unsigned int i = 0; i < NS_ARRAY_LENGTH(ValidStrings); ++i) {
    nsDependentString str16(ValidStrings[i].m16);
    nsDependentCString str8(ValidStrings[i].m8);

    nsCOMPtr<nsIAtom> atom = do_GetAtom(str16);
    
    if (!atom->Equals(str16) || !atom->EqualsUTF8(str8))
      return PR_FALSE;

    nsString tmp16;
    nsCString tmp8;
    atom->ToString(tmp16);
    atom->ToUTF8String(tmp8);
    if (!str16.Equals(tmp16) || !str8.Equals(tmp8))
      return PR_FALSE;

    if (!nsDependentString(atom->GetUTF16String()).Equals(str16))
      return PR_FALSE;

    if (!nsAtomString(atom).Equals(str16) ||
        !nsDependentAtomString(atom).Equals(str16) ||
        !nsAtomCString(atom).Equals(str8))
      return PR_FALSE;
  }
  
  return PR_TRUE;
}

bool
test_16vs8()
{
  for (unsigned int i = 0; i < NS_ARRAY_LENGTH(ValidStrings); ++i) {
    nsCOMPtr<nsIAtom> atom16 = do_GetAtom(ValidStrings[i].m16);
    nsCOMPtr<nsIAtom> atom8 = do_GetAtom(ValidStrings[i].m8);
    if (atom16 != atom8)
      return PR_FALSE;
  }
  
  return PR_TRUE;
}

bool
test_buffersharing()
{
  nsString unique;
  unique.AssignLiteral("this is a unique string !@#$");
  
  nsCOMPtr<nsIAtom> atom = do_GetAtom(unique);
  
  return unique.get() == atom->GetUTF16String();
}

bool
test_null()
{
  nsAutoString str(NS_LITERAL_STRING("string with a \0 char"));
  nsDependentString strCut(str.get());

  if (str.Equals(strCut))
    return PR_FALSE;
  
  nsCOMPtr<nsIAtom> atomCut = do_GetAtom(strCut);
  nsCOMPtr<nsIAtom> atom = do_GetAtom(str);
  
  return atom->GetLength() == str.Length() &&
         atom->Equals(str) &&
         atom->EqualsUTF8(NS_ConvertUTF16toUTF8(str)) &&
         atom != atomCut &&
         atomCut->Equals(strCut);
}

bool
test_invalid()
{
  for (unsigned int i = 0; i < NS_ARRAY_LENGTH(Invalid16Strings); ++i) {
    nsrefcnt count = NS_GetNumberOfAtoms();

    {
      nsCOMPtr<nsIAtom> atom16 = do_GetAtom(Invalid16Strings[i].m16);
      if (!atom16->Equals(nsDependentString(Invalid16Strings[i].m16)))
        return PR_FALSE;
    }
    
    if (count != NS_GetNumberOfAtoms())
      return PR_FALSE;
  }

  for (unsigned int i = 0; i < NS_ARRAY_LENGTH(Invalid8Strings); ++i) {
    nsrefcnt count = NS_GetNumberOfAtoms();

    {
      nsCOMPtr<nsIAtom> atom8 = do_GetAtom(Invalid8Strings[i].m8);
      nsCOMPtr<nsIAtom> atom16 = do_GetAtom(Invalid8Strings[i].m16);
      if (atom16 != atom8 ||
          !atom16->Equals(nsDependentString(Invalid8Strings[i].m16)))
        return PR_FALSE;
    }
    
    if (count != NS_GetNumberOfAtoms())
      return PR_FALSE;
  }

// Don't run this test in debug builds as that intentionally asserts.
#ifndef DEBUG
  nsCOMPtr<nsIAtom> emptyAtom = do_GetAtom("");

  for (unsigned int i = 0; i < NS_ARRAY_LENGTH(Malformed8Strings); ++i) {
    nsrefcnt count = NS_GetNumberOfAtoms();

    nsCOMPtr<nsIAtom> atom8 = do_GetAtom(Malformed8Strings[i]);
    if (atom8 != emptyAtom ||
        count != NS_GetNumberOfAtoms())
      return PR_FALSE;
  }
#endif

  return PR_TRUE;
}

#define FIRST_ATOM_STR "first static atom. Hello!"
#define SECOND_ATOM_STR "second static atom. @World!"
#define THIRD_ATOM_STR "third static atom?!"

static nsIAtom* sAtom1 = 0;
static nsIAtom* sAtom2 = 0;
static nsIAtom* sAtom3 = 0;
NS_STATIC_ATOM_BUFFER(sAtom1_buffer, FIRST_ATOM_STR)
NS_STATIC_ATOM_BUFFER(sAtom2_buffer, SECOND_ATOM_STR)
NS_STATIC_ATOM_BUFFER(sAtom3_buffer, THIRD_ATOM_STR)
static const nsStaticAtom sAtoms_info[] = {
  NS_STATIC_ATOM(sAtom1_buffer, &sAtom1),
  NS_STATIC_ATOM(sAtom2_buffer, &sAtom2),
  NS_STATIC_ATOM(sAtom3_buffer, &sAtom3),
};

bool
isStaticAtom(nsIAtom* atom)
{
  // Don't use logic && in order to ensure that all addrefs/releases are always
  // run, even if one of the tests fail. This allows us to run this code on a
  // non-static atom without affecting its refcount.
  return (atom->AddRef() == 2) &
         (atom->AddRef() == 2) &
         (atom->AddRef() == 2) &
         (atom->Release() == 1) &
         (atom->Release() == 1) &
         (atom->Release() == 1);
}

bool
test_atomtable()
{
  nsrefcnt count = NS_GetNumberOfAtoms();
  
  nsCOMPtr<nsIAtom> thirdNonPerm = do_GetAtom(THIRD_ATOM_STR);
  
  if (isStaticAtom(thirdNonPerm))
    return PR_FALSE;

  if (!thirdNonPerm || NS_GetNumberOfAtoms() != count + 1)
    return PR_FALSE;

  NS_RegisterStaticAtoms(sAtoms_info, NS_ARRAY_LENGTH(sAtoms_info));

  return sAtom1 &&
         sAtom1->Equals(NS_LITERAL_STRING(FIRST_ATOM_STR)) &&
         isStaticAtom(sAtom1) &&
         sAtom2 &&
         sAtom2->Equals(NS_LITERAL_STRING(SECOND_ATOM_STR)) &&
         isStaticAtom(sAtom2) &&
         sAtom3 &&
         sAtom3->Equals(NS_LITERAL_STRING(THIRD_ATOM_STR)) &&
         isStaticAtom(sAtom3) &&
         NS_GetNumberOfAtoms() == count + 3 &&
         thirdNonPerm == sAtom3;
}

#define FIRST_PERM_ATOM_STR "first permanent atom. Hello!"
#define SECOND_PERM_ATOM_STR "second permanent atom. @World!"

bool
test_permanent()
{
  nsrefcnt count = NS_GetNumberOfAtoms();

  {
    nsCOMPtr<nsIAtom> first = do_GetAtom(FIRST_PERM_ATOM_STR);
    if (!first->Equals(NS_LITERAL_STRING(FIRST_PERM_ATOM_STR)) ||
        isStaticAtom(first))
      return PR_FALSE;
  
    nsCOMPtr<nsIAtom> first_p =
      NS_NewPermanentAtom(NS_LITERAL_STRING(FIRST_PERM_ATOM_STR));
    if (!first_p->Equals(NS_LITERAL_STRING(FIRST_PERM_ATOM_STR)) ||
        !isStaticAtom(first_p) ||
        first != first_p)
      return PR_FALSE;
  
    nsCOMPtr<nsIAtom> second_p =
      NS_NewPermanentAtom(NS_LITERAL_STRING(SECOND_PERM_ATOM_STR));
    if (!second_p->Equals(NS_LITERAL_STRING(SECOND_PERM_ATOM_STR)) ||
        !isStaticAtom(second_p))
      return PR_FALSE;
  
    nsCOMPtr<nsIAtom> second = do_GetAtom(SECOND_PERM_ATOM_STR);
    if (!second->Equals(NS_LITERAL_STRING(SECOND_PERM_ATOM_STR)) ||
        !isStaticAtom(second) ||
        second != second_p)
      return PR_FALSE;
  }

  return NS_GetNumberOfAtoms() == count + 2;
}

typedef bool (*TestFunc)();

static const struct Test
  {
    const char* name;
    TestFunc    func;
  }
tests[] =
  {
    { "test_basic", test_basic },
    { "test_16vs8", test_16vs8 },
    { "test_buffersharing", test_buffersharing },
    { "test_null", test_null },
    { "test_invalid", test_invalid },
// FIXME: Bug 577500 TestAtoms fails when run in dist/bin due to
// static atom table already being closed. TestStaticAtoms has similar
// failure.
#if 0
    { "test_atomtable", test_atomtable },
    { "test_permanent", test_permanent },
#endif
    { nsnull, nsnull }
  };

}

using namespace TestAtoms;

int main()
  {
    {
      nsCOMPtr<nsIServiceManager> servMan;
      NS_InitXPCOM2(getter_AddRefs(servMan), nsnull, nsnull);
  
      for (const Test* t = tests; t->name != nsnull; ++t)
        {
          printf("%25s : %s\n", t->name, t->func() ? "SUCCESS" : "FAILURE <--");
        }
    }

    NS_ShutdownXPCOM(nsnull);

    return 0;
  }

/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Util.h"

#include "nsIAtom.h"
#include "nsString.h"
#include "UTFStrings.h"
#include "nsIServiceManager.h"
#include "nsStaticAtom.h"

using namespace mozilla;

namespace TestAtoms {

bool
test_basic()
{
  for (unsigned int i = 0; i < ArrayLength(ValidStrings); ++i) {
    nsDependentString str16(ValidStrings[i].m16);
    nsDependentCString str8(ValidStrings[i].m8);

    nsCOMPtr<nsIAtom> atom = do_GetAtom(str16);
    
    if (!atom->Equals(str16) || !atom->EqualsUTF8(str8))
      return false;

    nsString tmp16;
    nsCString tmp8;
    atom->ToString(tmp16);
    atom->ToUTF8String(tmp8);
    if (!str16.Equals(tmp16) || !str8.Equals(tmp8))
      return false;

    if (!nsDependentString(atom->GetUTF16String()).Equals(str16))
      return false;

    if (!nsAtomString(atom).Equals(str16) ||
        !nsDependentAtomString(atom).Equals(str16) ||
        !nsAtomCString(atom).Equals(str8))
      return false;
  }
  
  return true;
}

bool
test_16vs8()
{
  for (unsigned int i = 0; i < ArrayLength(ValidStrings); ++i) {
    nsCOMPtr<nsIAtom> atom16 = do_GetAtom(ValidStrings[i].m16);
    nsCOMPtr<nsIAtom> atom8 = do_GetAtom(ValidStrings[i].m8);
    if (atom16 != atom8)
      return false;
  }
  
  return true;
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
    return false;
  
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
  for (unsigned int i = 0; i < ArrayLength(Invalid16Strings); ++i) {
    nsrefcnt count = NS_GetNumberOfAtoms();

    {
      nsCOMPtr<nsIAtom> atom16 = do_GetAtom(Invalid16Strings[i].m16);
      if (!atom16->Equals(nsDependentString(Invalid16Strings[i].m16)))
        return false;
    }
    
    if (count != NS_GetNumberOfAtoms())
      return false;
  }

  for (unsigned int i = 0; i < ArrayLength(Invalid8Strings); ++i) {
    nsrefcnt count = NS_GetNumberOfAtoms();

    {
      nsCOMPtr<nsIAtom> atom8 = do_GetAtom(Invalid8Strings[i].m8);
      nsCOMPtr<nsIAtom> atom16 = do_GetAtom(Invalid8Strings[i].m16);
      if (atom16 != atom8 ||
          !atom16->Equals(nsDependentString(Invalid8Strings[i].m16)))
        return false;
    }
    
    if (count != NS_GetNumberOfAtoms())
      return false;
  }

// Don't run this test in debug builds as that intentionally asserts.
#ifndef DEBUG
  nsCOMPtr<nsIAtom> emptyAtom = do_GetAtom("");

  for (unsigned int i = 0; i < ArrayLength(Malformed8Strings); ++i) {
    nsrefcnt count = NS_GetNumberOfAtoms();

    nsCOMPtr<nsIAtom> atom8 = do_GetAtom(Malformed8Strings[i]);
    if (atom8 != emptyAtom ||
        count != NS_GetNumberOfAtoms())
      return false;
  }
#endif

  return true;
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
    return false;

  if (!thirdNonPerm || NS_GetNumberOfAtoms() != count + 1)
    return false;

  NS_RegisterStaticAtoms(sAtoms_info);

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
      return false;
  
    nsCOMPtr<nsIAtom> first_p =
      NS_NewPermanentAtom(NS_LITERAL_STRING(FIRST_PERM_ATOM_STR));
    if (!first_p->Equals(NS_LITERAL_STRING(FIRST_PERM_ATOM_STR)) ||
        !isStaticAtom(first_p) ||
        first != first_p)
      return false;
  
    nsCOMPtr<nsIAtom> second_p =
      NS_NewPermanentAtom(NS_LITERAL_STRING(SECOND_PERM_ATOM_STR));
    if (!second_p->Equals(NS_LITERAL_STRING(SECOND_PERM_ATOM_STR)) ||
        !isStaticAtom(second_p))
      return false;
  
    nsCOMPtr<nsIAtom> second = do_GetAtom(SECOND_PERM_ATOM_STR);
    if (!second->Equals(NS_LITERAL_STRING(SECOND_PERM_ATOM_STR)) ||
        !isStaticAtom(second) ||
        second != second_p)
      return false;
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
    { nullptr, nullptr }
  };

}

using namespace TestAtoms;

int main()
  {
    {
      nsCOMPtr<nsIServiceManager> servMan;
      NS_InitXPCOM2(getter_AddRefs(servMan), nullptr, nullptr);
  
      for (const Test* t = tests; t->name != nullptr; ++t)
        {
          printf("%25s : %s\n", t->name, t->func() ? "SUCCESS" : "FAILURE <--");
        }
    }

    NS_ShutdownXPCOM(nullptr);

    return 0;
  }

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ArrayUtils.h"

#include "nsAtom.h"
#include "nsString.h"
#include "UTFStrings.h"
#include "nsIServiceManager.h"
#include "nsStaticAtom.h"
#include "nsThreadUtils.h"

#include "gtest/gtest.h"

using namespace mozilla;

int32_t NS_GetUnusedAtomCount(void);

namespace TestAtoms {

TEST(Atoms, Basic)
{
  for (unsigned int i = 0; i < ArrayLength(ValidStrings); ++i) {
    nsDependentString str16(ValidStrings[i].m16);
    nsDependentCString str8(ValidStrings[i].m8);

    RefPtr<nsAtom> atom = NS_Atomize(str16);

    EXPECT_TRUE(atom->Equals(str16));

    nsString tmp16;
    nsCString tmp8;
    atom->ToString(tmp16);
    atom->ToUTF8String(tmp8);
    EXPECT_TRUE(str16.Equals(tmp16));
    EXPECT_TRUE(str8.Equals(tmp8));

    EXPECT_TRUE(nsDependentString(atom->GetUTF16String()).Equals(str16));

    EXPECT_TRUE(nsAtomString(atom).Equals(str16));
    EXPECT_TRUE(nsDependentAtomString(atom).Equals(str16));
    EXPECT_TRUE(nsAtomCString(atom).Equals(str8));
  }
}

TEST(Atoms, 16vs8)
{
  for (unsigned int i = 0; i < ArrayLength(ValidStrings); ++i) {
    RefPtr<nsAtom> atom16 = NS_Atomize(ValidStrings[i].m16);
    RefPtr<nsAtom> atom8 = NS_Atomize(ValidStrings[i].m8);
    EXPECT_EQ(atom16, atom8);
  }
}

TEST(Atoms, BufferSharing)
{
  nsString unique;
  unique.AssignLiteral("this is a unique string !@#$");

  RefPtr<nsAtom> atom = NS_Atomize(unique);

  EXPECT_EQ(unique.get(), atom->GetUTF16String());
}

TEST(Atoms, Null)
{
  nsAutoString str(NS_LITERAL_STRING("string with a \0 char"));
  nsDependentString strCut(str.get());

  EXPECT_FALSE(str.Equals(strCut));

  RefPtr<nsAtom> atomCut = NS_Atomize(strCut);
  RefPtr<nsAtom> atom = NS_Atomize(str);

  EXPECT_EQ(atom->GetLength(), str.Length());
  EXPECT_TRUE(atom->Equals(str));
  EXPECT_NE(atom, atomCut);
  EXPECT_TRUE(atomCut->Equals(strCut));
}

TEST(Atoms, Invalid)
{
  for (unsigned int i = 0; i < ArrayLength(Invalid16Strings); ++i) {
    nsrefcnt count = NS_GetNumberOfAtoms();

    {
      RefPtr<nsAtom> atom16 = NS_Atomize(Invalid16Strings[i].m16);
      EXPECT_TRUE(atom16->Equals(nsDependentString(Invalid16Strings[i].m16)));
    }

    EXPECT_EQ(count, NS_GetNumberOfAtoms());
  }

  for (unsigned int i = 0; i < ArrayLength(Invalid8Strings); ++i) {
    nsrefcnt count = NS_GetNumberOfAtoms();

    {
      RefPtr<nsAtom> atom8 = NS_Atomize(Invalid8Strings[i].m8);
      RefPtr<nsAtom> atom16 = NS_Atomize(Invalid8Strings[i].m16);
      EXPECT_EQ(atom16, atom8);
      EXPECT_TRUE(atom16->Equals(nsDependentString(Invalid8Strings[i].m16)));
    }

    EXPECT_EQ(count, NS_GetNumberOfAtoms());
  }

// Don't run this test in debug builds as that intentionally asserts.
#ifndef DEBUG
  RefPtr<nsAtom> emptyAtom = NS_Atomize("");

  for (unsigned int i = 0; i < ArrayLength(Malformed8Strings); ++i) {
    nsrefcnt count = NS_GetNumberOfAtoms();

    RefPtr<nsAtom> atom8 = NS_Atomize(Malformed8Strings[i]);
    EXPECT_EQ(atom8, emptyAtom);
    EXPECT_EQ(count, NS_GetNumberOfAtoms());
  }
#endif
}

#define FIRST_ATOM_STR "first static atom. Hello!"
#define SECOND_ATOM_STR "second static atom. @World!"
#define THIRD_ATOM_STR "third static atom?!"

bool
isStaticAtom(nsAtom* atom)
{
  // Don't use logic && in order to ensure that all addrefs/releases are always
  // run, even if one of the tests fail. This allows us to run this code on a
  // non-static atom without affecting its refcount.
  bool rv = (atom->AddRef() == 2);
  rv &= (atom->AddRef() == 2);
  rv &= (atom->AddRef() == 2);

  rv &= (atom->Release() == 1);
  rv &= (atom->Release() == 1);
  rv &= (atom->Release() == 1);
  return rv;
}

TEST(Atoms, Table)
{
  nsrefcnt count = NS_GetNumberOfAtoms();

  RefPtr<nsAtom> thirdDynamic = NS_Atomize(THIRD_ATOM_STR);

  EXPECT_FALSE(isStaticAtom(thirdDynamic));

  EXPECT_TRUE(thirdDynamic);
  EXPECT_EQ(NS_GetNumberOfAtoms(), count + 1);
}

class nsAtomRunner final : public nsIRunnable
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS

  NS_IMETHOD Run() final override
  {
    for (int i = 0; i < 10000; i++) {
      RefPtr<nsAtom> atom = NS_Atomize(u"A Testing Atom");
    }
    return NS_OK;
  }

private:
  ~nsAtomRunner() {}
};

NS_IMPL_ISUPPORTS(nsAtomRunner, nsIRunnable)

TEST(Atoms, ConcurrentAccessing)
{
  static const size_t kThreadCount = 4;
  // Force a GC before so that we don't have any unused atom.
  NS_GetNumberOfAtoms();
  EXPECT_EQ(NS_GetUnusedAtomCount(), int32_t(0));
  nsCOMPtr<nsIThread> threads[kThreadCount];
  for (size_t i = 0; i < kThreadCount; i++) {
    nsresult rv = NS_NewThread(getter_AddRefs(threads[i]), new nsAtomRunner);
    EXPECT_TRUE(NS_SUCCEEDED(rv));
  }
  for (size_t i = 0; i < kThreadCount; i++) {
    threads[i]->Shutdown();
  }
  // We should have one unused atom from this test.
  EXPECT_EQ(NS_GetUnusedAtomCount(), int32_t(1));
}

}

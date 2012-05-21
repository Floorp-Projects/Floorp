/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TestHarness.h"
#include "TestingAtoms.cpp"
#include "MoreTestingAtoms.cpp"

int main(int argc, char** argv)
{
  ScopedXPCOM xpcom("TestStaticAtoms");
  if (xpcom.failed()) {
    return 1;
  }

  TestingAtoms::AddRefAtoms();

  NS_SealStaticAtomTable();

  nsCOMPtr<nsIAtom> atom = do_GetAtom("foo");
  if (!atom) {
    fail("Didn't get an atom for foo.");
    return 1;
  }
  if (atom->IsStaticAtom()) {
    passed("foo is a static atom");
  } else {
    fail("foo is not a static atom.");
    return 1;
  }
  if (atom == TestingAtoms::foo) {
    passed("foo is the right pointer");
  } else {
    fail("foo was not the right pointer");
    return 1;
  }
  nsIAtom* staticAtom = NS_GetStaticAtom(NS_LITERAL_STRING("foo"));
  if (!staticAtom) {
    fail("Did not get a static atom for foo");
    return 1;
  }

  if (atom == staticAtom) {
    passed("do_GetAtom and NS_GetStaticAtom returned the same atom.");
  } else {
    fail("do_GetAtom and NS_GetStaticAtom returned different atoms.");
    return 1;
  }

  MoreTestingAtoms::AddRefAtoms();
  
  atom = do_GetAtom("qux");
  if (!atom) {
    fail("Didn't get an atom for qux.");
    return 1;
  }
  if (atom->IsStaticAtom()) {
    passed("qux is a static atom");
  } else {
    fail("qux is not a static atom.");
    return 1;
  }
  if (atom == MoreTestingAtoms::qux) {
    passed("qux is the right pointer");
  } else {
    fail("qux was not the right pointer");
    return 1;
  }
  staticAtom = NS_GetStaticAtom(NS_LITERAL_STRING("qux"));
  if (staticAtom) {
    fail("Got an atom for qux. The static atom table was not sealed properly.");
    return 1;
  }
  return 0;
}

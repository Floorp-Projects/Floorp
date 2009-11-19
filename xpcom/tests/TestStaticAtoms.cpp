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
 * The Original Code is HTML Parser C++ Translator code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Henri Sivonen <hsivonen@iki.fi>
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

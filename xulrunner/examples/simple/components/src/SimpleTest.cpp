/* vim:set ts=2 sw=2 sts=2 et cin: */
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
 * The Initial Developer of the Original Code is Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Darin Fisher <darin@meer.net>
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

#include <stdio.h>
#include "nsISimpleTest.h"
#include "nsIGenericFactory.h"

class SimpleTest : public nsISimpleTest
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSISIMPLETEST
};

NS_IMPL_ISUPPORTS1(SimpleTest, nsISimpleTest)

NS_IMETHODIMP
SimpleTest::Add(PRInt32 a, PRInt32 b, PRInt32 *r)
{
  printf("add(%d,%d) from C++\n", a, b);

  *r = a + b;
  return NS_OK;
}

NS_GENERIC_FACTORY_CONSTRUCTOR(SimpleTest)

// 5e14b432-37b6-4377-923b-c987418d8429
#define SIMPLETEST_CID \
  { 0x5e14b432, 0x37b6, 0x4377, \
    { 0x92, 0x3b, 0xc9, 0x87, 0x41, 0x8d, 0x84, 0x29 } }

static const nsModuleComponentInfo components[] =
{
  { "SimpleTest in C++",
    SIMPLETEST_CID,
    "@test.mozilla.org/simple-test;1?impl=c++",
    SimpleTestConstructor
  }
};

NS_IMPL_NSGETMODULE(SimpleTestModule, components)

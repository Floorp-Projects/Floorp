/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 *   Suresh Duddi <dp@netscape.com>
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

#ifndef __TestFactory_h
#define __TestFactory_h

#include "nsIFactory.h"

// {8B330F20-A24A-11d1-A961-00805F8A7AC4}
#define NS_TESTFACTORY_CID    \
{ 0x8b330f20, 0xa24a, 0x11d1, \
  { 0xa9, 0x61, 0x0, 0x80, 0x5f, 0x8a, 0x7a, 0xc4 } }

// {8B330F21-A24A-11d1-A961-00805F8A7AC4}
#define NS_ITESTCLASS_IID     \
{ 0x8b330f21, 0xa24a, 0x11d1, \
  { 0xa9, 0x61, 0x0, 0x80, 0x5f, 0x8a, 0x7a, 0xc4 } }

// {8B330F22-A24A-11d1-A961-00805F8A7AC4}
#define NS_TESTLOADEDFACTORY_CID \
{ 0x8b330f22, 0xa24a, 0x11d1,    \
  { 0xa9, 0x61, 0x0, 0x80, 0x5f, 0x8a, 0x7a, 0xc4 } }

#define NS_TESTLOADEDFACTORY_CONTRACTID "@mozilla.org/xpcom/dynamic-test;1"

class ITestClass: public nsISupports {
public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_ITESTCLASS_IID)
  virtual void Test() = 0;
};

extern "C" void RegisterTestFactories();

#endif

/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

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

class ITestClass: public nsISupports {
public:
  virtual void Test() = 0;
};

extern "C" void RegisterTestFactories();

#endif

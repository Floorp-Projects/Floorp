/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
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

#ifndef __FACTORY_H
#define __FACTORY_H
#include <windows.h>
#include <ole2.h>
#include "ngprefs.h"

class CClassFactory : public IClassFactory
{
private:
   DWORD m_ObjRefCount;
   CLSID m_clsidObject;

public:
   CClassFactory(CLSID);
   ~CClassFactory();

   //IUnknown methods
   STDMETHOD (QueryInterface)(REFIID, LPVOID*);
   STDMETHOD_(DWORD, AddRef)();
   STDMETHOD_(DWORD, Release)();

   //IClassFactory methods
   STDMETHOD (CreateInstance)(LPUNKNOWN, REFIID, LPVOID*);
   STDMETHOD (LockServer)(BOOL);
};

#endif   // __FACTORY_H

/* -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; -*- */
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

#include "nsAppShell.h"
#include <windows.h>

//-------------------------------------------------------------------------
//
// Create the application shell
//
//-------------------------------------------------------------------------

void nsAppShell::Create()
{
}

//-------------------------------------------------------------------------
//
// Enter a message handler loop
//
//-------------------------------------------------------------------------

nsresult nsAppShell::Run()
{
    // Process messages
  MSG msg;
  while (GetMessage(&msg, NULL, 0, 0)) {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
  }
  return msg.wParam;
}

//-------------------------------------------------------------------------
//
// Exit a message handler loop
//
//-------------------------------------------------------------------------

void nsAppShell::Exit()
{
  PostQuitMessage(0);
}

//-------------------------------------------------------------------------
//
// nsAppShell constructor
//
//-------------------------------------------------------------------------
nsAppShell::nsAppShell(nsISupports *aOuter) : nsObject(aOuter)  
{ 
}

//-------------------------------------------------------------------------
//
// nsAppShell destructor
//
//-------------------------------------------------------------------------
nsAppShell::~nsAppShell()
{
}

//-------------------------------------------------------------------------
//
// Query interface implementation
//
//-------------------------------------------------------------------------
nsresult nsAppShell::QueryObject(const nsIID& aIID, void** aInstancePtr)
{
    nsresult result = NS_NOINTERFACE;
    static NS_DEFINE_IID(kInsAppShellIID, NS_IAPPSHELL_IID);
    if (result == NS_NOINTERFACE && aIID.Equals(kInsAppShellIID)) {
        *aInstancePtr = (void*) ((nsIAppShell*)this);
        AddRef();
        result = NS_OK;
    }

    return result;
}

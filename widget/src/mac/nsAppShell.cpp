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
#include "nsIAppShell.h"
#include "nsWindow.h"
#include <stdlib.h>
#include "nsMacMessagePump.h"

//XtAppContext gAppContext;

//-------------------------------------------------------------------------
//
// nsISupports implementation macro
//
//-------------------------------------------------------------------------
NS_DEFINE_IID(kIAppShellIID, NS_IAPPSHELL_IID);
NS_IMPL_ISUPPORTS(nsAppShell,kIAppShellIID);

void nsAppShell::SetDispatchListener(nsDispatchListener* aDispatchListener)
{
  mDispatchListener = aDispatchListener;
}

//-------------------------------------------------------------------------
//
// Create the application shell
//
//-------------------------------------------------------------------------

void nsAppShell::Create(int* argc, char ** argv)
{
		mToolKit = new nsToolkit();
}

//-------------------------------------------------------------------------
//
// Enter a message handler loop
//
//-------------------------------------------------------------------------
nsresult nsAppShell::Run()
{
	mMacPump = new nsMacMessagePump(mToolKit);
		mMacPump->DoMessagePump();
	delete mMacPump;
	mMacPump = nsnull;


    //if (mDispatchListener)
      //mDispatchListener->AfterDispatch();

  return NS_OK;
}

//-------------------------------------------------------------------------
//
// nsAppShell constructor
//
//-------------------------------------------------------------------------
void nsAppShell::Exit()
{
	if (mMacPump)
		mMacPump->StopRunning();
}

//-------------------------------------------------------------------------
//
// nsAppShell constructor
//
//-------------------------------------------------------------------------
nsAppShell::nsAppShell()
{ 
  mRefCnt = 0;
  mDispatchListener = 0;
  mMacPump = nsnull;
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
// GetNativeData
//
//-------------------------------------------------------------------------
void* nsAppShell::GetNativeData(PRUint32 aDataType)
{
  if (aDataType == NS_NATIVE_SHELL) 
  	{
    //return mTopLevel;
  	}
  return nsnull;
}



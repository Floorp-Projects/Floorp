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
#include <stdlib.h>

//-------------------------------------------------------------------------
//
// nsISupports implementation macro
//
//-------------------------------------------------------------------------
NS_DEFINE_IID(kIAppShellIID, NS_IAPPSHELL_IID);
NS_IMPL_ISUPPORTS(nsAppShell,kIAppShellIID);

//-------------------------------------------------------------------------
NS_METHOD nsAppShell::SetDispatchListener(nsDispatchListener* aDispatchListener)
{
  mDispatchListener = aDispatchListener;

  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Create the application shell
//
//-------------------------------------------------------------------------

NS_METHOD nsAppShell::Create(int* argc, char ** argv)
{
  gtk_set_locale ();

  gtk_init (argc, &argv);

  gdk_rgb_init ();
 
  // Windows and Mac don't create anything here, so why should we?
//  mTopLevel = gtk_window_new (GTK_WINDOW_TOPLEVEL);
/* we should probibly set even handlers here */
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Enter a message handler loop
//
//-------------------------------------------------------------------------

NS_METHOD nsAppShell::Run()
{

  gtk_main();

  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Exit a message handler loop
//
//-------------------------------------------------------------------------

NS_METHOD nsAppShell::Exit()
{
//  gtk_widget_destroy (mTopLevel);
  gtk_main_quit ();
 
  return NS_OK;
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
  if (aDataType == NS_NATIVE_SHELL) {
//    return mTopLevel;
  }
  return nsnull;
}



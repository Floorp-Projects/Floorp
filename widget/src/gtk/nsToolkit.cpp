/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#include "nsToolkit.h"
#include "nsGUIEvent.h"

//-------------------------------------------------------------------------
//
// constructor
//
//-------------------------------------------------------------------------
nsToolkit::nsToolkit()
{
  NS_INIT_REFCNT();
  /* XSetErrorHandler(nsToolkitErrorHandler); */
}

//-------------------------------------------------------------------------
//
// destructor
//
//-------------------------------------------------------------------------
nsToolkit::~nsToolkit()
{
}


void RunPump(void* arg)
{
}

//-------------------------------------------------------------------------
//
// constructor
//
//-------------------------------------------------------------------------
//int nsToolkitErrorHandler (Display * mydisplay, XErrorEvent * myerr)
//{
//  char msg[80] ;

//  XGetErrorText (mydisplay, myerr->error_code, msg, 80) ;
//  fprintf (stderr, "-------------------------------------\n");
//  fprintf (stderr, "Error code %s\n", msg) ;
//  fprintf (stderr, "-------------------------------------\n");
  //exit() ;
//  return 0;
//}


//-------------------------------------------------------------------------
//
// nsISupports implementation macro
//
//-------------------------------------------------------------------------
NS_DEFINE_IID(kIToolkitIID, NS_ITOOLKIT_IID);
NS_IMPL_ISUPPORTS(nsToolkit,kIToolkitIID);



//-------------------------------------------------------------------------
//
//
//-------------------------------------------------------------------------
NS_METHOD nsToolkit::Init(PRThread *aThread)
{
  mSharedGC = nsnull;
  return NS_OK;
}

void nsToolkit::SetSharedGC(GdkGC *aGC)
{
  mSharedGC = aGC;
}

GdkGC *nsToolkit::GetSharedGC()
{
  return (mSharedGC);
}

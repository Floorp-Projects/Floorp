/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "nsToolkit.h"
#include <stdio.h>

//-------------------------------------------------------------------------
//
// Error handler
//
//-------------------------------------------------------------------------
int nsToolkitErrorHandler (Display * mydisplay, XErrorEvent * myerr) 
{ 
  char msg[80] ; 

  XGetErrorText (mydisplay, myerr->error_code, msg, 80) ; 
  fprintf (stderr, "-------------------------------------\n");
  fprintf (stderr, "Error code %s\n", msg) ; 
  fprintf (stderr, "-------------------------------------\n");
  //exit() ;
  return 0;
} 
  

//-------------------------------------------------------------------------
//
// constructor
//
//-------------------------------------------------------------------------
nsToolkit::nsToolkit() 
{
  NS_INIT_REFCNT();
  XSetErrorHandler(nsToolkitErrorHandler); 
}

//-------------------------------------------------------------------------
//
// destructor
//
//-------------------------------------------------------------------------
nsToolkit::~nsToolkit()
{
}


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
  return NS_OK;
}

void nsToolkit::SetSharedGC(GC aGC)
{
  mSharedGC = aGC;
}

GC nsToolkit::GetSharedGC()
{
  return (mSharedGC);
}

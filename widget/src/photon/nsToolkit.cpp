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
#include "nsWindow.h"
#include "prmon.h"
#include "prtime.h"
#include "nsGUIEvent.h"
#include <Pt.h>
#include "nsPhWidgetLog.h"


NS_DEFINE_IID(kIToolkitIID,  NS_ITOOLKIT_IID);
NS_IMPL_ISUPPORTS(nsToolkit,kIToolkitIID);

//-------------------------------------------------------------------------
//
// constructor
//
//-------------------------------------------------------------------------
nsToolkit::nsToolkit()  
{
  NS_INIT_REFCNT();
  mWidget = NULL;
}


//-------------------------------------------------------------------------
//
// destructor
//
//-------------------------------------------------------------------------
nsToolkit::~nsToolkit()
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsToolkit::~nsToolkit\n"));
}


//-------------------------------------------------------------------------
//
//
//-------------------------------------------------------------------------
NS_METHOD nsToolkit::Init(PRThread *aThread)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsToolkit::Init this=<%p> aThread=<%p>\n", this, aThread));
  if( aThread )
  {  
    /* No idea what this is for */
  }
  else
  {
    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsToolkit::Init - ERROR: Wants to create a thread!\n"));
  }

  return NS_OK;
}

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
#include "nscore.h"

#include "nsPhWidgetLog.h"

/* Set our static member to NULL */
PhDrawContext_t * nsToolkit::mDefaultPhotonDrawContext = nsnull;

NS_IMPL_ISUPPORTS1(nsToolkit,nsIToolkit);

//-------------------------------------------------------------------------
//
// constructor
//
//-------------------------------------------------------------------------
nsToolkit::nsToolkit()  
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsToolkit::nsToolkit this=<%p>\n", this));

  NS_INIT_REFCNT();
  
  if (mDefaultPhotonDrawContext == nsnull)
  {
    mDefaultPhotonDrawContext = PhDCGetCurrent();  
  }
}


//-------------------------------------------------------------------------
//
// destructor
//
//-------------------------------------------------------------------------
nsToolkit::~nsToolkit()
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsToolkit::~nsToolkit this=<%p>\n", this));
}


//-------------------------------------------------------------------------
//
//
//-------------------------------------------------------------------------
NS_METHOD nsToolkit::Init(PRThread *aThread)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsToolkit::Init this=<%p> aThread=<%p>\n", this, aThread));
  return NS_OK;
}

PhDrawContext_t *nsToolkit::GetDefaultPhotonDrawContext()
{
  if (mDefaultPhotonDrawContext == NULL)
  {
    NS_ASSERTION(mDefaultPhotonDrawContext, "nsToolkit::GetDefaultPhotonDrawContext mDefaultPhotonDrawContext is NULL");
    abort();  
  }

  return mDefaultPhotonDrawContext;
}
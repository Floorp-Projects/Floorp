/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#include "nscore.h"  // needed for 'nsnull'
#include "nsToolkit.h"
#include "nsGUIEvent.h"
#include "plevent.h"
#include "nsWidget.h"

//
// Static thread local storage index of the Toolkit
// object associated with a given thread...
//
static PRUintn gToolkitTLSIndex = 0;

//-------------------------------------------------------------------------
//
// constructor
//
//-------------------------------------------------------------------------
nsToolkit::nsToolkit()
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsToolkit::nsToolkit()\n"));
    NS_INIT_REFCNT();
}

//-------------------------------------------------------------------------
//
// destructor
//
//-------------------------------------------------------------------------
nsToolkit::~nsToolkit()
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsToolkit::~nsToolkit()\n"));

    // Remove the TLS reference to the toolkit...
    PR_SetThreadPrivate(gToolkitTLSIndex, nsnull); 
}

//-------------------------------------------------------------------------
//
// nsISupports implementation macro
//
//-------------------------------------------------------------------------
NS_IMPL_ISUPPORTS1(nsToolkit, nsIToolkit)   
 
//-------------------------------------------------------------------------
//
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsToolkit::Init(PRThread *aThread)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsToolkit::Init()\n"));
    return NS_OK;
}

//-------------------------------------------------------------------------
//
// Return the nsIToolkit for the current thread.  If a toolkit does not
// yet exist, then one will be created...
//
//-------------------------------------------------------------------------
NS_METHOD NS_GetCurrentToolkit(nsIToolkit* *aResult)
{
  nsIToolkit* toolkit = nsnull;
  nsresult rv = NS_OK;
  PRStatus status;
 
  // Create the TLS index the first time through...
  if (0 == gToolkitTLSIndex)
  {
    status = PR_NewThreadPrivateIndex(&gToolkitTLSIndex, NULL);
    if (PR_FAILURE == status)
    {
      rv = NS_ERROR_FAILURE;
    }
  }
 
  if (NS_SUCCEEDED(rv))
  {
    toolkit = (nsIToolkit*)PR_GetThreadPrivate(gToolkitTLSIndex);
 
    //
    // Create a new toolkit for this thread...
    //
    if (!toolkit)
    {
      toolkit = new nsToolkit();
 
      if (!toolkit)
      {
        rv = NS_ERROR_OUT_OF_MEMORY;
      }
      else
      {
        NS_ADDREF(toolkit);
        toolkit->Init(PR_GetCurrentThread());
        //
        // The reference stored in the TLS is weak.  It is removed in the
        // nsToolkit destructor...
        //
        PR_SetThreadPrivate(gToolkitTLSIndex, (void*)toolkit);
       }
    }
    else
    {
      NS_ADDREF(toolkit);
    }
    *aResult = toolkit;
  }
  return rv;
}
 

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
#include "nsGUIEvent.h"
#include "plevent.h"

// Static Thread Local Storage index of the toolkit object associated with
// a given thread...

static PRUintn gToolkitTLSIndex = 0;

nsToolkit::nsToolkit()
{
  NS_INIT_REFCNT();
}

nsToolkit::~nsToolkit()
{
}

NS_DEFINE_IID(kIToolkitIID, NS_ITOOLKIT_IID);
NS_IMPL_ISUPPORTS(nsToolkit,kIToolkitIID);

NS_METHOD nsToolkit::Init(PRThread *aThread)
{
  return NS_OK;
}

NS_METHOD NS_GetCurrentToolkit(nsIToolkit* *aResult)
{
  nsIToolkit* toolkit = nsnull;
  nsresult rv = NS_OK;
  PRStatus status;

  // Create the TLS (Thread Local Storage) index the first time through
  if (gToolkitTLSIndex == 0)
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

    // Create a new toolkit for this thread
    if (!toolkit)
    {
      fprintf(stderr, "Creating a new nsIToolkit!\n");
    }
    else
      fprintf(stderr, "No need to create a new nsIToolkit!\n");
  }

  return NS_OK;
}

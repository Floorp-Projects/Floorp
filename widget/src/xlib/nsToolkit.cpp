/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim:ts=2:et:sw=2
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

#include "nscore.h" // needed for 'nsnull'
#include "xlibrgb.h"
#include "nsToolkit.h"
#include "nsGCCache.h"
#include "nsAppShell.h" // needed for nsAppShell::GetXlibRgbHandle()

// Static Thread Local Storage index of the toolkit object associated with
// a given thread...
static PRUintn gToolkitTLSIndex = 0;

nsToolkit::nsToolkit()
{
  NS_INIT_REFCNT();
  mGC = nsnull;
  mDisplay = xxlib_rgb_get_display(nsAppShell::GetXlibRgbHandle());
}

nsToolkit::~nsToolkit()
{
  if (mGC)
    mGC->Release();

  // Remove the TLS reference to the toolkit...
  PR_SetThreadPrivate(gToolkitTLSIndex, nsnull);
}

NS_IMPL_ISUPPORTS1(nsToolkit, nsIToolkit)

void nsToolkit::CreateSharedGC()
{
  if (mGC)
    return;

  mGC = new xGC(mDisplay, XDefaultRootWindow(mDisplay), 0, nsnull);
  mGC->AddRef(); // this is for us
}

xGC *nsToolkit::GetSharedGC()
{
  if (!mGC)
    CreateSharedGC();

  mGC->AddRef();
  return mGC;
}

NS_METHOD nsToolkit::Init(PRThread *aThread)
{
  CreateSharedGC();

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
    status = PR_NewThreadPrivateIndex(&gToolkitTLSIndex, nsnull);
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
    if (!toolkit) {
      toolkit = new nsToolkit();
      
      if (!toolkit) {
        rv = NS_ERROR_OUT_OF_MEMORY;
      } else {
        NS_ADDREF(toolkit);
        toolkit->Init(PR_GetCurrentThread());
        //
        // The reference stored in the TLS is weak.  It is removed in the
        // nsToolkit destructor...
        //
        PR_SetThreadPrivate(gToolkitTLSIndex, (void*)toolkit);
      }
    } else {
      NS_ADDREF(toolkit);
    }
    *aResult = toolkit;
  }
  return rv;
}

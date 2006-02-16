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
 *   John Fairhurst <john_fairhurst@iname.com>
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *   IBM Corp.
 */

#include "nsDeviceContextSpecFactoryO.h"
#include "nsDeviceContextSpecOS2.h"
#define INCL_DEV
#include <os2.h>
#include "libprint.h"
#include "nsGfxCIID.h"

nsDeviceContextSpecFactoryOS2 :: nsDeviceContextSpecFactoryOS2()
{
  NS_INIT_REFCNT();
}

nsDeviceContextSpecFactoryOS2 :: ~nsDeviceContextSpecFactoryOS2()
{
}

static NS_DEFINE_IID(kDeviceContextSpecFactoryIID, NS_IDEVICE_CONTEXT_SPEC_FACTORY_IID);
static NS_DEFINE_IID(kIDeviceContextSpecIID, NS_IDEVICE_CONTEXT_SPEC_IID);
static NS_DEFINE_IID(kDeviceContextSpecCID, NS_DEVICE_CONTEXT_SPEC_CID);

NS_IMPL_QUERY_INTERFACE(nsDeviceContextSpecFactoryOS2, kDeviceContextSpecFactoryIID)
NS_IMPL_ADDREF(nsDeviceContextSpecFactoryOS2)
NS_IMPL_RELEASE(nsDeviceContextSpecFactoryOS2)

NS_IMETHODIMP nsDeviceContextSpecFactoryOS2 :: Init(void)
{
  return NS_OK;
}

//XXX this method needs to do what the API says...

NS_IMETHODIMP nsDeviceContextSpecFactoryOS2 :: CreateDeviceContextSpec(nsIDeviceContextSpec *aOldSpec,
                                                                       nsIDeviceContextSpec *&aNewSpec,
                                                                       PRBool aQuiet)
{
  nsresult  rv = NS_ERROR_FAILURE;

  PRTQUEUE *pq = PrnSelectPrinter( HWND_DESKTOP, aQuiet ? TRUE : FALSE);

  if( pq)
  {
    nsIDeviceContextSpec  *devspec = nsnull;

    nsComponentManager::CreateInstance(kDeviceContextSpecCID, nsnull, kIDeviceContextSpecIID, (void **)&devspec);

    if (nsnull != devspec)
    {
      //XXX need to QI rather than cast... MMP
      if (NS_OK == ((nsDeviceContextSpecOS2 *)devspec)->Init(pq))
      {
        aNewSpec = devspec;
        rv = NS_OK;
      }
    }
  }

  return rv;
}

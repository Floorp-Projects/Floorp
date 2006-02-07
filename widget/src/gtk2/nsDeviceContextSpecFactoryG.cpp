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
 * Copyright (C) 1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *   Roland Mainz <roland.mainz@informatik.med.uni-giessen.de>
 */

#include "nsDeviceContextSpecFactoryG.h"
#include "nsDeviceContextSpecG.h"
#include "nsGfxCIID.h"
#include "plstr.h"

/** -------------------------------------------------------
 *  Constructor
 *  @update   dc 2/16/98
 */
nsDeviceContextSpecFactoryGTK :: nsDeviceContextSpecFactoryGTK()
{
  NS_INIT_REFCNT();
}

/** -------------------------------------------------------
 *  Destructor
 *  @update   dc 2/16/98
 */
nsDeviceContextSpecFactoryGTK :: ~nsDeviceContextSpecFactoryGTK()
{
}

NS_IMPL_ISUPPORTS1(nsDeviceContextSpecFactoryGTK, nsIDeviceContextSpecFactory)

/** -------------------------------------------------------
 *  Initialize the device context spec factory
 *  @update   dc 2/16/98
 */
NS_IMETHODIMP nsDeviceContextSpecFactoryGTK :: Init(void)
{
  return NS_OK;
}

/** -------------------------------------------------------
 *  Get a device context specification
 *  @update   dc 2/16/98
 */
NS_IMETHODIMP nsDeviceContextSpecFactoryGTK :: CreateDeviceContextSpec(nsIWidget *aWidget,
                                                                       nsIDeviceContextSpec *&aNewSpec,
                                                                       PRBool aQuiet)
{
  nsresult rv;
  static NS_DEFINE_CID(kDeviceContextSpecCID, NS_DEVICE_CONTEXT_SPEC_CID);
  nsCOMPtr<nsIDeviceContextSpec> devSpec = do_CreateInstance(kDeviceContextSpecCID, &rv);
  if (NS_SUCCEEDED(rv))
  {
    rv = ((nsDeviceContextSpecGTK *)devSpec.get())->Init(aQuiet);
    if (NS_SUCCEEDED(rv))
    {
      aNewSpec = devSpec;
      NS_ADDREF(aNewSpec);
    }
  }

  return rv;  
}


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

#include "nsDeviceContextSpecFactoryW.h"
#include "nsDeviceContextSpecWin.h"
#include <windows.h>
#include <commdlg.h>
#include "nsGfxCIID.h"
#include "plstr.h"

nsDeviceContextSpecFactoryWin :: nsDeviceContextSpecFactoryWin()
{
  NS_INIT_REFCNT();
}

nsDeviceContextSpecFactoryWin :: ~nsDeviceContextSpecFactoryWin()
{
}

static NS_DEFINE_IID(kIDeviceContextSpecIID, NS_IDEVICE_CONTEXT_SPEC_IID);
static NS_DEFINE_IID(kDeviceContextSpecCID, NS_DEVICE_CONTEXT_SPEC_CID);

NS_IMPL_ISUPPORTS1(nsDeviceContextSpecFactoryWin, nsIDeviceContextSpecFactory)

NS_IMETHODIMP nsDeviceContextSpecFactoryWin :: Init(void)
{
  return NS_OK;
}

//XXX this method needs to do what the API says...

NS_IMETHODIMP nsDeviceContextSpecFactoryWin :: CreateDeviceContextSpec(nsIDeviceContextSpec *aOldSpec,
                                                                       nsIDeviceContextSpec *&aNewSpec,
                                                                       PRBool aQuiet)
{
  PRINTDLG  prntdlg;
  nsresult  rv = NS_ERROR_FAILURE;

  prntdlg.lStructSize = sizeof(prntdlg);
  prntdlg.hwndOwner = NULL;               //XXX need to find a window here. MMP
  prntdlg.hDevMode = NULL;
  prntdlg.hDevNames = NULL;
  prntdlg.hDC = NULL;
  prntdlg.Flags = PD_ALLPAGES | PD_RETURNIC | PD_NOSELECTION | PD_HIDEPRINTTOFILE;
  prntdlg.nFromPage = 0;
  prntdlg.nToPage = 0;
  prntdlg.nMinPage = 0;
  prntdlg.nMaxPage = 0;
  prntdlg.nCopies = 1;
  prntdlg.hInstance = NULL;
  prntdlg.lCustData = 0;
  prntdlg.lpfnPrintHook = NULL;
  prntdlg.lpfnSetupHook = NULL;
  prntdlg.lpPrintTemplateName = NULL;
  prntdlg.lpSetupTemplateName = NULL;
  prntdlg.hPrintTemplate = NULL;
  prntdlg.hSetupTemplate = NULL;


  if(PR_TRUE == aQuiet){
    prntdlg.Flags = PD_RETURNDEFAULT;
  }



  BOOL res = ::PrintDlg(&prntdlg);

  if (TRUE == res)
  {
    DEVNAMES *devnames = (DEVNAMES *)::GlobalLock(prntdlg.hDevNames);

    char device[200], driver[200];

    //print something...

    PL_strcpy(device, &(((char *)devnames)[devnames->wDeviceOffset]));
    PL_strcpy(driver, &(((char *)devnames)[devnames->wDriverOffset]));

#ifdef NS_DEBUG
printf("printer: driver %s, device %s\n", driver, device);
#endif

    nsIDeviceContextSpec  *devspec = nsnull;

    nsComponentManager::CreateInstance(kDeviceContextSpecCID, nsnull, kIDeviceContextSpecIID, (void **)&devspec);

    if (nsnull != devspec)
    {
      //XXX need to QI rather than cast... MMP
      if (NS_OK == ((nsDeviceContextSpecWin *)devspec)->Init(driver, device, prntdlg.hDevMode))
      {
        aNewSpec = devspec;
        rv = NS_OK;
      }
    }

    //don't free the DEVMODE because the device context spec now owns it...
    ::GlobalUnlock(prntdlg.hDevNames);
    ::GlobalFree(prntdlg.hDevNames);
  }

  return rv;
}

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

#include "nsDeviceContextSpecWin.h"
#include "prmem.h"
#include "plstr.h"

nsDeviceContextSpecWin :: nsDeviceContextSpecWin()
{
  NS_INIT_REFCNT();

  mDriverName = nsnull;
  mDeviceName = nsnull;
  mDEVMODE = NULL;
}

nsDeviceContextSpecWin :: ~nsDeviceContextSpecWin()
{
  if (nsnull != mDriverName)
  {
    PR_Free(mDriverName);
    mDriverName = nsnull;
  }

  if (nsnull != mDeviceName)
  {
    PR_Free(mDeviceName);
    mDeviceName = nsnull;
  }

  if (NULL != mDEVMODE)
  {
    ::GlobalFree(mDEVMODE);
    mDEVMODE = NULL;
  }
}

NS_IMPL_ISUPPORTS1(nsDeviceContextSpecWin, nsIDeviceContextSpec)

NS_IMETHODIMP nsDeviceContextSpecWin :: Init(char *aDriverName, char *aDeviceName, HGLOBAL aDEVMODE)
{
  if (nsnull != aDriverName)
  {
    mDriverName = (char *)PR_Malloc(PL_strlen(aDriverName) + 1);
    PL_strcpy(mDriverName, aDriverName);
  }

  if (nsnull != aDeviceName)
  {
    mDeviceName = (char *)PR_Malloc(PL_strlen(aDeviceName) + 1);
    PL_strcpy(mDeviceName, aDeviceName);
  }

  mDEVMODE = aDEVMODE;

  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextSpecWin :: GetDriverName(char *&aDriverName) const
{
  aDriverName = mDriverName;
  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextSpecWin :: GetDeviceName(char *&aDeviceName) const
{
  aDeviceName = mDeviceName;
  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextSpecWin :: GetDEVMODE(HGLOBAL &aDEVMODE) const
{
  aDEVMODE = mDEVMODE;
  return NS_OK;
}

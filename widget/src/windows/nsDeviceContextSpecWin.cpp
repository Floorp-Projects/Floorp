/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "nsDeviceContextSpecWin.h"
#include "prmem.h"
#include "plstr.h"

nsDeviceContextSpecWin :: nsDeviceContextSpecWin()
{
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

static NS_DEFINE_IID(kDeviceContextSpecIID, NS_IDEVICE_CONTEXT_SPEC_IID);

NS_IMPL_QUERY_INTERFACE(nsDeviceContextSpecWin, kDeviceContextSpecIID)
NS_IMPL_ADDREF(nsDeviceContextSpecWin)
NS_IMPL_RELEASE(nsDeviceContextSpecWin)

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

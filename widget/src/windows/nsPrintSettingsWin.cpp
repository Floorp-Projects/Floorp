/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#include "nsPrintSettingsWin.h"
#include "nsCRT.h"

NS_IMPL_ISUPPORTS_INHERITED1(nsPrintSettingsWin, 
                             nsPrintSettings, 
                             nsIPrintSettingsWin)

/** ---------------------------------------------------
 *  See documentation in nsPrintSettingsWin.h
 *	@update 
 */
nsPrintSettingsWin::nsPrintSettingsWin() :
  nsPrintSettings(),
  mDeviceName(nsnull),
  mDriverName(nsnull),
  mDevMode(nsnull)
{

}

/** ---------------------------------------------------
 *  See documentation in nsPrintSettingsWin.h
 *	@update 
 */
nsPrintSettingsWin::~nsPrintSettingsWin()
{
  if (mDeviceName) nsCRT::free(mDeviceName);
  if (mDriverName) nsCRT::free(mDriverName);
  if (mDevMode) free(mDevMode);
}

/* [noscript] attribute charPtr deviceName; */
NS_IMETHODIMP nsPrintSettingsWin::SetDeviceName(char * aDeviceName)
{
  if (mDeviceName) {
    nsCRT::free(mDeviceName);
  }
  mDeviceName = aDeviceName?nsCRT::strdup(aDeviceName):nsnull;
  return NS_OK;
}
NS_IMETHODIMP nsPrintSettingsWin::GetDeviceName(char * *aDeviceName)
{
  NS_ENSURE_ARG_POINTER(aDeviceName);
  *aDeviceName = mDeviceName?nsCRT::strdup(mDeviceName):nsnull;
  return NS_OK;
}

/* [noscript] attribute charPtr driverName; */
NS_IMETHODIMP nsPrintSettingsWin::SetDriverName(char * aDriverName)
{
  if (mDriverName) {
    nsCRT::free(mDriverName);
  }
  mDriverName = aDriverName?nsCRT::strdup(aDriverName):nsnull;
  return NS_OK;
}
NS_IMETHODIMP nsPrintSettingsWin::GetDriverName(char * *aDriverName)
{
  NS_ENSURE_ARG_POINTER(aDriverName);
  *aDriverName = mDriverName?nsCRT::strdup(mDriverName):nsnull;
  return NS_OK;
}

/* [noscript] attribute nsDevMode devMode; */
NS_IMETHODIMP nsPrintSettingsWin::GetDevMode(DEVMODE * *aDevMode)
{
  NS_ENSURE_ARG_POINTER(aDevMode);

  if (mDevMode) {
    size_t size = sizeof(*mDevMode);
    *aDevMode = (LPDEVMODE)malloc(size);
    memcpy(*aDevMode, mDevMode, size);
  } else {
    *aDevMode = nsnull;
  }
  return NS_OK;
}
NS_IMETHODIMP nsPrintSettingsWin::SetDevMode(DEVMODE * aDevMode)
{
  if (mDevMode) {
    free(mDevMode);
    mDevMode = NULL;
  }

  if (aDevMode) {
    size_t size = sizeof(*aDevMode);
    mDevMode = (LPDEVMODE)malloc(size);
    memcpy(mDevMode, aDevMode, size);
  }
  return NS_OK;
}

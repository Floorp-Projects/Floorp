/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Device Support
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Doug Turner <dougt@meer.net>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsXPCOM.h"
#include "nsCOMPtr.h"
#include "nsIServiceManager.h"
#include "nsIGenericFactory.h"

#include "string.h"

#include "nsIDeviceSupport.h"

class nsDeviceSupport : public nsIDeviceSupport
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIDEVICESUPPORT

  nsDeviceSupport();

private:
  ~nsDeviceSupport();

protected:
};

NS_IMPL_ISUPPORTS1(nsDeviceSupport, nsIDeviceSupport)

nsDeviceSupport::nsDeviceSupport()
{
}

nsDeviceSupport::~nsDeviceSupport()
{
}

NS_IMETHODIMP nsDeviceSupport::RotateScreen(PRBool aLandscapeMode)
{
#ifdef WINCE
    DEVMODE DevMode;
    memset (&DevMode, 0, sizeof (DevMode));
    DevMode.dmSize   = sizeof (DevMode);

    DevMode.dmFields = DM_DISPLAYORIENTATION;
    DevMode.dmDisplayOrientation = aLandscapeMode ? DMDO_90 : DMDO_0;
    ChangeDisplaySettingsEx(NULL, &DevMode, NULL, CDS_RESET, NULL);
#endif

    return NS_OK;
}



//------------------------------------------------------------------------------
//  XPCOM REGISTRATION BELOW
//------------------------------------------------------------------------------

#define DeviceSupport_CID \
{  0x6b60b326, 0x483e, 0x47d6, {0x82, 0x87, 0x88, 0x1a, 0xd9, 0x51, 0x0c, 0x8f} }

#define DeviceSupport_ContractID "@mozilla.org/device/support;1"

NS_GENERIC_FACTORY_CONSTRUCTOR(nsDeviceSupport)

static const nsModuleComponentInfo components[] =
{
  { "Device Service", 
    DeviceSupport_CID, 
    DeviceSupport_ContractID,
    nsDeviceSupportConstructor,
  }  
};

NS_IMPL_NSGETMODULE(DeviceSupportModule, components)

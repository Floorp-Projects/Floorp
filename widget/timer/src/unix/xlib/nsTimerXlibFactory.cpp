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
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *   Ken Faulkner <faulkner@igelaus.com.au>
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

#include "nsIGenericFactory.h"
#include "nsIModule.h"
#include "nsCOMPtr.h"

#include "nsTimerXlib.h"

/* Same CID for Xlib as GTK. Originally CID's were in 
 * widget/timer/src/unix/nsUnixTimerCIID.h but wont use this for now.
 * May become an future issue.
 */

// {48B62AD2-48D3-11d3-B224-000064657374}
#define NS_TIMER_XLIB_CID \
{ 0x48b62ad2, 0x48d3, 0x11d3, \
  { 0xb2, 0x24, 0x0, 0x0, 0x64, 0x65, 0x73, 0x74 } }

NS_GENERIC_FACTORY_CONSTRUCTOR(nsTimerXlib)

static nsModuleComponentInfo components[] =
{
  { "XLIB timer",
    NS_TIMER_XLIB_CID,
    "@mozilla.org/timer;1", 
    nsTimerXlibConstructor }
};

PR_STATIC_CALLBACK(void)
nsXlibTimerModuleDtor(nsIModule *self)
{
    nsTimerXlib::Shutdown();
}

NS_IMPL_NSGETMODULE_WITH_DTOR(nsXlibTimerModule,
                              components,
                              nsXlibTimerModuleDtor)

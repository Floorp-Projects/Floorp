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
 * The Original Code is Windows CE Link Monitoring.
 *
 * The Initial Developer of the Original Code is Mozilla Corporation
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Doug Turner <dougt@meer.net>  (Original Author)
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

#include "nsNotifyAddrListener.h"
#include "nsServiceManagerUtils.h"
#include "nsString.h"
#include "nsIObserverService.h"

#include <objbase.h>
#ifdef WINCE_WINDOWS_MOBILE
#include <connmgr.h>
#endif

// pulled from the header so that we do not get multiple define errors during link
static const GUID nal_DestNetInternet =
        { 0x436ef144, 0xb4fb, 0x4863, { 0xa0, 0x41, 0x8f, 0x90, 0x5a, 0x62, 0xc5, 0x72 } };


NS_IMPL_THREADSAFE_ISUPPORTS1(nsNotifyAddrListener,
                              nsINetworkLinkService)

nsNotifyAddrListener::nsNotifyAddrListener()
: mConnectionHandle(NULL)
{
}

nsNotifyAddrListener::~nsNotifyAddrListener()
{
#ifdef WINCE_WINDOWS_MOBILE
  if (mConnectionHandle)
    ConnMgrReleaseConnection(mConnectionHandle, 0);
#endif
}

nsresult
nsNotifyAddrListener::Init(void)
{
#ifdef WINCE_WINDOWS_MOBILE
  CONNMGR_CONNECTIONINFO conn_info;
  memset(&conn_info, 0, sizeof(conn_info));
  
  conn_info.cbSize      = sizeof(conn_info);
  conn_info.dwParams    = CONNMGR_PARAM_GUIDDESTNET;
  conn_info.dwPriority  = CONNMGR_PRIORITY_LOWBKGND;
  conn_info.guidDestNet = nal_DestNetInternet;
  conn_info.bExclusive  = FALSE;
  conn_info.bDisabled   = FALSE;
  
  ConnMgrEstablishConnection(&conn_info, 
                             &mConnectionHandle);

#endif
  return NS_OK;
}

NS_IMETHODIMP
nsNotifyAddrListener::GetIsLinkUp(PRBool *aIsUp)
{
#ifdef WINCE_WINDOWS_MOBILE
  DWORD status;
  HRESULT result = ConnMgrConnectionStatus(mConnectionHandle, &status);
  if (FAILED(result)) {
    *aIsUp = PR_FALSE;
    return NS_ERROR_FAILURE;
  }
  *aIsUp = status == CONNMGR_STATUS_CONNECTED;
#else
  *aIsUp = PR_TRUE;
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsNotifyAddrListener::GetLinkStatusKnown(PRBool *aIsKnown)
{
#ifdef WINCE_WINDOWS_MOBILE
  DWORD status;
  HRESULT result = ConnMgrConnectionStatus(mConnectionHandle, &status);
  if (FAILED(result)) {
    *aIsKnown = PR_FALSE;
    return NS_ERROR_FAILURE;
  }
  *aIsKnown = status != CONNMGR_STATUS_UNKNOWN;
#else
  *aIsKnown = PR_TRUE;
#endif
  return NS_OK;
}

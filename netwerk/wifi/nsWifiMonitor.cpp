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
 * The Original Code is Geolocation.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * This is a derivative of work done by Google under a BSD style License.
 * See: http://gears.googlecode.com/svn/trunk/gears/geolocation/
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

#include "nsCOMPtr.h"
#include "nsComponentManagerUtils.h"
#include "nsServiceManagerUtils.h"
#include "nsThreadUtils.h"
#include "nsXPCOM.h"
#include "nsXPCOMCID.h"
#include "nsIObserver.h"
#include "nsIObserverService.h"
#include "nsWifiMonitor.h"

#include "nsIProxyObjectManager.h"
#include "nsServiceManagerUtils.h"
#include "nsComponentManagerUtils.h"
#include "mozilla/Services.h"

using namespace mozilla;

#if defined(PR_LOGGING)
PRLogModuleInfo *gWifiMonitorLog;
#endif

NS_IMPL_THREADSAFE_ISUPPORTS3(nsWifiMonitor,
                              nsIRunnable,
                              nsIObserver,
                              nsIWifiMonitor)

nsWifiMonitor::nsWifiMonitor()
: mKeepGoing(PR_TRUE)
, mReentrantMonitor("nsWifiMonitor.mReentrantMonitor")
{
#if defined(PR_LOGGING)
  gWifiMonitorLog = PR_NewLogModule("WifiMonitor");
#endif

  nsCOMPtr<nsIObserverService> obsSvc = mozilla::services::GetObserverService();
  if (obsSvc)
    obsSvc->AddObserver(this, "xpcom-shutdown", PR_FALSE);

  LOG(("@@@@@ wifimonitor created\n"));
}

nsWifiMonitor::~nsWifiMonitor()
{
}

NS_IMETHODIMP
nsWifiMonitor::Observe(nsISupports *subject, const char *topic,
                     const PRUnichar *data)
{
  if (!strcmp(topic, "xpcom-shutdown")) {
    LOG(("Shutting down\n"));
    mKeepGoing = PR_FALSE;

    ReentrantMonitorAutoEnter mon(mReentrantMonitor);
    mon.Notify();
  }
  return NS_OK;
}


NS_IMETHODIMP nsWifiMonitor::StartWatching(nsIWifiListener *aListener)
{
  if (!aListener)
    return NS_ERROR_NULL_POINTER;

  nsresult rv = NS_OK;
  if (!mThread) {
    rv = NS_NewThread(getter_AddRefs(mThread), this);
    if (NS_FAILED(rv))
      return rv;
  }

  ReentrantMonitorAutoEnter mon(mReentrantMonitor);

  mKeepGoing = PR_TRUE;

  mListeners.AppendElement(nsWifiListener(aListener));

  // tell ourselves that we have a new watcher.
  mon.Notify();
  return NS_OK;
}

NS_IMETHODIMP nsWifiMonitor::StopWatching(nsIWifiListener *aListener)
{
  if (!aListener)
    return NS_ERROR_NULL_POINTER;

  LOG(("removing listener\n"));

  ReentrantMonitorAutoEnter mon(mReentrantMonitor);

  for (PRUint32 i = 0; i < mListeners.Length(); i++) {

    if (mListeners[i].mListener == aListener) {
      mListeners.RemoveElementAt(i);
      break;
    }
  }

  if (mListeners.Length() == 0) {
    mKeepGoing = PR_FALSE;
    mon.Notify();
    mThread = nsnull;
  }

  return NS_OK;
}


NS_IMETHODIMP nsWifiMonitor::Run()
{
  LOG(("@@@@@ wifi monitor run called\n"));

  nsresult rv = DoScan();

  if (mKeepGoing && NS_FAILED(rv)) {

    nsCOMPtr<nsIProxyObjectManager> proxyObjMgr = do_GetService("@mozilla.org/xpcomproxy;1");

    // send error
    for (PRUint32 i = 0; i < mListeners.Length(); i++) {
      LOG(("About to send error to a listener\n"));

      nsCOMPtr<nsIWifiListener> proxy;
      proxyObjMgr->GetProxyForObject(NS_PROXY_TO_MAIN_THREAD,
                                     NS_GET_IID(nsIWifiListener),
                                     mListeners[i].mListener,
                                     NS_PROXY_SYNC | NS_PROXY_ALWAYS,
                                     getter_AddRefs(proxy));

      if (proxy) {
        proxy->OnError(rv);
        LOG( ("... sent %d\n", rv));
      }
    }
  }

  return NS_OK;
}

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
 *  Ginn Chen <ginn.chen@sun.com>
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

#include "nsWifiMonitor.h"
#include "nsWifiAccessPoint.h"

#include "nsIProxyObjectManager.h"
#include "nsServiceManagerUtils.h"
#include "nsComponentManagerUtils.h"
#include "nsIMutableArray.h"

#include "plstr.h"
#include <glib.h>

#define DLADM_STRSIZE 256
#define DLADM_SECTIONS 3

using namespace mozilla;

struct val_strength_t {
  const char *strength_name;
  int         signal_value;
};

static val_strength_t strength_vals[] = {
  { "very weak", -112 },
  { "weak",      -88 },
  { "good",      -68 },
  { "very good", -40 },
  { "excellent", -16 }
};

static nsWifiAccessPoint *
do_parse_str(char *bssid_str, char *essid_str, char *strength)
{
  unsigned char mac_as_int[6] = { 0 };
  sscanf(bssid_str, "%x:%x:%x:%x:%x:%x", &mac_as_int[0], &mac_as_int[1],
         &mac_as_int[2], &mac_as_int[3], &mac_as_int[4], &mac_as_int[5]);

  int signal = 0;
  PRUint32 strength_vals_count = sizeof(strength_vals) / sizeof (val_strength_t);
  for (PRUint32 i = 0; i < strength_vals_count; i++) {
    if (!strncasecmp(strength, strength_vals[i].strength_name, DLADM_STRSIZE)) {
      signal = strength_vals[i].signal_value;
      break;
    }
  }

  nsWifiAccessPoint *ap = new nsWifiAccessPoint();
  if (ap) {
    ap->setMac(mac_as_int);
    ap->setSignal(signal);
    ap->setSSID(essid_str, PL_strnlen(essid_str, DLADM_STRSIZE));
  }
  return ap;
}

static void
do_dladm(nsCOMArray<nsWifiAccessPoint> &accessPoints)
{
  GError *err = NULL;
  char *sout = NULL;
  char *serr = NULL;
  int exit_status = 0;
  char * dladm_args[] = { "/usr/bin/pfexec", "/usr/sbin/dladm",
                          "scan-wifi", "-p", "-o", "BSSID,ESSID,STRENGTH" };

  gboolean rv = g_spawn_sync("/", dladm_args, NULL, (GSpawnFlags)0, NULL, NULL,
                             &sout, &serr, &exit_status, &err);
  if (rv && !exit_status) {
    char wlan[DLADM_SECTIONS][DLADM_STRSIZE+1];
    PRUint32 section = 0;
    PRUint32 sout_scan = 0;
    PRUint32 wlan_put = 0;
    PRBool escape = PR_FALSE;
    nsWifiAccessPoint* ap;
    char sout_char;
    do {
      sout_char = sout[sout_scan++];
      if (escape) {
        escape = PR_FALSE;
        if (sout_char != '\0') {
          wlan[section][wlan_put++] = sout_char;
          continue;
        }
      }

      if (sout_char =='\\') {
        escape = PR_TRUE;
        continue;
      }

      if (sout_char == ':') {
        wlan[section][wlan_put] = '\0';
        section++;
        wlan_put = 0;
        continue;
      }

      if ((sout_char == '\0') || (sout_char == '\n')) {
        wlan[section][wlan_put] = '\0';
        if (section == DLADM_SECTIONS - 1) {
          ap = do_parse_str(wlan[0], wlan[1], wlan[2]);
          if (ap) {
            accessPoints.AppendObject(ap);
          }
        }
        section = 0;
        wlan_put = 0;
        continue;
      }

      wlan[section][wlan_put++] = sout_char;

    } while ((wlan_put <= DLADM_STRSIZE) && (section < DLADM_SECTIONS) &&
             (sout_char != '\0'));
  }

  g_free(sout);
  g_free(serr);
}

nsresult
nsWifiMonitor::DoScan()
{
  // Regularly get the access point data.

  nsCOMArray<nsWifiAccessPoint> lastAccessPoints;
  nsCOMArray<nsWifiAccessPoint> accessPoints;

  while (mKeepGoing) {

    accessPoints.Clear();
    do_dladm(accessPoints);

    PRBool accessPointsChanged = !AccessPointsEqual(accessPoints, lastAccessPoints);
    nsCOMArray<nsIWifiListener> currentListeners;

    {
      MonitorAutoEnter mon(mMonitor);

      for (PRUint32 i = 0; i < mListeners.Length(); i++) {
        if (!mListeners[i].mHasSentData || accessPointsChanged) {
          mListeners[i].mHasSentData = PR_TRUE;
          currentListeners.AppendObject(mListeners[i].mListener);
        }
      }
    }

    ReplaceArray(lastAccessPoints, accessPoints);

    if (currentListeners.Count() > 0)
    {
      PRUint32 resultCount = lastAccessPoints.Count();
      nsIWifiAccessPoint** result = static_cast<nsIWifiAccessPoint**> (nsMemory::Alloc(sizeof(nsIWifiAccessPoint*) * resultCount));
      if (!result) {
        return NS_ERROR_OUT_OF_MEMORY;
      }

      for (PRUint32 i = 0; i < resultCount; i++)
        result[i] = lastAccessPoints[i];

      for (PRInt32 i = 0; i < currentListeners.Count(); i++) {

        LOG(("About to send data to the wifi listeners\n"));

        nsCOMPtr<nsIWifiListener> proxy;
        nsCOMPtr<nsIProxyObjectManager> proxyObjMgr = do_GetService("@mozilla.org/xpcomproxy;1");
        proxyObjMgr->GetProxyForObject(NS_PROXY_TO_MAIN_THREAD,
                                       NS_GET_IID(nsIWifiListener),
                                       currentListeners[i],
                                       NS_PROXY_SYNC | NS_PROXY_ALWAYS,
                                       getter_AddRefs(proxy));
        if (!proxy) {
          LOG(("There is no proxy available.  this should never happen\n"));
        }
        else
        {
          nsresult rv = proxy->OnChange(result, resultCount);
          LOG( ("... sent %d\n", rv));
        }
      }

      nsMemory::Free(result);
    }

    LOG(("waiting on monitor\n"));

    MonitorAutoEnter mon(mMonitor);
    mon.Wait(PR_SecondsToInterval(60));
  }

  return NS_OK;
}

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "iwlib.h"

#include "dlfcn.h"

#include "nsWifiMonitor.h"
#include "nsWifiAccessPoint.h"

#include "nsServiceManagerUtils.h"
#include "nsComponentManagerUtils.h"
#include "nsIMutableArray.h"
#include "nsThreadUtils.h"

using namespace mozilla;


typedef int (*iw_open_t)(void);

typedef void (*iw_enum_t)(int	skfd,
			  iw_enum_handler fn,
			  char *args[],
			  int count);

typedef  int (*iw_stats_t)(int skfd,
			   const char *ifname,
			   iwstats *stats,
			   const iwrange *range,
			   int has_range);

static int scan_wifi(int skfd, char* ifname, char* args[], int count)
{
  nsCOMArray<nsWifiAccessPoint>* accessPoints = (nsCOMArray<nsWifiAccessPoint>*) args[0];
  iw_stats_t iw_stats = (iw_stats_t) args[1];

  struct iwreq wrq;

  int result = iw_get_ext(skfd, ifname, SIOCGIWMODE, &wrq);
  if (result < 0)
    return 0;

  // We only cared about "Managed" mode.  2 is the magic number.  There isn't a #define for this.
  if (wrq.u.mode != 2)
    return 0;

  nsWifiAccessPoint* ap = new nsWifiAccessPoint();
  if (!ap)
    return 0;

  char buffer[128];
  wrq.u.essid.pointer = buffer;
  wrq.u.essid.length  = 128;
  wrq.u.essid.flags   = 0;
  result = iw_get_ext(skfd, ifname, SIOCGIWESSID, &wrq);
  if (result < 0) {
    delete ap;
    return 0;
  }

  ap->setSSID(buffer, wrq.u.essid.length);

  result = iw_get_ext(skfd, ifname, SIOCGIWAP, &wrq);
  if (result < 0) {
    delete ap;
    return 0;
  }
  ap->setMac( (const uint8*) wrq.u.ap_addr.sa_data );

  iwrange range;
  iwstats stats;
  result = (*iw_stats)(skfd, ifname, &stats, &range, 1);
  if (result < 0) {
    delete ap;
    return 0;
  }

  if(stats.qual.level > range.max_qual.level)
    ap->setSignal(stats.qual.level - 0x100);
  else
    ap->setSignal(0);

  accessPoints->AppendObject(ap);
  return 0;
}

nsresult
nsWifiMonitor::DoScan()
{
  static void* iwlib_handle = NULL;

  if (!iwlib_handle) {
    iwlib_handle = dlopen("libiw.so", RTLD_NOW);
    if (!iwlib_handle) {
      iwlib_handle = dlopen("libiw.so.29", RTLD_NOW);
      if (!iwlib_handle) {
        iwlib_handle = dlopen("libiw.so.30", RTLD_NOW);
        if (!iwlib_handle) {
          LOG(("Could not load libiw\n"));
          return NS_ERROR_NOT_AVAILABLE;
        }
      }
    }
  }
  else {
    LOG(("Loaded libiw\n"));
  }

  static iw_open_t iw_open = NULL;
  if (!iw_open)
    iw_open = (iw_open_t) dlsym(iwlib_handle, "iw_sockets_open");

  static iw_enum_t iw_enum = NULL;
  if (!iw_enum)
    iw_enum = (iw_enum_t) dlsym(iwlib_handle, "iw_enum_devices");

  static iw_stats_t iw_stats = NULL;
  if (!iw_stats)
    iw_stats = (iw_stats_t)dlsym(iwlib_handle, "iw_get_stats");

  if (!iw_open || !iw_enum || !iw_stats) {
    LOG(("Could not load a symbol from iwlib.so\n"));
    return NS_ERROR_FAILURE;
  }

  int skfd = (*iw_open)();

  if (skfd < 0) {
    LOG(("Could not iw_open\n"));
    return NS_ERROR_FAILURE;
  }

  struct SocketsGuard {
    int skfd;
    SocketsGuard(int skfd) : skfd(skfd) {}
    ~SocketsGuard() { iw_sockets_close(skfd); }
  } guard(skfd);

  nsCOMArray<nsWifiAccessPoint> lastAccessPoints;
  nsCOMArray<nsWifiAccessPoint> accessPoints;

  char* args[] = {(char*) &accessPoints, (char*) iw_stats, nsnull };

  while (mKeepGoing) {
    accessPoints.Clear();

    (*iw_enum)(skfd, &scan_wifi, args, 1);

    bool accessPointsChanged = !AccessPointsEqual(accessPoints, lastAccessPoints);
    ReplaceArray(lastAccessPoints, accessPoints);

    nsresult rv = CallWifiListeners(lastAccessPoints, accessPointsChanged);
    NS_ENSURE_SUCCESS(rv, rv);

    LOG(("waiting on monitor\n"));

    ReentrantMonitorAutoEnter mon(mReentrantMonitor);
    mon.Wait(PR_SecondsToInterval(60));
  }

  return NS_OK;
}

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsWifiAccessPoint.h"
#include "nsString.h"
#include "nsMemory.h"
#include "prlog.h"

#if defined(PR_LOGGING)
extern PRLogModuleInfo *gWifiMonitorLog;
#endif
#define LOG(args)     PR_LOG(gWifiMonitorLog, PR_LOG_DEBUG, args)


NS_IMPL_ISUPPORTS1(nsWifiAccessPoint, nsIWifiAccessPoint)

nsWifiAccessPoint::nsWifiAccessPoint()
{
  // make sure these are null terminated (because we are paranoid)
  mMac[0] = '\0';
  mSsid[0] = '\0';
  mSsidLen = 0;
}

nsWifiAccessPoint::~nsWifiAccessPoint()
{
}

NS_IMETHODIMP nsWifiAccessPoint::GetMac(nsACString& aMac)
{
  aMac.Assign(mMac);
  return NS_OK;
}

NS_IMETHODIMP nsWifiAccessPoint::GetSsid(nsAString& aSsid)
{
  // just assign and embedded nulls will truncate resulting
  // in a displayable string.
  CopyASCIItoUTF16(mSsid, aSsid);
  return NS_OK;
}


NS_IMETHODIMP nsWifiAccessPoint::GetRawSSID(nsACString& aRawSsid)
{
  aRawSsid.Assign(mSsid, mSsidLen); // SSIDs are 32 chars long
  return NS_OK;
}

NS_IMETHODIMP nsWifiAccessPoint::GetSignal(int32_t *aSignal)
{
  NS_ENSURE_ARG(aSignal);
  *aSignal = mSignal;
  return NS_OK;
}

// Helper functions:

bool AccessPointsEqual(nsCOMArray<nsWifiAccessPoint>& a, nsCOMArray<nsWifiAccessPoint>& b)
{
  if (a.Count() != b.Count()) {
    LOG(("AccessPoint lists have different lengths\n"));
    return false;
  }

  for (int32_t i = 0; i < a.Count(); i++) {
    LOG(("++ Looking for %s\n", a[i]->mSsid));
    bool found = false;
    for (int32_t j = 0; j < b.Count(); j++) {
      LOG(("   %s->%s | %s->%s\n", a[i]->mSsid, b[j]->mSsid, a[i]->mMac, b[j]->mMac));
      if (!strcmp(a[i]->mSsid, b[j]->mSsid) &&
          !strcmp(a[i]->mMac, b[j]->mMac)) {
        found = true;
      }
    }
    if (!found)
      return false;
  }
  LOG(("   match!\n"));
  return true;
}

void ReplaceArray(nsCOMArray<nsWifiAccessPoint>& a, nsCOMArray<nsWifiAccessPoint>& b)
{
  a.Clear();

  // better way to copy?
  for (int32_t i = 0; i < b.Count(); i++) {
    a.AppendObject(b[i]);
  }

  b.Clear();
}



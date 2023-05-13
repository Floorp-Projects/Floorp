/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsWifiAccessPoint.h"
#include "nsString.h"
#include "mozilla/Logging.h"

extern mozilla::LazyLogModule gWifiMonitorLog;
#define LOG(args) MOZ_LOG(gWifiMonitorLog, mozilla::LogLevel::Debug, args)

NS_IMPL_ISUPPORTS(nsWifiAccessPoint, nsIWifiAccessPoint)

nsWifiAccessPoint::nsWifiAccessPoint() {
  // make sure these are null terminated (because we are paranoid)
  mMac[0] = '\0';
  mSsid[0] = '\0';
  mSsidLen = 0;
  mSignal = -1000;
}

NS_IMETHODIMP nsWifiAccessPoint::GetMac(nsACString& aMac) {
  aMac.Assign(mMac);
  return NS_OK;
}

NS_IMETHODIMP nsWifiAccessPoint::GetSsid(nsAString& aSsid) {
  // just assign and embedded nulls will truncate resulting
  // in a displayable string.
  aSsid.AssignASCII(mSsid);
  return NS_OK;
}

NS_IMETHODIMP nsWifiAccessPoint::GetRawSSID(nsACString& aRawSsid) {
  aRawSsid.Assign(mSsid, mSsidLen);  // SSIDs are 32 chars long
  return NS_OK;
}

NS_IMETHODIMP nsWifiAccessPoint::GetSignal(int32_t* aSignal) {
  NS_ENSURE_ARG(aSignal);
  *aSignal = mSignal;
  return NS_OK;
}

int nsWifiAccessPoint::Compare(const nsWifiAccessPoint& o) const {
  int ret = strcmp(mMac, o.mMac);
  if (ret) {
    return ret;
  }
  if (mSsidLen != o.mSsidLen) {
    return (mSsidLen < o.mSsidLen) ? -1 : 1;
  }
  ret = strncmp(mSsid, o.mSsid, mSsidLen);
  if (ret) {
    return ret;
  }
  if (mSignal == o.mSignal) {
    return 0;
  }
  return (mSignal < o.mSignal) ? -1 : 1;
}

bool nsWifiAccessPoint::operator==(const nsWifiAccessPoint& o) const {
  LOG(("nsWifiAccessPoint comparing %s->%s | %s->%s | %d -> %d\n", mSsid,
       o.mSsid, mMac, o.mMac, mSignal, o.mSignal));
  return Compare(o) == 0;
}

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsWifiMonitor.h"
#include "nsIWifiAccessPoint.h"

#include "nsString.h"
#include "nsCOMArray.h"
#include "mozilla/Attributes.h"

#ifndef __nsWifiAccessPoint__
#define __nsWifiAccessPoint__

class nsWifiAccessPoint MOZ_FINAL : public nsIWifiAccessPoint
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIWIFIACCESSPOINT

  nsWifiAccessPoint();
  ~nsWifiAccessPoint();

  char mMac[18];
  int  mSignal;
  char mSsid[33];
  int  mSsidLen;

  void setSignal(int signal)
  {
    mSignal = signal;
  }

  void setMac(const unsigned char mac_as_int[6])
  {
    // mac_as_int is big-endian. Write in byte chunks.
    // Format is XX-XX-XX-XX-XX-XX.

    const unsigned char holder[6] = {0};
    if (!mac_as_int) {
      mac_as_int = holder;
    }

    static const char *kMacFormatString = ("%02x-%02x-%02x-%02x-%02x-%02x");

    sprintf(mMac, kMacFormatString,
            mac_as_int[0], mac_as_int[1], mac_as_int[2],
            mac_as_int[3], mac_as_int[4], mac_as_int[5]);

    mMac[17] = 0;
  }

  void setSSID(const char* aSSID, unsigned long len) {
    if (aSSID && (len < sizeof(mSsid))) {
        strncpy(mSsid, aSSID, len);
        mSsid[len] = 0;
        mSsidLen = len;
    }
    else
    {
      mSsid[0] = 0;
      mSsidLen = 0;
    }
  }
};



// Helper functions

bool AccessPointsEqual(nsCOMArray<nsWifiAccessPoint>& a, nsCOMArray<nsWifiAccessPoint>& b);
void ReplaceArray(nsCOMArray<nsWifiAccessPoint>& a, nsCOMArray<nsWifiAccessPoint>& b);


#endif

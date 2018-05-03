/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __nsWifiAccessPoint__
#define __nsWifiAccessPoint__

#include <algorithm>
#include "nsWifiMonitor.h"
#include "nsIWifiAccessPoint.h"

#include "nsString.h"
#include "nsCOMArray.h"
#include "mozilla/ArrayUtils.h" // ArrayLength
#include "mozilla/Attributes.h"
#include "mozilla/Sprintf.h"

class nsWifiAccessPoint final : public nsIWifiAccessPoint
{
  ~nsWifiAccessPoint() = default;

public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIWIFIACCESSPOINT

  nsWifiAccessPoint();

  char mMac[18];
  int  mSignal;
  char mSsid[33];
  int  mSsidLen;

  void setSignal(int signal)
  {
    mSignal = signal;
  }

  void setMacRaw(const char* aString)
  {
    memcpy(mMac, aString, mozilla::ArrayLength(mMac));
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

    SprintfLiteral(mMac, kMacFormatString,
                   mac_as_int[0], mac_as_int[1], mac_as_int[2],
                   mac_as_int[3], mac_as_int[4], mac_as_int[5]);

    mMac[17] = 0;
  }

  void setSSIDRaw(const char* aSSID, size_t len) {
    mSsidLen = std::min(len, mozilla::ArrayLength(mSsid));
    memcpy(mSsid, aSSID, mSsidLen);
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

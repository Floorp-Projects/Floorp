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

#include "nsWifiMonitor.h"
#include "nsIWifiAccessPoint.h"

#include "nsString.h"
#include "nsCOMArray.h"

#ifndef __nsWifiAccessPoint__
#define __nsWifiAccessPoint__

class nsWifiAccessPoint : public nsIWifiAccessPoint
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
  };

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
  };

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
  };
};



// Helper functions

bool AccessPointsEqual(nsCOMArray<nsWifiAccessPoint>& a, nsCOMArray<nsWifiAccessPoint>& b);
void ReplaceArray(nsCOMArray<nsWifiAccessPoint>& a, nsCOMArray<nsWifiAccessPoint>& b);


#endif

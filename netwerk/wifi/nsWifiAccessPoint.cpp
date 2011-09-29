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
 * Portions created by the Initial Developer are Copyright (C) 2009
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

#include "nsWifiAccessPoint.h"
#include "nsString.h"
#include "nsMemory.h"
#include "prlog.h"

#if defined(PR_LOGGING)
extern PRLogModuleInfo *gWifiMonitorLog;
#endif
#define LOG(args)     PR_LOG(gWifiMonitorLog, PR_LOG_DEBUG, args)


NS_IMPL_THREADSAFE_ISUPPORTS1(nsWifiAccessPoint, nsIWifiAccessPoint)

nsWifiAccessPoint::nsWifiAccessPoint()
{
  // make sure these are null terminated (because we are paranoid)
  mMac[0] = nsnull;
  mSsid[0] = nsnull;
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

NS_IMETHODIMP nsWifiAccessPoint::GetSignal(PRInt32 *aSignal)
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
    return PR_FALSE;
  }

  for (PRInt32 i = 0; i < a.Count(); i++) {
    LOG(("++ Looking for %s\n", a[i]->mSsid));
    bool found = false;
    for (PRInt32 j = 0; j < b.Count(); j++) {
      LOG(("   %s->%s | %s->%s\n", a[i]->mSsid, b[j]->mSsid, a[i]->mMac, b[j]->mMac));
      if (!strcmp(a[i]->mSsid, b[j]->mSsid) &&
          !strcmp(a[i]->mMac, b[j]->mMac)) {
        found = PR_TRUE;
      }
    }
    if (!found)
      return PR_FALSE;
  }
  LOG(("   match!\n"));
  return PR_TRUE;
}

void ReplaceArray(nsCOMArray<nsWifiAccessPoint>& a, nsCOMArray<nsWifiAccessPoint>& b)
{
  a.Clear();

  // better way to copy?
  for (PRInt32 i = 0; i < b.Count(); i++) {
    a.AppendObject(b[i]);
  }

  b.Clear();
}



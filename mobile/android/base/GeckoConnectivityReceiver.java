/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Mozilla Android code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Makoto Kato <m_kato@ga2.so-net.ne.jp> (Original Author)
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

package org.mozilla.gecko;

import android.app.Activity;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.net.ConnectivityManager;
import android.net.NetworkInfo;

public class GeckoConnectivityReceiver extends BroadcastReceiver {
    /*
     * Keep the below constants in sync with
     * http://mxr.mozilla.org/mozilla-central/source/netwerk/base/public/nsINetworkLinkService.idl
     */
    private static final String LINK_DATA_UP = "up";
    private static final String LINK_DATA_DOWN = "down";
    private static final String LINK_DATA_UNKNOWN = "unknown";

    private IntentFilter mFilter;

    private static boolean isRegistered = false;

    public GeckoConnectivityReceiver() {
        mFilter = new IntentFilter();
        mFilter.addAction(ConnectivityManager.CONNECTIVITY_ACTION);
    }

    @Override
    public void onReceive(Context context, Intent intent) {
        String status;
        ConnectivityManager cm = (ConnectivityManager)
            context.getSystemService(Context.CONNECTIVITY_SERVICE);
        NetworkInfo info = cm.getActiveNetworkInfo();
        if (info == null)
            status = LINK_DATA_UNKNOWN;
        else if (!info.isConnected())
            status = LINK_DATA_DOWN;
        else
            status = LINK_DATA_UP;

        if (GeckoApp.checkLaunchState(GeckoApp.LaunchState.GeckoRunning))
            GeckoAppShell.onChangeNetworkLinkStatus(status);
    }

    public void registerFor(Activity activity) {
        if (!isRegistered) {
            activity.registerReceiver(this, mFilter);
            isRegistered = true;
        }
    }

    public void unregisterFor(Activity activity) {
        if (isRegistered) {
            activity.unregisterReceiver(this);
            isRegistered = false;
        }
    }
}

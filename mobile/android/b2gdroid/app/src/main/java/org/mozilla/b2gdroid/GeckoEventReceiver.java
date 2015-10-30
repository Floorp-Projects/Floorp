/* vim: set ts=4 sw=4 tw=80 et :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.b2gdroid;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;

import android.util.Log;

import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.GeckoEvent;
import org.mozilla.gecko.AppConstants;

/**
 * A generic 'GeckoEvent' Intent receiver.
 *
 * A Context running in a different process to the main Context may wish to
 * send a GeckoEvent.  Using GeckoAppShell.sendEventToGecko from other
 * processes will silently fail.  Instead, broadcast an Intent containing the
 * GeckoEvent data and the GeckoEventReceiver registered with the primary
 * Context will forward the Event to Gecko.
 */
public class GeckoEventReceiver extends BroadcastReceiver {
    private static final String LOGTAG = "B2GDroid:GeckoEventReceiver";

    private static final String SEND_GECKO_EVENT = AppConstants.ANDROID_PACKAGE_NAME + ".SEND_GECKO_EVENT";

    private static final String EXTRA_SUBJECT = "SUBJECT";
    private static final String EXTRA_DATA = "DATA";

    public static Intent createBroadcastEventIntent(String subject, String data) {
        Intent intent = new Intent(SEND_GECKO_EVENT);
        intent.putExtra(EXTRA_SUBJECT, subject);
        intent.putExtra(EXTRA_DATA, data);
        return intent;
    }

    public void registerWithContext(Context aContext) {
        aContext.registerReceiver(this, new IntentFilter(SEND_GECKO_EVENT));
    }

    public void destroy(Context aContext) {
        aContext.unregisterReceiver(this);
    }

    @Override
    public void onReceive(Context aContext, Intent aIntent) {
        String geckoEventSubject = aIntent.getStringExtra(EXTRA_SUBJECT);
        String geckoEventJSON = aIntent.getStringExtra(EXTRA_DATA);

        Log.i(LOGTAG, "onReceive(" + geckoEventSubject + ")");

        GeckoEvent e = GeckoEvent.createBroadcastEvent(geckoEventSubject, geckoEventJSON);
        GeckoAppShell.sendEventToGecko(e);
    }
}

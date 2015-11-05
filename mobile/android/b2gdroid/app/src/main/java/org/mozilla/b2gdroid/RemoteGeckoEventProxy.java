/* vim: set ts=4 sw=4 tw=78 et :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */
package org.mozilla.b2gdroid;

import android.util.Log;

import android.content.Intent;
import android.content.IntentFilter;
import android.content.Context;
import android.content.BroadcastReceiver;

import org.json.JSONObject;

import org.mozilla.gecko.util.GeckoEventListener;
import org.mozilla.gecko.AppConstants;

/**
 * Proxy GeckoEvents from the Java UI Thread to other processes owned by this
 * app - deferring processing to those processes.
 *
 * Usage:
 *
 * // In remote process `context` given GeckoEventListener `listener`
 * RemoteGeckoEventProxy
 *  .registerRemoteGeckoThreadListener(context, listener, "Message:Message");
 *
 * // In UI Thread (Main Activity `context`)
 * RemoteGeckoEventProxy mGeckoEventProxy = new RemoteGeckoEventProxy(context);
 * EventDispatcher
 *  .getInstance()
 *  .registerGeckoThreadListener(mGeckoEventProxy, "Message:Message");
 */
class RemoteGeckoEventProxy implements GeckoEventListener {

    private static final String LOGTAG = "RemoteGeckoEventProxy";

    private static final String RECEIVE_GECKO_EVENT = AppConstants.ANDROID_PACKAGE_NAME + ".RECEIVE_GECKO_EVENT";

    private static final String EXTRA_DATA = "DATA";
    private static final String EXTRA_SUBJECT = "SUBJECT";

    private Context mSenderContext;

    public static void registerRemoteGeckoThreadListener(Context aContext, GeckoEventListener mListener, String... aMessages) {
        for (String message : aMessages) {
            IntentFilter intentFilter = new IntentFilter(RECEIVE_GECKO_EVENT + "." + message);
            aContext.registerReceiver(new GeckoEventProxyReceiver(mListener), intentFilter);
        }
    }

    public RemoteGeckoEventProxy(Context aSenderContext) {
        mSenderContext = aSenderContext;
    }

    public void handleMessage(String aEvent, JSONObject aMessage) {
        Intent broadcastIntent = createBroadcastIntent(aEvent, aMessage.toString());
        mSenderContext.sendBroadcast(broadcastIntent);
    }

    private static Intent createBroadcastIntent(String aEvent, String aMessage) {
        Intent intent = new Intent(RECEIVE_GECKO_EVENT + "." + aEvent);
        intent.putExtra(EXTRA_SUBJECT, aEvent);
        intent.putExtra(EXTRA_DATA, aMessage);
        return intent;
    }

    static class GeckoEventProxyReceiver extends BroadcastReceiver {

        private GeckoEventListener mGeckoEventListener;

        public GeckoEventProxyReceiver(GeckoEventListener aEventListener) {
            mGeckoEventListener = aEventListener;
        }

        @Override
        public void onReceive(Context aContext, Intent aIntent) {
            String geckoEventSubject = aIntent.getStringExtra(EXTRA_SUBJECT);
            String geckoEventJSON = aIntent.getStringExtra(EXTRA_DATA);

            // Unwrap the message data and invoke the GeckoEventListener
            JSONObject obj;
            try {
                obj = new JSONObject(geckoEventJSON);
                obj = obj.getJSONObject("value");
            } catch(Exception ex) {
                Log.wtf(RemoteGeckoEventProxy.LOGTAG, "Could not decode JSON payload for message: " + geckoEventSubject);
                return;
            }

            mGeckoEventListener.handleMessage(geckoEventSubject, obj);
        }
    }
}

/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.distribution;

import org.json.JSONException;
import org.json.JSONObject;
import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.GeckoEvent;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.text.TextUtils;
import android.util.Log;

public class ReferrerReceiver extends BroadcastReceiver {
    private static final String LOGTAG = "GeckoReferrerReceiver";

    private static final String ACTION_INSTALL_REFERRER = "com.android.vending.INSTALL_REFERRER";

    /**
     * If the install intent has this source, we'll track the campaign ID.
     */
    private static final String MOZILLA_UTM_SOURCE = "mozilla";

    /**
     * If the install intent has this campaign, we'll load the specified distribution.
     */
    private static final String DISTRIBUTION_UTM_CAMPAIGN = "distribution";

    @Override
    public void onReceive(Context context, Intent intent) {
        Log.v(LOGTAG, "Received intent " + intent);
        if (!ACTION_INSTALL_REFERRER.equals(intent.getAction())) {
            // This should never happen.
            return;
        }

        ReferrerDescriptor referrer = new ReferrerDescriptor(intent.getStringExtra("referrer"));

        // Track the referrer object for distribution handling.
        if (TextUtils.equals(referrer.campaign, DISTRIBUTION_UTM_CAMPAIGN)) {
            Distribution.onReceivedReferrer(referrer);
        } else {
            Log.d(LOGTAG, "Not downloading distribution: non-matching campaign.");
        }

        // If this is a Mozilla campaign, pass the campaign along to Gecko.
        if (TextUtils.equals(referrer.source, MOZILLA_UTM_SOURCE)) {
            propagateMozillaCampaign(referrer);
        }
    }


    private void propagateMozillaCampaign(ReferrerDescriptor referrer) {
        if (referrer.campaign == null) {
            return;
        }

        try {
            final JSONObject data = new JSONObject();
            data.put("id", "playstore");
            data.put("version", referrer.campaign);
            String payload = data.toString();

            // Try to make sure the prefs are written as a group.
            final GeckoEvent event = GeckoEvent.createBroadcastEvent("Campaign:Set", payload);
            GeckoAppShell.sendEventToGecko(event);
        } catch (JSONException e) {
            Log.e(LOGTAG, "Error propagating campaign identifier.", e);
        }
    }
}

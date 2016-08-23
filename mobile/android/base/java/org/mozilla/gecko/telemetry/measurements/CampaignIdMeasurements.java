/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, you can obtain one at http://mozilla.org/MPL/2.0/.
 */

package org.mozilla.gecko.telemetry.measurements;

import android.content.Context;
import android.support.annotation.NonNull;
import android.text.TextUtils;

import org.mozilla.gecko.GeckoSharedPrefs;
import org.mozilla.gecko.adjust.AttributionHelperListener;

/**
 * A class to retrieve and store the campaign Id pref that is used when the Adjust SDK gives us
 * new attribution from the {@link AttributionHelperListener}.
 */
public class CampaignIdMeasurements {
    private static final String PREF_CAMPAIGN_ID = "measurements-campaignId";

    public static String getCampaignIdFromPrefs(@NonNull final Context context) {
        return GeckoSharedPrefs.forProfile(context)
                .getString(PREF_CAMPAIGN_ID, null);
    }

    public static void updateCampaignIdPref(@NonNull final Context context, @NonNull final String campaignId) {
        if (TextUtils.isEmpty(campaignId)) {
            return;
        }
        GeckoSharedPrefs.forProfile(context)
                .edit()
                .putString(PREF_CAMPAIGN_ID, campaignId)
                .apply();
    }
}

/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, you can obtain one at http://mozilla.org/MPL/2.0/.
 */

package org.mozilla.gecko.distribution;

import android.content.Context;
import android.content.SharedPreferences;
import android.util.Log;
import org.mozilla.gecko.GeckoSharedPrefs;

import java.lang.ref.WeakReference;

/**
 * A distribution ready callback that will store the distribution ID to profile-specific shared preferences.
 */
public class DistributionStoreCallback implements Distribution.ReadyCallback {
    private static final String LOGTAG = "Gecko" + DistributionStoreCallback.class.getSimpleName();

    public static final String PREF_DISTRIBUTION_ID = "distribution.id";

    private final WeakReference<Context> contextReference;
    private final String profileName;

    public DistributionStoreCallback(final Context context, final String profileName) {
        this.contextReference = new WeakReference<>(context);
        this.profileName = profileName;
    }

    public void distributionNotFound() { /* nothing to do here */ }

    @Override
    public void distributionFound(final Distribution distribution) {
        storeDistribution(distribution);
    }

    @Override
    public void distributionArrivedLate(final Distribution distribution) {
        storeDistribution(distribution);
    }

    private void storeDistribution(final Distribution distribution) {
        final Context context = contextReference.get();
        if (context == null) {
            Log.w(LOGTAG, "Context is no longer alive, could retrieve shared prefs to store distribution");
            return;
        }

        // While the distribution preferences are per install and not per profile, it's okay to use the
        // profile-specific prefs because:
        //   1) We don't really support mulitple profiles for end-users
        //   2) The TelemetryUploadService already accesses profile-specific shared prefs so this keeps things simple.
        final SharedPreferences sharedPrefs = GeckoSharedPrefs.forProfileName(context, profileName);
        final Distribution.DistributionDescriptor desc = distribution.getDescriptor();
        if (desc != null) {
            sharedPrefs.edit().putString(PREF_DISTRIBUTION_ID, desc.id).apply();
        }
    }
}
